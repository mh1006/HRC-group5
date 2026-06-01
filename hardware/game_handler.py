"""
Game logic for the hospital waiting-room robot.

Two game classes (ColorGame, SequenceGame) are chained by FullGame.
Use FullGame.run() as the single entry point from state.py.

Exit conditions shared by both games:
  - Child presses the wrong button → immediate fail.
  - No button press within the timeout → treated as "child left / disengaged" → fail.
  - A failed round ends the whole game (robot shows SAD, FullGame.run() returns False).
  - Completing all rounds → robot shows HAPPY, game returns True.

Arduino serial protocol used here:
  - Outgoing: set_game_color(color), set_emotion(emotion)
  - Incoming: "BTN,<COLOR>" lines emitted by check_buttons() in sensor.h
"""

import random
import time
from hardware.arduino_controller import ArduinoController, Emotion

COLORS = ["RED", "BLUE", "GREEN", "YELLOW"]


class ColorGame:
    """
    Phase 1 — Color Match.

    Each round the robot lights its eyes with a random color.
    The child must press the matching button before time runs out.

    Exact per-round behavior
    ────────────────────────
    1. Eyes light up with a random color (RED / BLUE / GREEN / YELLOW).
    2. Robot waits for a button press.
       • Correct button  → HAPPY eyes for 0.5 s, then next round.
       • Wrong button    → game ends immediately (see _on_wrong).
       • No press within the time limit → game ends (timeout = wrong).
    3. Time limit starts at 5 s and shrinks by 0.3 s each round (floor: 2 s),
       so the game gets harder as it progresses.

    End-of-game behavior
    ────────────────────
    • All 5 rounds correct → _on_win()  : HAPPY eyes.
    • Any round fails      → _on_wrong(): SAD eyes for 1 s.
    """

    def __init__(self, arduino: ArduinoController):
        self.arduino = arduino
        self.score = 0
        self.round = 0
        self.max_rounds = 5
        self.time_limit = 5.0       # seconds allowed in round 1

    def run(self) -> bool:
        """Run all rounds. Returns True if child completed every round correctly."""
        self.score = 0

        for self.round in range(1, self.max_rounds + 1):
            correct = self._run_round()
            if not correct:
                self._on_wrong()
                return False

        self._on_win()
        return True

    def _run_round(self) -> bool:
        target_color = random.choice(COLORS)
        time_limit = max(2.0, self.time_limit - self.round * 0.3)

        self.arduino.drain_log()            # discard stale presses from last round
        self.arduino.set_game_color(target_color)

        deadline = time.time() + time_limit
        while time.time() < deadline:
            event = self._poll_button()
            if event:
                if event == target_color:
                    self.score += 1
                    self._on_correct()
                    return True
                else:
                    return False            # wrong button
            time.sleep(0.01)

        return False                        # timeout

    def _poll_button(self) -> str | None:
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


