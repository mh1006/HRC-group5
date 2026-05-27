import random
import time
from hardware.arduino_controller import ArduinoController, Emotion

COLORS = ["RED", "BLUE", "GREEN", "YELLOW"]

class ColorGame:
    def __init__(self, arduino: ArduinoController):
        self.arduino = arduino
        self.score = 0
        self.round = 0
        self.max_rounds = 5
        self.time_limit = 5.0       # seconds to respond

    def run(self) -> bool:
        """Run a full game. Returns True if player won."""
        self.score = 0

        for self.round in range(1, self.max_rounds + 1):
            correct = self._run_round()
            if not correct:
                self._on_wrong()
                return False        # end game on first mistake, or remove to allow retries

        self._on_win()
        return True

    def _run_round(self) -> bool:
        target_color = random.choice(COLORS)
        time_limit = max(2.0, self.time_limit - self.round * 0.3)  # gets harder

        # Flush stale button presses from the previous round
        self.arduino.drain_log()

        # Tell Arduino to show the color
        self.arduino.set_game_color(target_color)

        # Wait for button press, with timeout
        deadline = time.time() + time_limit
        while time.time() < deadline:
            event = self._poll_button()
            if event:
                if event == target_color:
                    self.score += 1
                    self._on_correct()
                    return True
                else:
                    return False    # wrong button
            time.sleep(0.01)        # avoid busy-wait

        return False                # timeout counts as wrong

    def _poll_button(self) -> str | None:
        """Check if Arduino sent a BTN event. Non-blocking."""
        for line in self.arduino.drain_log():
            if line.startswith("BTN,"):
                return line.split(",")[1].strip()
        return None

    def _on_correct(self):
        self.arduino.set_emotion(Emotion.HAPPY)
        time.sleep(0.5)

    def _on_wrong(self):
        self.arduino.set_emotion(Emotion.SAD)
        time.sleep(1.0)

    def _on_win(self):
        self.arduino.set_emotion(Emotion.HAPPY)
        # TODO: play victory sound
