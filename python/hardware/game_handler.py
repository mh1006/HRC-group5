"""
Game logic for the hospital waiting-room robot.

Two game classes (ColorGame, SequenceGame) are chained by FullGame.
Use FullGame.run() as the single entry point from state.py.

Exit conditions shared by both games:
  - Child presses the wrong button → SAD eyes, then the round retries (up to max_retries times).
  - No button press within the timeout → treated as wrong → same retry logic applies.
  - Exhausting all retries on a round ends the whole game (FullGame.run() returns False).
  - Completing all rounds → robot shows HAPPY, game returns True.

Arduino serial protocol used here:
  - Outgoing: set_game_color(color) for a "watch this" cue, set_button_colors(colors)
    to assign one color per physical button (2 eye matrices + LCD), set_emotion(emotion)
  - Incoming: JSON {"pressed_button": <0|1|2>} lines, parsed via arduino.parse_button()

There are only 3 physical buttons (arduino_controller/config.h::BUTTON_PINS), so
each round 3 colors are randomly drawn from the full palette and assigned to the
3 buttons — which button means which color rotates every round.
"""

import random
import time
from python.hardware.arduino_controller import ArduinoController, Emotion, Sound

COLORS = ["RED", "GREEN", "BLUE", "YELLOW", "CYAN", "PURPLE", "WHITE"]


class ColorGame:
    """
    Phase 1 — Color Match.

    Each round 3 colors are randomly drawn from the palette and assigned to
    the 3 physical buttons (rotates every round — there's no fixed button↔color
    mapping). The robot then flashes the target color as a cue before revealing
    the button layout, and the child must press the button currently showing
    that color before time runs out.

    Exact per-round behavior
    ────────────────────────
    1. 3 random colors are assigned to the 3 buttons; one of them is picked as
       the target and flashed solo across all indicators as a "watch this" cue.
    2. The button layout (3 distinct colors) is revealed and the robot waits
       for a button press.
       • Correct button  → HAPPY eyes for 0.5 s, then next round.
       • Wrong button or timeout → SAD eyes for 1 s, then the same round retries.
         After max_retries failed attempts the game ends.
    3. Time limit starts at 15 s and shrinks by 0.3 s each round (floor: 10 s),
       so the game gets harder as it progresses while still giving the child
       a realistic amount of time to find the right button.

    End-of-game behavior
    ────────────────────
    • All 5 rounds correct         → _on_win(): HAPPY eyes.
    • Round fails all retries      → _on_wrong() then game ends (returns False).
    """

    def __init__(self, arduino: ArduinoController, max_retries: int = 2):
        self.arduino = arduino
        self.score = 0
        self.round = 0
        self.max_rounds = 5
        self.max_retries = max_retries
        self.time_limit = 15.0      # seconds allowed in round 1

    def run(self) -> bool:
        """Run all rounds. Returns True if child completed every round correctly."""
        self.score = 0

        for self.round in range(1, self.max_rounds + 1):
            for attempt in range(self.max_retries + 1):
                correct = self._run_round()
                if correct:
                    break
                self._on_wrong()
                if attempt >= self.max_retries:
                    return False

        self._on_win()
        return True

    def _run_round(self) -> bool:
        button_colors = random.sample(COLORS, 3)
        target_index = random.randrange(3)
        target_color = button_colors[target_index]
        time_limit = max(10.0, self.time_limit - self.round * 0.3)

        self.arduino.drain_log()                   # discard stale presses from last round
        self.arduino.set_game_color(target_color)  # cue: flash target color solo
        time.sleep(4.0)                            # hold long enough for kids to register the color
        self.arduino.set_button_colors(button_colors)  # reveal the button layout
        time.sleep(0.2)                            # give Arduino time to process and render
        self.arduino.drain_log()                   # discard presses made during the cue

        print(f"[Game] Button colors: 0={button_colors[0]}, 1={button_colors[1]}, 2={button_colors[2]}")
        print(f"[Game] Target: button index {target_index} ({target_color})")
        deadline = time.time() + time_limit
        while time.time() < deadline:
            pressed_index = self._poll_button()
            if pressed_index is not None:
                print(f"[Game] Pressed button index: {pressed_index}, needed: {target_index} -> {'CORRECT' if pressed_index == target_index else 'WRONG'}")
                if pressed_index == target_index:
                    self.score += 1
                    self._on_correct()
                    return True
                else:
                    return False            # wrong button
            time.sleep(0.01)

        return False                        # timeout

    def _poll_button(self) -> int | None:
        for line in self.arduino.drain_log():
            index = self.arduino.parse_button(line)
            if index is not None:
                return index
        return None

    def _on_correct(self):
        self.arduino.set_emotion(Emotion.HAPPY)
        self.arduino.set_servo(90, 70)
        self.arduino.set_audio(Sound.CORRECT)
        time.sleep(1.5)
        self.arduino.look_forward()
        self.arduino.set_game_color("OFF")  # clear so next round's cue color is obvious
        time.sleep(0.5)

    def _on_wrong(self):
        self.arduino.set_emotion(Emotion.SAD)
        self.arduino.set_servo(90, 5)
        self.arduino.set_audio(Sound.WRONG)
        time.sleep(2.0)
        self.arduino.set_game_color("OFF")
        time.sleep(0.5)

    def _on_win(self):
        self.arduino.set_emotion(Emotion.HAPPY)
        self.arduino.play_animation("NOD")


