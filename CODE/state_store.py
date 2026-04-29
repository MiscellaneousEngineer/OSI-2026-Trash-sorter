"""
state_store.py - Single shared-state dict + lock.
Imported by main.py and dashboard.py so both see the same object
when running in the same process (via run_all.py).
"""

import threading

state_lock = threading.Lock()

state: dict = {
    "cameras": {
        0: {"running": False, "log": [], "last_label": "-", "latest_jpeg": None, "detections": []},
        1: {"running": False, "log": [], "last_label": "-", "latest_jpeg": None, "detections": []},
    },
    "bins": {
        f"S{i}": {"distance_mm": None, "fullness_pct": 0.0, "is_full": False}
        for i in range(1, 6)
    },
    "ultrasonic_running": False,
}
