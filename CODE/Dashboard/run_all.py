"""
run_all.py — Start everything in one command.

  python run_all.py

Launches the camera + ultrasonic worker threads (from main.py) and the
Flask dashboard (from dashboard.py) in a single process so they share
the same in-memory state defined in state_store.py.

Open http://localhost:5000 in your browser once it's running.
"""

import sys
import os

# Make sure sibling modules (main.py, dashboard.py, state_store.py) are importable
# regardless of where this script is invoked from.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import main as worker          # camera + ultrasonic threads
from dashboard import app      # Flask app (already wired to state_store)


def main() -> None:
    print("=" * 60)
    print(" Starting worker threads (cameras + ultrasonic)…")
    print("=" * 60)
    worker.start_all()

    try:
        print("\nDashboard running at http://localhost:5000  (Ctrl-C to stop)\n")
        # use_reloader=False is critical — the reloader spawns a 2nd process
        # which would start the worker threads twice and break the shared state.
        app.run(host="0.0.0.0", port=5000, debug=False, use_reloader=False)
    except KeyboardInterrupt:
        print("\nKeyboard interrupt received.")
    finally:
        print("Stopping worker threads…")
        worker.stop_all()
        print("Bye.")


if __name__ == "__main__":
    main()
