from enum import Enum
import time
from python.interaction.intent import Intent
from python.interaction.dialogflow_handler import trigger_event, detect_intent_mic
from playsound import playsound
from python.hardware.arduino_controller import ArduinoController
from python.hardware.game_handler import FullGame


class State(Enum):
    """
    These are the possible states of the robot.
    """
    IDLE = "IDLE"            # attract attention
    LISTENING = "LISTENING"  # detect first speech
    ATTRACTING = "ATTRACTING"
    ENGAGED = "ENGAGED"      # child responded
    PLAYING = "PLAYING"      # game started
    CALMING = "CALMING"      # calm child if stress detected
    ERROR = "ERROR"          # fallback


# Face bounding-box width (pixels) at which a passerby is considered "close".
# HuskyLens resolution is 320×240; tune this based on the robot's physical setup.
# TODO: probably needs tuning 
FACE_PROXIMITY_THRESHOLD = 80


class StateMachine:
    def __init__(self, project_id, session_id, language_code, arduino):
        self.project_id = project_id
        self.session_id = session_id
        self.language_code = language_code
        self.arduino = arduino
        self.retries = 0
        self.game_retries = 0
        self.engagement_count = 0

    def execute(self, state) -> State:
        match state:
            case State.IDLE:
                print("Entered IDLE state!")
                self.engagement_count = 0
                self.arduino.on_idle()
                for line in self.arduino.drain_log():
                    face = self.arduino.parse_face(line)
                    if face and face["width"] >= FACE_PROXIMITY_THRESHOLD:
                        return State.ATTRACTING
                time.sleep(0.1)
                return State.IDLE

            case State.ATTRACTING:
                print("Entered Attracting state")
                self.arduino.on_attracting()
                playsound('sounds/chameleon_sound.mp3')
                return State.LISTENING

            case State.LISTENING:
                result = detect_intent_mic(self.project_id, self.session_id, self.language_code)
                intent_name = result["intent_name"]

                if not intent_name:
                    time.sleep(1)
                    return State.LISTENING

                try:
                    intent = Intent(intent_name)
                    # if child says something but input not recognized as any of the intents, assume that engaged
                    if intent == Intent.FALLBACK_INTENT and result["user_input"]:
                        return State.ENGAGED
                    return intent.to_state()
                except ValueError:
                    print(f"Unknown intent: '{intent_name}'")
                    return State.ERROR

            case State.ENGAGED:
                print("Entered ENGAGED state!")
                self.retries = 0
                self.engagement_count += 1  # track how many times child has responded
                self.arduino.on_engaged()        # HAPPY eyes + face tracking 
                # TODO: play sound
                if self.engagement_count >= 2:
                    self.engagement_count = 0
                    return State.PLAYING   # they're engaged enough, start the game
                return State.LISTENING

            case State.PLAYING:
                print("Entered PLAYING state!")
                self.retries = 0
                self.arduino.on_playing()
                won = FullGame(self.arduino).run()
                if won:
                    print("CONGRATS! YOU WON!")
                    return state.IDLE
                else:
                    print("YOU LOST!")
                    return state.CALMING

            case State.CALMING:
                print("Entered CALMING state!")
                self.retries = 0
                self.arduino.on_calming()        # SAD eyes (mirrors calm)
                # TODO: play sound
                # TODO: game?/send to arduino
                return State.ATTRACTING

            case State.ERROR:
                print("Entered ERROR state!")
                self.retries += 1
                if self.retries >= 5:
                    print("Interaction ended")
                    self.retries = 0
                    return State.IDLE
                self.arduino.on_error()          # ANGRY eyes?
                print("Retrying...")
                return State.LISTENING

            case _:
                return State.IDLE
