"""
dashboard.py - Flask server for the live dashboard AND the Flutter phone app.

Existing endpoints:
  GET /             -> dashboard.html
  GET /api/state    -> JSON (polled every 500 ms, no images)
  POST /api/control -> start/stop threads
  GET /video/<id>   -> MJPEG stream of cam_id

Phone-app endpoints:
  GET /camera      -> single latest JPEG (Flutter polls this on a Timer)
  GET /bins        -> bin fill levels in the Flutter app's shape
  GET /detections  -> latest detections from all cameras
  GET /health      -> overall system health summary  (NEW)

Run via run_all.py.
"""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import time
from flask import Flask, jsonify, request, send_from_directory, Response

from state_store import state, state_lock
import main as worker

app = Flask(__name__, static_folder=".")


# ─────────────────────────────────────────────────────────────────────────────
# Phone-app config — edit to match your physical bin layout
# ─────────────────────────────────────────────────────────────────────────────
BIN_LAYOUT = [
    {"sid": "S1", "id": "metal",   "label": "Metal"},
    {"sid": "S2", "id": "glass",   "label": "Glass"},
    {"sid": "S3", "id": "plastic", "label": "Plastic"},
    {"sid": "S4", "id": "paper",   "label": "Paper"},
    {"sid": "S5", "id": "other",   "label": "Other"},
]
BIN_CAPACITY_L  = 20
PHONE_CAMERA_ID = 0

# Health thresholds
BIN_NEAR_FULL_PCT = 85.0


# ─────────────────────────────────────────────────────────────────────────────
# CORS — required for the Flutter app when running in Chrome
# ─────────────────────────────────────────────────────────────────────────────
@app.after_request
def add_cors_headers(resp):
    resp.headers["Access-Control-Allow-Origin"]  = "*"
    resp.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS"
    resp.headers["Access-Control-Allow-Headers"] = "*"
    return resp


# ─────────────────────────────────────────────────────────────────────────────
# Existing endpoints (unchanged)
# ─────────────────────────────────────────────────────────────────────────────
@app.route("/")
def index():
    return send_from_directory(".", "dashboard.html")


@app.route("/api/state")
def api_state():
    with state_lock:
        cameras_out = {}
        for cam_id, cam in state["cameras"].items():
            cameras_out[str(cam_id)] = {
                "running":      cam["running"],
                "last_label":   cam["last_label"],
                "log":          cam["log"][-20:],
                "has_frame":    cam["latest_jpeg"] is not None,
                "detections":   cam.get("detections", []),
            }

        bins_out = {}
        for sid, data in state["bins"].items():
            bins_out[sid] = {
                "distance_mm":  data["distance_mm"],
                "fullness_pct": data["fullness_pct"],
                "is_full":      data["is_full"],
            }

        thread_status = {name: t.is_alive() for name, t in worker._threads.items()}

        return jsonify({
            "cameras":            cameras_out,
            "bins":               bins_out,
            "ultrasonic_running": state["ultrasonic_running"],
            "thread_status":      thread_status,
        })


@app.route("/video/<int:cam_id>")
def video(cam_id):
    boundary = b"frame"

    def generate():
        last_id = None
        while True:
            with state_lock:
                jpg = state["cameras"].get(cam_id, {}).get("latest_jpeg")
            if jpg is not None and id(jpg) != last_id:
                last_id = id(jpg)
                yield b"--" + boundary + b"\r\n"
                yield b"Content-Type: image/jpeg\r\n"
                yield ("Content-Length: " + str(len(jpg)) + "\r\n\r\n").encode()
                yield jpg
                yield b"\r\n"
            time.sleep(0.03)

    return Response(generate(),
                    mimetype="multipart/x-mixed-replace; boundary=frame")


@app.route("/api/control", methods=["POST"])
def api_control():
    data   = request.get_json(force=True)
    action = data.get("action", "")
    thread = data.get("thread", "")

    if action == "stop_all":
        worker.stop_all()
        return jsonify({"ok": True, "action": "stop_all"})
    if action == "start_all":
        worker.start_all()
        return jsonify({"ok": True, "action": "start_all"})
    if action == "stop" and thread:
        worker.stop_thread(thread)
        return jsonify({"ok": True, "action": "stop", "thread": thread})
    if action == "start" and thread:
        worker.start_thread(thread)
        return jsonify({"ok": True, "action": "start", "thread": thread})

    return jsonify({"ok": False, "error": "unknown action"}), 400


# ─────────────────────────────────────────────────────────────────────────────
# Phone-app endpoints
# ─────────────────────────────────────────────────────────────────────────────

@app.route("/camera")
def phone_camera():
    cam_id = int(request.args.get("cam", PHONE_CAMERA_ID))
    with state_lock:
        jpg = state["cameras"].get(cam_id, {}).get("latest_jpeg")
    if jpg is None:
        return Response(status=503)
    return Response(jpg, mimetype="image/jpeg",
                    headers={"Cache-Control": "no-store"})


