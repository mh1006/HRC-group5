"""
ArduinoController
=================
Matches the protocol already implemented in the Arduino sketch:

    Commands are sent as:  "TYPE,value;TYPE,value;."
    Arduino replies with:  "Setting <type> to: <value>"

You can chain multiple commands in one transmission:
    send("EMOTION,HAPPY;SERVO,(90,30);")  →  "EMOTION,HAPPY;SERVO,(90,30);."

Supported command types (from the sketch's communication() function):
    EMOTION  →  NEUTRAL | HAPPY | SAD | ANGRY | SURPRISED
    EYES     →  (reserved in sketch, not yet handled, but forwarded)
    SERVO    →  (horizontal, vertical) tuple — e.g. SERVO,(90,30);
"""

import serial
import threading
import time
import json
from queue import Queue, Empty
from enum import Enum
import json


# ── Mirror the Arduino enums so Python stays in sync ──────────────────────────

class Emotion(str, Enum):
    NEUTRAL   = "NEUTRAL"
    HAPPY     = "HAPPY"
    SAD       = "SAD"
    ANGRY     = "ANGRY"
    SURPRISED = "SURPRISED"   # sketch maps "SURPRISED" → SUPRISED internally


class CommandType(str, Enum):
    EMOTION    = "EMOTION"
    EYES       = "EYES"
    SERVO      = "SERVO"
    EYES_COLOR = "EYES_COLOR"  # game color display: RED | BLUE | GREEN | YELLOW


# ── Controller ─────────────────────────────────────────────────────────────────

class ArduinoController:
    """
    Sends commands to the robot Arduino and reads back its log lines.

    Usage
    -----
        arduino = ArduinoController(port="/dev/ttyUSB0")
        arduino.set_emotion(Emotion.HAPPY)
        arduino.send_raw("EMOTION,SAD;SERVO,90;")   # chain manually if needed
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
        self.send((CommandType.EMOTION, emotion.value))

    def set_game_color(self, color: str):
        """
        Show a solid color on the robot's eyes during the color game.
        color: "RED" | "BLUE" | "GREEN" | "YELLOW"
        Arduino must handle EYES_COLOR in its communication() function.
        """
        self.send((CommandType.EYES_COLOR, color))

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