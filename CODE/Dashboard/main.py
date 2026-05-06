"""
main.py - Camera + ultrasonic threads with YOLOv5 inference for OSI-2026.

Pipeline per camera:
  serial JPEG  ->  cv2.imdecode  ->  flip  ->  YOLOv5  ->
  draw boxes   ->  cv2.imencode   ->  shared state  ->  /video/<id> MJPEG

Auto-detects USB devices on the hub:
  Arduino Uno     -> VID:PID 9025:67    (CSV "d1,d2,d3,d4,d5\\n", 115200)
  OpenMV H7 cams  -> VID:PID 14277:4682 (b'FRAME' + callsign + size + jpeg)

Run via run_all.py.
"""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import threading
import time
import serial
import numpy as np
import cv2
from serial.tools import list_ports

from state_store import state, state_lock

# ── Config ────────────────────────────────────────────────────────────────────
FULL_DISTANCE_MM = 75
MAX_DISTANCE_MM  = 170
NUM_BINS         = 5
NUM_CAMERAS      = 2

ARDUINO_VID_PID  = (9025, 67)
OPENMV_VID_PID   = (14277, 4682)
MAGIC            = b'FRAME'
MAX_FRAME_BYTES  = 200_000
BAUD             = 115200

# YOLO config
YOLO_PATH        = '/home/nvidia/yolov5'
YOLO_MODEL       = '/home/nvidia/yolov5/yolov5s.pt'   # swap with the waste model when ready
CONF_THRES       = 0.25
IOU_THRES        = 0.45
INFERENCE_SIZE   = 640
JPEG_QUALITY     = 80      # output JPEG quality for the browser stream

# Inference rate limit: run YOLO every N frames, skip the rest.
# 1 = every frame. Bump to 2 or 3 if frame rate drops too much on the TX2.
INFER_EVERY_N    = 1

# ── Try to load YOLOv5 (optional: dashboard runs without it) ─────────────────
MODEL  = None
DEVICE = None
NAMES  = []
try:
    import torch
    sys.path.insert(0, YOLO_PATH)
    from models.experimental import attempt_load
    from utils.general import non_max_suppression, scale_coords
    from utils.torch_utils import select_device

    DEVICE = select_device('')
    MODEL  = attempt_load(YOLO_MODEL, map_location=DEVICE)
    MODEL.eval()
    NAMES  = MODEL.module.names if hasattr(MODEL, 'module') else MODEL.names
    print(f"[yolo] loaded {YOLO_MODEL} on {DEVICE}, {len(NAMES)} classes")
except Exception as e:
    print(f"[yolo] disabled: {e}")
    MODEL = None

# ── Stop events ──────────────────────────────────────────────────────────────
stop_events = {
    "camera_0":   threading.Event(),
    "camera_1":   threading.Event(),
    "ultrasonic": threading.Event(),
}
_threads = {}


# ─────────────────────────────────────────────────────────────────────────────
# USB device discovery
# ─────────────────────────────────────────────────────────────────────────────
def find_devices():
    arduinos, cams = [], []
    for p in list_ports.comports():
        if (p.vid, p.pid) == ARDUINO_VID_PID:
            arduinos.append(p)
        elif (p.vid, p.pid) == OPENMV_VID_PID:
            cams.append(p)
    arduinos.sort(key=lambda p: p.serial_number or "")
    cams.sort(key=lambda p: p.serial_number or "")
    arduino_port = arduinos[0].device if arduinos else None
    cam_ports    = {i: cams[i].device for i in range(min(NUM_CAMERAS, len(cams)))}
    return arduino_port, cam_ports


# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────
def read_exact(ser, n, stop_event):
    buf = b''
    while len(buf) < n:
        if stop_event.is_set():
            return None
        chunk = ser.read(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def fullness_from_mm(distance_mm):
    d = max(0, min(distance_mm, MAX_DISTANCE_MM))
    if d <= FULL_DISTANCE_MM:
        return 100.0, True
    pct = (1 - (d - FULL_DISTANCE_MM) / (MAX_DISTANCE_MM - FULL_DISTANCE_MM)) * 100
    return round(max(0.0, min(100.0, pct)), 1), False


# ─────────────────────────────────────────────────────────────────────────────
# YOLOv5 inference + bounding box drawing
# ─────────────────────────────────────────────────────────────────────────────
def run_inference(frame, callsign):
    """
    Mutates `frame` in place: draws boxes, label strings, centre dots,
    and `(cx,cy)` pixel coordinates on each detection.

    Returns (frame, best_label, detections) where detections is a list of:
        {"label": str, "conf": float, "bbox": [x1,y1,x2,y2], "cx": int, "cy": int}
    `best_label` is "<class> <conf>% @ (cx,cy)" for the highest-conf det, or None.
    """
    if MODEL is None:
        return frame, None, []

    # Preprocess: BGR HWC uint8  ->  RGB CHW float [0,1] batch=1
    img = cv2.resize(frame, (INFERENCE_SIZE, INFERENCE_SIZE))
    img = img[:, :, ::-1].transpose(2, 0, 1)
    img = np.ascontiguousarray(img)
    img = torch.from_numpy(img).to(DEVICE).float() / 255.0
    img = img.unsqueeze(0)

    with torch.no_grad():
        pred = MODEL(img)[0]
        pred = non_max_suppression(pred, conf_thres=CONF_THRES, iou_thres=IOU_THRES)

    detections = []
    best_label = None
    best_conf  = 0.0
    timestamp  = time.strftime('%H:%M:%S')

    for det in pred:
        if not len(det):
            continue
        # Map boxes from 640x640 inference space back to original frame size
        det[:, :4] = scale_coords(img.shape[2:], det[:, :4], frame.shape).round()

        for *xyxy, conf, cls in det:
            x1, y1, x2, y2 = map(int, xyxy)
            cx, cy         = (x1 + x2) // 2, (y1 + y2) // 2
            cls_idx        = int(cls)
            confidence     = float(conf)
            object_name    = NAMES[cls_idx] if cls_idx < len(NAMES) else f"cls_{cls_idx}"

            info_string  = (f'{callsign} | {object_name} | '
                            f'{confidence*100:.1f}% | {timestamp}')
            coord_string = f'({cx},{cy})'

            # Bounding box
            cv2.rectangle(frame, (x1, y1), (x2, y2), (200, 200, 200), 2)
            # Label above the box
            cv2.putText(frame, info_string, (x1, max(y1 - 8, 12)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (60, 40, 90), 2)
            # Centre-point dot + pixel-coordinate text next to it
            cv2.circle(frame, (cx, cy), 4, (0, 255, 255), -1)
            cv2.putText(frame, coord_string, (cx + 6, cy + 4),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 255, 255), 1)

            detections.append({
                "label": object_name,
                "conf":  round(confidence, 3),
                "bbox":  [x1, y1, x2, y2],
                "cx":    cx,
                "cy":    cy,
            })

            if confidence > best_conf:
                best_conf  = confidence
                best_label = (f"{object_name} {confidence*100:.1f}% "
                              f"@ ({cx},{cy})")

    return frame, best_label, detections


# ─────────────────────────────────────────────────────────────────────────────
# CAMERA THREAD
# ─────────────────────────────────────────────────────────────────────────────
def camera_thread(cam_id, stop_event):
    key      = f"camera_{cam_id}"
    callsign = f"CAM{cam_id + 1}"   # CAM1 / CAM2 to match the OpenMV-side script
    print(f"[{key}] thread started ({callsign})")
    frame_count = 0

    while not stop_event.is_set():
        _, cam_ports = find_devices()
        port = cam_ports.get(cam_id)
        if not port:
            with state_lock:
                state["cameras"][cam_id]["running"] = False
            time.sleep(2)
            continue

        try:
            with serial.Serial(port, BAUD, timeout=2) as ser:
                print(f"[{key}] connected on {port}")
                with state_lock:
                    state["cameras"][cam_id]["running"] = True
                    state["cameras"][cam_id]["log"].append(f"connected on {port}")
                    state["cameras"][cam_id]["log"] = state["cameras"][cam_id]["log"][-50:]

                while not stop_event.is_set():
                    # Sync to MAGIC
                    header = b''
                    while header != MAGIC:
                        if stop_event.is_set():
                            break
                        b = ser.read(1)
                        if not b:
                            continue
                        header = (header + b)[-len(MAGIC):]
                    if stop_event.is_set():
                        break

                    cs_bytes = read_exact(ser, 4, stop_event)
                    if cs_bytes is None:
                        break
                    size_bytes = read_exact(ser, 4, stop_event)
                    if size_bytes is None:
                        break
                    size = int.from_bytes(size_bytes, 'big')

                    if size == 0 or size > MAX_FRAME_BYTES:
                        with state_lock:
                            state["cameras"][cam_id]["log"].append(f"bad size {size}, resync")
                            state["cameras"][cam_id]["log"] = state["cameras"][cam_id]["log"][-50:]
                        continue

                    jpg = read_exact(ser, size, stop_event)
                    if jpg is None:
                        break

                    # Decode -> flip -> infer -> re-encode
                    arr   = np.frombuffer(jpg, dtype=np.uint8)
                    frame = cv2.imdecode(arr, cv2.IMREAD_COLOR)
                    if frame is None:
                        continue
                    frame = cv2.flip(frame, 1)

                    frame_count += 1
                    best_label = None
                    detections = []
                    if MODEL is not None and (frame_count % INFER_EVERY_N == 0):
                        frame, best_label, detections = run_inference(frame, callsign)

                    # Re-encode the (possibly annotated) frame for the browser
                    ok, encoded = cv2.imencode(
                        '.jpg', frame, [cv2.IMWRITE_JPEG_QUALITY, JPEG_QUALITY]
                    )
                    if not ok:
                        continue
                    out_jpg = encoded.tobytes()

                    with state_lock:
                        state["cameras"][cam_id]["latest_jpeg"] = out_jpg
                        state["cameras"][cam_id]["running"]     = True
                        state["cameras"][cam_id]["detections"]  = detections
                        if best_label:
                            state["cameras"][cam_id]["last_label"] = best_label
                            log  = state["cameras"][cam_id]["log"]
                            coords = ", ".join(
                                f"{d['label']}({d['cx']},{d['cy']})"
                                for d in detections
                            )
                            line = f"{len(detections)} det: {coords}"
                            # only append when the detection summary changes
                            if not log or log[-1] != line:
                                log.append(line)
                                state["cameras"][cam_id]["log"] = log[-50:]

        except serial.SerialException as e:
            print(f"[{key}] serial error: {e}; rescanning in 3s")
            with state_lock:
                state["cameras"][cam_id]["running"] = False
                state["cameras"][cam_id]["log"].append(f"disconnected: {e}")
                state["cameras"][cam_id]["log"] = state["cameras"][cam_id]["log"][-50:]
            time.sleep(3)
        except Exception as e:
            print(f"[{key}] unexpected error: {e}")
            time.sleep(3)

    with state_lock:
        state["cameras"][cam_id]["running"] = False
    print(f"[{key}] thread stopped")


# ─────────────────────────────────────────────────────────────────────────────
# ULTRASONIC THREAD
# ─────────────────────────────────────────────────────────────────────────────
def ultrasonic_thread(stop_event):
    print("[ultrasonic] thread started")

    while not stop_event.is_set():
        arduino_port, _ = find_devices()
        if not arduino_port:
            with state_lock:
                state["ultrasonic_running"] = False
            print("[ultrasonic] no Arduino found, retrying in 3s")
            time.sleep(3)
            continue

        try:
            with serial.Serial(arduino_port, BAUD, timeout=1) as ser:
                print(f"[ultrasonic] connected on {arduino_port}")
                ser.reset_input_buffer()
                with state_lock:
                    state["ultrasonic_running"] = True

                while not stop_event.is_set():
                    raw = ser.readline().decode("ascii", errors="ignore").strip()
                    if not raw:
                        continue
                    parts = raw.split(",")
                    if len(parts) != NUM_BINS:
                        continue
                    try:
                        distances = [int(v) for v in parts]
                    except ValueError:
                        continue

                    readings = {}
                    for i, d in enumerate(distances, start=1):
                        if d < 0:
                            readings[f"S{i}"] = {
                                "distance_mm":  None,
                                "fullness_pct": 0.0,
                                "is_full":      False,
                            }
                        else:
                            pct, full = fullness_from_mm(d)
                            readings[f"S{i}"] = {
                                "distance_mm":  max(0, min(d, MAX_DISTANCE_MM)),
                                "fullness_pct": pct,
                                "is_full":      full,
                            }

                    with state_lock:
                        state["bins"] = readings
                        state["ultrasonic_running"] = True

        except serial.SerialException as e:
            print(f"[ultrasonic] serial error: {e}; rescanning in 3s")
            with state_lock:
                state["ultrasonic_running"] = False
            time.sleep(3)
        except Exception as e:
            print(f"[ultrasonic] unexpected error: {e}")
            time.sleep(3)

    with state_lock:
        state["ultrasonic_running"] = False
    print("[ultrasonic] thread stopped")


# ─────────────────────────────────────────────────────────────────────────────
# Thread management
# ─────────────────────────────────────────────────────────────────────────────
def start_thread(name):
    if name in _threads and _threads[name].is_alive():
        return
    stop_events[name].clear()
    if name.startswith("camera_"):
        cam_id = int(name.split("_")[1])
        t = threading.Thread(target=camera_thread,
                             args=(cam_id, stop_events[name]),
                             daemon=True, name=name)
    elif name == "ultrasonic":
        t = threading.Thread(target=ultrasonic_thread,
                             args=(stop_events[name],),
                             daemon=True, name=name)
    else:
        raise ValueError(f"Unknown thread: {name}")
    _threads[name] = t
    t.start()


def stop_thread(name):
    stop_events[name].set()


def stop_all():
    for ev in stop_events.values():
        ev.set()


def start_all():
    for name in stop_events:
        start_thread(name)


# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("=== USB device scan ===")
    ap, cps = find_devices()
    print(f"Arduino: {ap or 'NOT FOUND'}")
    for cid, p in cps.items():
        print(f"CAM {cid}: {p}")
    if not cps:
        print("No OpenMV cameras found")
    print(f"YOLO: {'loaded' if MODEL is not None else 'DISABLED'}")
    print("=======================\n")

    start_all()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        stop_all()
