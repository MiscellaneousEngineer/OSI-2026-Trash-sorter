import serial
import numpy as np
import cv2
import time
import threading

CAMERAS = {
    'CAM1': '/dev/ttyACM0',
    'CAM2': '/dev/ttyACM1',
}
MAGIC = b'FRAME'

frames = {'CAM1': None, 'CAM2': None}
lock = threading.Lock()

def read_exact(ser, n):
    buf = b''
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if chunk:
            buf += chunk
    return buf

def camera_thread(callsign, port):
    while True:
        try:
            with serial.Serial(port, 115200, timeout=5) as ser:
                print(f"[{callsign}] Connected on {port}")
                while True:
                    header = b''
                    while header != MAGIC:
                        byte = ser.read(1)
                        if not byte:
                            continue
                        header = (header + byte)[-len(MAGIC):]

                    incoming_callsign = read_exact(ser, 4).decode('ascii')

                    size_bytes = read_exact(ser, 4)
                    size = int.from_bytes(size_bytes, 'big')

                    if size == 0 or size > 50000:
                        print(f"[{callsign}] Bad frame size: {size}, re-syncing...")
                        continue

                    jpg_data = read_exact(ser, size)
                    np_arr = np.frombuffer(jpg_data, dtype=np.uint8)
                    frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)

                    if frame is None:
                        print(f"[{callsign}] Failed to decode, skipping...")
                        continue

                    frame = cv2.flip(frame, 1)

                    # --- ADD COORDINATES READING HERE (2 bytes X + 2 bytes Y after callsign) ---

                    with lock:
                        frames[callsign] = frame

        except serial.SerialException as e:
            print(f"[{callsign}] Disconnected: {e}, retrying in 3s...")
            with lock:
                frames[callsign] = None
            time.sleep(3)

# Start one thread per camera
for name, port in CAMERAS.items():
    t = threading.Thread(target=camera_thread, args=(name, port), daemon=True)
    t.start()

print("Press Q to quit")
while True:
    with lock:
        f1 = frames['CAM1'].copy() if frames['CAM1'] is not None else None
        f2 = frames['CAM2'].copy() if frames['CAM2'] is not None else None

    if f1 is not None and f2 is not None:
        # Label bottom-left corner of each frame
        h1, w1 = f1.shape[:2]
        h2, w2 = f2.shape[:2]
        cv2.putText(f1, 'CAM1', (10, h1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.putText(f2, 'CAM2', (10, h2 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        combined = np.hstack((f1, f2))
        cv2.imshow('S.T.R.I.A Feed', combined)

    elif f1 is not None:
        h1, w1 = f1.shape[:2]
        cv2.putText(f1, 'CAM1', (10, h1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.imshow('S.T.R.I.A Feed', f1)

    elif f2 is not None:
        h2, w2 = f2.shape[:2]
        cv2.putText(f2, 'CAM2', (10, h2 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.imshow('S.T.R.I.A Feed', f2)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()
