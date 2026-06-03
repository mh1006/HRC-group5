"""
Run this to test the game logic without a physical Arduino.

How to play:
  - When the robot shows a color, type it and press Enter: RED / BLUE / GREEN / YELLOW
  - You have a limited time window (same as the real game)
  - Press Ctrl+C to quit at any time
"""

import threading
from queue import Queue, Empty
from python.hardware.game_handler import FullGame


class MockArduino:
    def __init__(self):
        self._queue: Queue[str] = Queue()
        self._reader = threading.Thread(target=self._read_input, daemon=True)
        self._reader.start()

    def _read_input(self):
        while True:
            try:
                line = input().strip().upper()
                if line in ["RED", "BLUE", "GREEN", "YELLOW"]:
                    self._queue.put(f"BTN,{line}")
                elif line:
                    print(f"  (unknown input '{line}' — use RED, BLUE, GREEN or YELLOW)")
            except EOFError:
                break

    def drain_log(self) -> list[str]:
        lines = []
        while True:
            try:
                lines.append(self._queue.get_nowait())
            except Empty:
                break
        return lines

    def set_emotion(self, emotion):
        labels = {
            "HAPPY": "😊 HAPPY", "SAD": "😢 SAD",
            "SURPRISED": "😮 SURPRISED", "ANGRY": "😠 ANGRY", "NEUTRAL": "😐 NEUTRAL"
        }
        print(f"  [eyes] {labels.get(emotion.value, emotion)}")

    def set_game_color(self, color):
        blocks = {"RED": "🟥", "BLUE": "🟦", "GREEN": "🟩", "YELLOW": "🟨", "OFF": "⬛"}
        print(f"  [color] {blocks.get(color, color)} {color}")

    # State convenience methods (unused during game but needed so calls don't crash)
    def on_idle(self):        print("  [state] idle")
    def on_attracting(self):  print("  [state] attracting")
    def on_engaged(self):     print("  [state] engaged")
    def on_playing(self):     print("  [state] playing")
    def on_calming(self):     print("  [state] calming")
    def on_error(self):       print("  [state] error")


if __name__ == "__main__":
    print("=== Game Test (no Arduino) ===")
    print("Type RED / BLUE / GREEN / YELLOW and press Enter to press a button.")
    print("Starting in 2 seconds...\n")

    import time
    time.sleep(2)

    arduino = MockArduino()
    won = FullGame(arduino).run()

    print("\n" + ("=== YOU WON! ===" if won else "=== GAME OVER ==="))
