"""
Wizard-of-Oz keyboard override.

Lets an operator force the robot into any state by pressing a key,
bypassing perception/speech recognition when the room is too noisy or
busy for the real pipeline (mic, HuskyLens) to pick up a cue. The normal
pipeline keeps running underneath — pressing a key just jumps the state
machine to wherever the operator wants it to go.

Key	Forces state
i	IDLE
a	ATTRACTING
l	LISTENING
e	ENGAGED
p	PLAYING
c	CALMING
r	ERROR
"""

import sys
import threading
import queue

try:
    import msvcrt
    _WINDOWS = True
except ImportError:
    msvcrt = None
    _WINDOWS = False

if not _WINDOWS:
    import termios
    import tty
    import select


class WozOverride:

    KEY_MAP = {
        "i": "IDLE",
        "a": "ATTRACTING",
        "l": "LISTENING",
        "e": "ENGAGED",
        "p": "PLAYING",
        "c": "CALMING",
        "r": "ERROR",
    }

    def __init__(self):
        self._queue: queue.Queue[str] = queue.Queue()
        self._running = True
        self._thread = threading.Thread(target=self._listen, daemon=True)
        self._thread.start()
        self._print_help()

    def _print_help(self):
        print("\n[WoZ] Keyboard override active. Press a key any time to force a state:")
        for key, name in self.KEY_MAP.items():
            print(f"  {key} -> {name}")
        print("[WoZ] If you don't press anything, the normal pipeline keeps running.\n")

    def _listen(self):
        if _WINDOWS:
            self._listen_windows()
        else:
            self._listen_unix()

    def _listen_windows(self):
        try:
            while self._running:
                if msvcrt.kbhit():
                    char = msvcrt.getwch().lower()
                    self._handle(char)
                else:
                    threading.Event().wait(0.05)
        except Exception as e:
            print(f"[WoZ] Keyboard listener crashed: {e!r} — overrides disabled.")

    def _listen_unix(self):
        if not sys.stdin.isatty():
            print("[WoZ] stdin is not a terminal — keyboard override disabled. "
                  "Run main.py directly in a real terminal (not via an IDE 'Run' "
                  "button or a piped/redirected stdin) for WoZ keys to work.")
            return
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setcbreak(fd)
            while self._running:
                ready, _, _ = select.select([sys.stdin], [], [], 0.05)
                if ready:
                    char = sys.stdin.read(1).lower()
                    self._handle(char)
        except Exception as e:
            print(f"[WoZ] Keyboard listener crashed: {e!r} — overrides disabled.")
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

    def _handle(self, char: str):
        if char in self.KEY_MAP:
            self._queue.put(self.KEY_MAP[char])

    def poll(self) -> str | None:
        """Non-blocking. Returns the pending state-name override, or None."""
        try:
            return self._queue.get_nowait()
        except queue.Empty:
            return None

    def stop(self):
        self._running = False