class SequenceGame:
    """
    Phase 2 — Sequence Repeat (Simon Says).

    Each round 3 colors are randomly assigned to the 3 physical buttons (fixed
    for the whole round) and the robot shows a sequence of button positions
    one at a time, by flashing each position's color solo. The child must
    then press the buttons in exactly the same order.

    Each round the sequence grows by one step (start_length + round - 1).

    Exact per-round behavior
    ────────────────────────
    DEMO PHASE — robot shows the sequence:
      1. SURPRISED eyes for 0.6 s  →  signals "watch me carefully".
      2. For each step in the sequence:
           a. Only that step's button lights up (its assigned color), others
              dark, for 1.2 s.
           b. All indicators go dark for 0.4 s  (the gap makes two consecutive
              identical steps feel like separate flashes, not one long one).
      3. GO SIGNAL — robot rapidly cycles through the color palette three
         times, each color shown for 0.1 s (~1.2 s total). This acts as a
         language-free countdown / "your turn" cue.
      4. The round's 3 button colors are revealed together, NEUTRAL eyes for 0.3 s.
      5. Serial buffer is drained  →  any accidental button presses made
         during the demo are discarded.

    INPUT PHASE — child repeats the sequence:
      For each step the child must press the matching button (in order):
        • No press within 15 s   → timeout → treated as wrong (see below).
        • Wrong button pressed   → SAD eyes for 1 s, then the whole round replays
                                   (same sequence shown again). After max_retries
                                   failed attempts the game ends.
        • Correct button pressed → that button briefly flashes solo for 0.25 s,
                                   then the full layout returns and the robot
                                   waits for the next button in the sequence.

    End-of-round / end-of-game behavior
    ─────────────────────────────────────
    • Round complete (all buttons correct) → HAPPY eyes for 0.5 s, next round begins.
    • Round fails all retries              → SAD eyes for 1 s, game ends.
    • All rounds complete                  → _on_win(): HAPPY eyes.
    """

    COLOR_SHOW_DURATION = 1.2   # seconds each color is displayed during demo
    COLOR_GAP_DURATION  = 0.4   # seconds of SURPRISED gap between demo colors
    INPUT_TIMEOUT       = 15.0  # seconds child has to press each button

    def __init__(self, arduino: ArduinoController, max_rounds: int = 3, start_length: int = 3, max_retries: int = 2):
        self.arduino = arduino
        self.score = 0
        self.round = 0
        self.max_rounds = max_rounds
        self.max_retries = max_retries
        self.start_length = start_length

    def run(self) -> bool:
        """Run all rounds. Returns True if child completed every round correctly."""
        self.score = 0

        for self.round in range(1, self.max_rounds + 1):
            length        = self.start_length + self.round - 1
            button_colors = random.sample(COLORS, 3)
            sequence      = [random.randrange(3) for _ in range(length)]

            for attempt in range(self.max_retries + 1):
                correct = self._run_round(sequence, button_colors)
                if correct:
                    break
                self._on_wrong()
                if attempt >= self.max_retries:
                    return False

            self.score += 1
            self._on_correct()

        self._on_win()
        return True

    def _run_round(self, sequence: list, button_colors: list) -> bool:
        self._show_sequence(sequence, button_colors)
        self._signal_your_turn(button_colors)

        for expected_index in sequence:
            pressed_index = self._wait_for_button(self.INPUT_TIMEOUT)
            if pressed_index != expected_index:
                return False                # wrong button or timeout
            # Brief positive flash so child knows this step was correct
            self._flash_single(pressed_index, button_colors)
            time.sleep(0.25)
            self.arduino.set_button_colors(button_colors)  # restore full layout

        return True

    def _flash_single(self, index: int, button_colors: list):
        """Light only one button's indicator, others off."""
        colors = ["OFF", "OFF", "OFF"]
        colors[index] = button_colors[index]
        self.arduino.set_button_colors(colors)

    def _show_sequence(self, sequence: list, button_colors: list):
        self.arduino.set_emotion(Emotion.SURPRISED)
        time.sleep(0.6)

        for index in sequence:
            self._flash_single(index, button_colors)
            time.sleep(self.COLOR_SHOW_DURATION)
            self.arduino.set_game_color("OFF")  # brief dark pause separates consecutive identical steps
            time.sleep(self.COLOR_GAP_DURATION)

    def _signal_your_turn(self, button_colors: list):
        # Rapid palette cycle × 3 = language-free "go!" countdown
        for _ in range(3):
            for color in COLORS:
                self.arduino.set_game_color(color)
                time.sleep(0.1)

        self.arduino.set_button_colors(button_colors)  # reveal the round's button layout
        self.arduino.look_up()
        self.arduino.set_emotion(Emotion.NEUTRAL)
        time.sleep(0.3)
        self.arduino.drain_log()    # discard any presses made during the demo

    def _wait_for_button(self, timeout: float) -> int | None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            for line in self.arduino.drain_log():
                index = self.arduino.parse_button(line)
                if index is not None:
                    return index
            time.sleep(0.01)
        return None                 # timeout → caller treats as wrong

    def _on_correct(self):
        self.arduino.set_emotion(Emotion.HAPPY)
        self.arduino.set_servo(90, 70)   # quick head up
        self.arduino.set_audio(Sound.CORRECT)
        time.sleep(1.5)
        self.arduino.look_forward()

    def _on_wrong(self):
        self.arduino.set_emotion(Emotion.SAD)
        self.arduino.set_servo(90, 5)    # quick head down
        self.arduino.set_audio(Sound.WRONG)
        time.sleep(2.0)
        self.arduino.look_forward()

    def _on_win(self):
        self.arduino.set_emotion(Emotion.HAPPY)
        self.arduino.play_animation("NOD")
        

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
      • Wrong/timeout → SAD eyes, retry same round (up to max_retries times).
      • All retries exhausted → session ends, returns False.
      • All 5 rounds correct → transition animation begins.

    TRANSITION (phase 1 → phase 2):
      1. All four colors cycle twice quickly (each for 0.15 s) — "level up" signal.
      2. SURPRISED eyes for 1.0 s  →  primes child's attention for the new challenge.

    PHASE 2 — Sequence Repeat (3 rounds, starting with 3-color sequences):
      See SequenceGame docstring for per-round detail.
      • Wrong/timeout → SAD eyes, same sequence replayed (up to max_retries times).
      • All retries exhausted → session ends, returns False.
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
