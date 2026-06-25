"""
ArduinoController
=================
Matches the protocol already implemented in the Arduino sketch:

    Commands are sent as:  "TYPE,value;TYPE,value;."
    Arduino replies with:  "Setting <type> to: <value>"

You can chain multiple commands in one transmission:
    send("EYES,HAPPY;SERVO,(90,30);")  →  "EYES,HAPPY;SERVO,(90,30);."

Supported command types (from the sketch's communication() function):
    EYES     →  NEUTRAL | HAPPY | SAD | ANGRY | SURPRISED
    SERVO    →  (horizontal, vertical) tuple — e.g. SERVO,(90,30);
"""

import re
import serial
import threading
import time
import json
from queue import Queue, Empty
from enum import Enum


# ── Mirror the Arduino enums so Python stays in sync ──────────────────────────

class Emotion(str, Enum):
    NEUTRAL   = "NEUTRAL"
    HAPPY     = "HAPPY"
    SAD       = "SAD"
    ANGRY     = "ANGRY"
    SURPRISED = "SURPRISED"   # sketch maps "SURPRISED" → SUPRISED internally


class CommandType(str, Enum):
    EYES    = "EYES"
    SERVO   = "SERVO"
    BUTTONS = "BUTTONS"  # game color display: 3 chars, one per eye matrix + LCD
    AUDIO   = "AUDIO"


# Maps a game color name to the single-char code Arduino's char_to_color() expects
# (see util.h::char_to_color() — any other char is treated as "off")
_COLOR_CHARS = {
    "RED":    "R",
    "GREEN":  "G",
    "BLUE":   "B",
    "YELLOW": "Y",
    "CYAN":   "C",
    "PURPLE": "P",
    "WHITE":  "W",
    "OFF":    "O",
}


# ── Controller ─────────────────────────────────────────────────────────────────