class SequenceGame:
    """
    Phase 2 — Sequence Repeat (Simon Says).

    The robot shows a sequence of colors one at a time. The child must
    then press the buttons in exactly the same order.

    Each round the sequence grows by one color (start_length + round - 1).

    Exact per-round behavior
    ────────────────────────
    DEMO PHASE — robot shows the sequence:
      1. SURPRISED eyes for 0.6 s  →  signals "watch me carefully".
      2. For each color in the sequence:
           a. Eyes light up with that color for 1.2 s.
           b. Eyes return to SURPRISED for 0.4 s  (the gap makes two consecutive
              identical colors feel like separate steps, not one long flash).
      3. GO SIGNAL — robot rapidly cycles through all four colors
         (RED → BLUE → GREEN → YELLOW) three times, each color shown for 0.1 s
         (~1.2 s total). This acts as a language-free countdown / "your turn" cue.
      4. NEUTRAL eyes for 0.3 s.
      5. Serial buffer is drained  →  any accidental button presses made
         during the demo are discarded.

    INPUT PHASE — child repeats the sequence:
      For each color the child must press (in order):
        • No press within 8 s    → timeout → treated as "child left" → game ends.
        • Wrong button pressed   → game ends immediately.
        • Correct button pressed → eyes briefly flash that color for 0.25 s,
                                   then wait for the next button in the sequence.

    End-of-round / end-of-game behavior
    ─────────────────────────────────────
    • Round complete (all buttons correct) → HAPPY eyes for 0.5 s, next round begins.
    • Any failure                          → SAD eyes for 1 s, game ends.
    • All rounds complete                  → _on_win(): HAPPY eyes.
    """

    COLOR_SHOW_DURATION = 1.2   # seconds each color is displayed during demo
    COLOR_GAP_DURATION  = 0.4   # seconds of SURPRISED gap between demo colors
    INPUT_TIMEOUT       = 8.0   # seconds child has to press each button

    def __init__(self, arduino: ArduinoController, max_rounds: int = 3, start_length: int = 3):
        self.arduino = arduino
        self.score = 0
        self.round = 0
        self.max_rounds = max_rounds
        self.start_length = start_length

    def run(self) -> bool:
        """Run all rounds. Returns True if child completed every round correctly."""
        self.score = 0

        for self.round in range(1, self.max_rounds + 1):
            length   = self.start_length + self.round - 1
            sequence = [random.choice(COLORS) for _ in range(length)]

            correct = self._run_round(sequence)
            if not correct:
                self._on_wrong()
                return False

            self.score += 1
            self._on_correct()

        self._on_win()
        return True

    def _run_round(self, sequence: list) -> bool:
        self._show_sequence(sequence)
        self._signal_your_turn()

        for expected in sequence:
            pressed = self._wait_for_button(self.INPUT_TIMEOUT)
            if pressed != expected:
                return False                # wrong button or timeout
            # Brief positive flash so child knows this step was correct
            self.arduino.set_game_color(pressed)
            time.sleep(0.25)

        return True

    def _show_sequence(self, sequence: list):
        self.arduino.set_emotion(Emotion.SURPRISED)
        time.sleep(0.6)

        for color in sequence:
            self.arduino.set_game_color(color)
            time.sleep(self.COLOR_SHOW_DURATION)
            # Gap between colors: return to SURPRISED so two identical colors
            # don't blur into one long flash
            self.arduino.set_emotion(Emotion.SURPRISED)
            time.sleep(self.COLOR_GAP_DURATION)

    def _signal_your_turn(self):
        # Rapid 4-color cycle × 3 = language-free "go!" countdown
        for _ in range(3):
            for color in COLORS:
                self.arduino.set_game_color(color)
                time.sleep(0.1)

        self.arduino.set_emotion(Emotion.NEUTRAL)
        time.sleep(0.3)
        self.arduino.drain_log()    # discard any presses made during the demo

    def _wait_for_button(self, timeout: float) -> str | None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            for line in self.arduino.drain_log():
                if line.startswith("BTN,"):
                    return line.split(",")[1].strip()
            time.sleep(0.01)
        return None                 # timeout → caller treats as wrong

    def _on_correct(self):
        self.arduino.set_emotion(Emotion.HAPPY)
        time.sleep(0.5)

    def _on_wrong(self):
        self.arduino.set_emotion(Emotion.SAD)
        time.sleep(1.0)

    def _on_win(self):
        self.arduino.set_emotion(Emotion.HAPPY)
        # TODO: play victory sound


class FullGame:
    """
    Complete two-phase game session. Use this as the single entry point
    from state.py.

    Phase 1 → ColorGame (5 rounds of color matching)
    Transition animation
    Phase 2 → SequenceGame (3 rounds of sequence repetition)

    Exact full-session behavior
    ────────────────────────────
    PHASE 1 — Color Match (5 rounds):
      See ColorGame docstring for per-round detail.
      • Any failure → SAD eyes 1 s → session ends, returns False.
      • All 5 rounds correct → transition animation begins.

    TRANSITION (phase 1 → phase 2):
      1. All four colors cycle twice quickly (each for 0.15 s) — "level up" signal.
      2. SURPRISED eyes for 1.0 s  →  primes child's attention for the new challenge.

    PHASE 2 — Sequence Repeat (3 rounds, starting with 3-color sequences):
      See SequenceGame docstring for per-round detail.
      • Any failure → SAD eyes 1 s → session ends, returns False.
      • All 3 rounds correct → HAPPY eyes → session ends, returns True.

    Usage from state.py:
        game = FullGame(arduino)
        won = game.run()
    """

    def __init__(self, arduino: ArduinoController):
        self.arduino = arduino

    def run(self) -> bool:
        """Run both phases back-to-back. Returns True if child wins both."""
        if not ColorGame(self.arduino).run():
            return False

        self._transition_to_phase2()

        return SequenceGame(self.arduino).run()

    def _transition_to_phase2(self):
        """
        Level-up animation between phases.

        All four colors cycle twice (each color 0.15 s) to signal something
        new is starting, then SURPRISED eyes hold for 1 s to prime attention.
        """
        for _ in range(2):
            for color in COLORS:
                self.arduino.set_game_color(color)
                time.sleep(0.15)

        self.arduino.set_emotion(Emotion.SURPRISED)
        time.sleep(1.0)