@app.route("/bins")
def phone_bins():
    with state_lock:
        bins_payload = []
        for entry in BIN_LAYOUT:
            sensor = state["bins"].get(entry["sid"], {})
            fullness = sensor.get("fullness_pct") or 0
            bins_payload.append({
                "id":           entry["id"],
                "label":        entry["label"],
                "fill_percent": float(fullness),
                "capacity_l":   BIN_CAPACITY_L,
            })
    return jsonify({
        "timestamp": time.time(),
        "bins":      bins_payload,
    })


@app.route("/detections")
def phone_detections():
    with state_lock:
        out = []
        next_id = 1
        for cam_id, cam in state["cameras"].items():
            for det in cam.get("detections", []):
                if not isinstance(det, dict):
                    continue
                out.append({
                    "id":         next_id,
                    "label":      str(det.get("label", "")),
                    "confidence": float(det.get("conf", 0)),
                    "bbox":       list(det.get("bbox", [0, 0, 0, 0])),
                })
                next_id += 1
    return jsonify({
        "timestamp": time.time(),
        "objects":   out,
    })


@app.route("/health")
def phone_health():
    """Overall system health summary for the Flutter app's Health tab.

    Each subsystem reports `status`: 'ok' (green), 'warn' (orange),
    or 'error' (red). The overall status is the worst of the subsystems.
    """
    with state_lock:
        cameras = state["cameras"]
        bins    = state["bins"]
        ultrasonic_running = bool(state.get("ultrasonic_running", False))

        # ---- Cameras ----
        cam_total  = len(cameras)
        cam_online = sum(
            1 for cam in cameras.values()
            if cam.get("running") and cam.get("latest_jpeg") is not None
        )
        if cam_total == 0:
            cam_status = "error"
        elif cam_online == cam_total:
            cam_status = "ok"
        elif cam_online > 0:
            cam_status = "warn"
        else:
            cam_status = "error"

        # ---- Ultrasonic system ----
        sensors_total = len(bins)
        sensors_responsive = sum(
            1 for b in bins.values() if b.get("distance_mm") is not None
        )
        if not ultrasonic_running:
            ultrasonic_status = "error"
            ultrasonic_detail = "Not running"
        elif sensors_responsive == sensors_total:
            ultrasonic_status = "ok"
            ultrasonic_detail = "{}/{} responsive".format(
                sensors_responsive, sensors_total)
        elif sensors_responsive > 0:
            ultrasonic_status = "warn"
            ultrasonic_detail = "{}/{} responsive".format(
                sensors_responsive, sensors_total)
        else:
            ultrasonic_status = "error"
            ultrasonic_detail = "0/{} responsive".format(sensors_total)

        # ---- YOLO model ----
        yolo_loaded = worker.MODEL is not None
        yolo_status = "ok" if yolo_loaded else "warn"
        yolo_detail = "Loaded" if yolo_loaded else "Disabled"

        # ---- Bins fullness alert ----
        bins_full = sum(1 for b in bins.values() if b.get("is_full"))
        bins_near = sum(
            1 for b in bins.values()
            if (b.get("fullness_pct") or 0) >= BIN_NEAR_FULL_PCT
            and not b.get("is_full")
        )
        if bins_full > 0:
            bins_status = "warn"
            bins_detail = "{} full".format(bins_full)
        elif bins_near > 0:
            bins_status = "warn"
            bins_detail = "{} nearly full".format(bins_near)
        else:
            bins_status = "ok"
            bins_detail = "All within limits"

        # ---- Worker threads ----
        threads = {name: t.is_alive() for name, t in worker._threads.items()}
        threads_alive = sum(1 for alive in threads.values() if alive)
        threads_total = len(threads)
        if threads_total == 0:
            threads_status = "error"
        elif threads_alive == threads_total:
            threads_status = "ok"
        else:
            threads_status = "warn"
        threads_detail = "{}/{} alive".format(threads_alive, threads_total)

        subsystems = [
            {"id": "cameras",    "label": "Cameras",
             "status": cam_status,
             "detail": "{}/{} online".format(cam_online, cam_total)},
            {"id": "ultrasonic", "label": "Ultrasonic Sensors",
             "status": ultrasonic_status, "detail": ultrasonic_detail},
            {"id": "yolo",       "label": "YOLO Model",
             "status": yolo_status,       "detail": yolo_detail},
            {"id": "bins",       "label": "Bins",
             "status": bins_status,       "detail": bins_detail},
            {"id": "threads",    "label": "Worker Threads",
             "status": threads_status,    "detail": threads_detail},
        ]

    # Overall = worst of subsystems. error > warn > ok.
    statuses = [s["status"] for s in subsystems]
    if "error" in statuses:
        overall = "error"
    elif "warn" in statuses:
        overall = "warn"
    else:
        overall = "ok"

    return jsonify({
        "timestamp":  time.time(),
        "overall":    overall,
        "subsystems": subsystems,
    })


# ─────────────────────────────────────────────────────────────────────────────
# Entrypoint
# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    worker.start_all()
    print("Dashboard at http://localhost:5000")
    app.run(host="0.0.0.0", port=5000, debug=False,
            use_reloader=False, threaded=True)