class ArduinoController:
    """
    Sends commands to the robot Arduino and reads back its log lines.

    Usage
    -----
        arduino = ArduinoController(port="/dev/ttyUSB0")
        arduino.set_emotion(Emotion.HAPPY)
        arduino.send_raw("EYES,SAD;SERVO,90;")   # chain manually if needed
        arduino.close()
    """

    def __init__(self, port: str = "/dev/ttyUSB0", baud: int = 115200, timeout: float = 2.0):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self._running = True
        self._log_queue: Queue[str] = Queue()

        # Background thread — reads every line the Arduino prints
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

        # Arduino resets when serial opens; wait for it to boot
        time.sleep(2)
        print(f"[ArduinoController] Connected on {port} @ {baud} baud")

    # ── Internal ───────────────────────────────────────────────────────────────

    def _read_loop(self):
        """Continuously read lines from Arduino and push to the log queue."""
        while self._running:
            try:
                raw = self.ser.readline()
                if raw:
                    line = raw.decode("utf-8", errors="replace").strip()
                    if line:
                        self._log_queue.put(line)
                        try:
                            response_json = json.loads(line)
                            print(f"[Detected json] {response_json}")
                        except ValueError:
                            print(f"[Arduino] {line}")
            except Exception:
                pass

    def _build_packet(self, *command_pairs: tuple[str, str]) -> bytes:
        """
        Build a packet in the sketch's expected format:
            "TYPE,value;TYPE,value;."
        Each pair is (CommandType, value_string).
        """
        body = "".join(f"{t},{v};" for t, v in command_pairs)
        packet = body + "."
        return packet.encode("utf-8")

    def send_raw(self, raw: str):
        """
        Send a pre-formatted command string.
        Must end with '.' — e.g. "EMOTION,HAPPY;SERVO,(90,90);."
        If the trailing '.' is missing it is added automatically.
        """
        print(raw)
        if not raw.endswith("."):
            raw += "."
        self.ser.write(raw.encode("utf-8"))

    def send(self, *command_pairs: tuple[str, str]):
        """
        Low-level send of one or more (type, value) pairs in one packet.

        Example:
            arduino.send(("EMOTION", "HAPPY"), ("SERVO", "90"))
            # sends: "EMOTION,HAPPY;SERVO,90;."
        """
        packet = self._build_packet(*command_pairs)
        self.ser.write(packet)

    # ── High-level API — called by state machine ──────────────────────────

    def set_emotion(self, emotion: Emotion):
        """
        Set the robot's facial emotion.
        Maps to the Arduino's `emotion` global and drives eye patterns + servos.
        """
        self.send((CommandType.EYES, emotion.value))

    def set_game_color(self, color: str):
        """
        Show one solid color across both eye matrices and the LCD (e.g. as a
        "watch this color" cue, or to clear all three with "OFF").
        color: one of _COLOR_CHARS' keys, e.g. "RED" | "CYAN" | "OFF"
        """
        char = _COLOR_CHARS[color.upper()]
        self.send((CommandType.BUTTONS, char * 3))

    def set_button_colors(self, colors: list[str]):
        """
        Assign a different color to each of the 3 physical buttons (2 eye
        matrices + LCD — see buttons.h::display_button_colors()). This is
        how the color game rotates which button means which color each round.
        colors: list of exactly 3 color names, e.g. ["RED", "CYAN", "WHITE"]
        """
        chars = "".join(_COLOR_CHARS[c.upper()] for c in colors)
        self.send((CommandType.BUTTONS, chars))

    def set_tracking(self, enabled: bool):
        """Enable or disable face-tracking servos on the Arduino."""
        self.send(("TRACKING", "ON" if enabled else "OFF"))

    def set_servo(self, horizontal: int, vertical: int):
        """
        Send a SERVO command to set both axes at once.
        horizontal: 0–180 (left/right pan, SERVO_PIN_1)
        vertical:   0–180 (up/down tilt, SERVO_PIN_2)
        Arduino parses the tuple format: SERVO,(h,v);
        """
        self.send((CommandType.SERVO, f"({horizontal},{vertical})"))

    def set_audio(self, audio_index: int):
        """
        Send a SERVO command to set both axes at once.
        horizontal: 0–180 (left/right pan, SERVO_PIN_1)
        vertical:   0–180 (up/down tilt, SERVO_PIN_2)
        Arduino parses the tuple format: SERVO,(h,v);
        """
        self.send((CommandType.AUDIO, str(audio_index)))

    def chain(self, **kwargs):
        """
        Send multiple commands in one packet.

        Keyword args map command type → value:
            arduino.chain(emotion="HAPPY", servo="90")
            # sends: "EMOTION,HAPPY;SERVO,90;."

        Keys are case-insensitive and uppercased automatically.
        """
        pairs = [(k.upper(), str(v)) for k, v in kwargs.items()]
        self.send(*pairs)
        
    def parse_face(self, line: str) -> dict | None:
        """Parse a face JSON line from the Arduino. Returns the face dict or None."""
        try:
            return json.loads(line).get("detected_face")
        except (json.JSONDecodeError, AttributeError):
            return None

    def parse_button(self, line: str) -> int | None:
        """
        Parse a button-press JSON line from the Arduino.
        Returns the pressed button's index (0, 1, or 2), or None if this
        line carries no button press. Falls back to regex if JSON is garbled.
        """
        try:
            return json.loads(line).get("pressed_button")
        except (json.JSONDecodeError, AttributeError):
            pass
        # Fallback: extract digit from garbled JSON if it looks like a button press
        if "pressed" in line or "button" in line or ("button" in line.lower()):
            match = re.search(r'([012])\s*}?\s*$', line)
            if match:
                return int(match.group(1))
        return None

    # ── Convenience wrappers for each state in your state machine ──────────────

    def on_attracting(self):
        """Robot notices someone — show SURPRISED, look alert."""
        self.set_emotion(Emotion.SURPRISED)
        self.set_tracking(True)

    def on_engaged(self):
        """Child responded — switch to HAPPY."""
        self.set_emotion(Emotion.HAPPY)
        self.set_tracking(True)

    def on_playing(self):
        """Game started — stay HAPPY (tracking + dancing handled on Arduino)."""
        self.set_emotion(Emotion.HAPPY)
        self.set_tracking(False)

    def on_calming(self):
        """Stress detected — switch to NEUTRAL/SAD to mirror calm."""
        self.set_emotion(Emotion.SAD)
        self.set_tracking(True)
        self.set_audio(1)

    def on_idle(self):
        """Return to NEUTRAL — Arduino will auto-blink and wait."""
        self.set_emotion(Emotion.NEUTRAL)
        self.set_tracking(False)

    def on_error(self):
        """Something went wrong — ANGRY face as a visual cue."""
        self.set_emotion(Emotion.ANGRY)
        self.set_tracking(False)

    # ── Utility ────────────────────────────────────────────────────────────────

    def drain_log(self) -> list[str]:
        """Return all unread Arduino log lines (non-blocking)."""
        lines = []
        while True:
            try:
                lines.append(self._log_queue.get_nowait())
            except Empty:
                break
        return lines

    def close(self):
        self._running = False
        self.ser.close()
        print("[ArduinoController] Serial connection closed.")