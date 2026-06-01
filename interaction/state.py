from enum import Enum
import time
from interaction.intent import Intent
from interaction.dialogflow_handler import trigger_event, detect_intent_mic
from playsound import playsound
from hardware.arduino_controller import ArduinoController
from hardware.game_handler import FullGame

# check if silent
retries = 0
engagement_count = 0

# ── Init Arduino once at module level ─────────────────────────────────────────
# Change port to match your system:
#   Windows → "COM3"
#   Linux   → "/dev/ttyUSB0" or "/dev/ttyACM0"
#   Mac     → "/dev/tty.usbmodemXXXX"
# arduino = ArduinoController(port="/dev/ttyUSB0", baud=115200)

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
       
def execute_state(state, project_id, session_id, language_code, arduino):
    global retries, engagement_count
    match state:
            case State.IDLE:
                print("Entered IDLE state!")
                # TODO: notify from arduino when someone is approaching
                
                # TODO: uncomment when arduino is ready
                # arduino.on_idle()
                # for line in arduino.drain_log():
                #     if line == "BEHAVIOR,APPROACHING":
                #         return State.ATTRACTING
                # time.sleep(0.1)
                # return State.IDLE
                
                # TODO: delete when arduino is ready
                approaches = True
                if approaches:
                    time.sleep(3)
                    return State.ATTRACTING
                
            case State.ATTRACTING:
                print("Entered Attracting state")
                # TODO: uncomment when arduino is ready
                # arduino.on_attracting()
                playsound('sounds/chameleon_sound.mp3')
                return State.LISTENING
            
            case State.LISTENING:
                result = detect_intent_mic(project_id, session_id, language_code)
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
                retries = 0
                engagement_count += 1  # track how many times child has responded
                # TODO: uncomment when arduino is ready
                # arduino.on_engaged()        # HAPPY eyes + face tracking + dance
                # TODO: play sound
                if engagement_count >= 2:
                    engagement_count = 0
                    return State.PLAYING   # they're engaged enough, start the game
                return State.LISTENING
                
            case State.PLAYING:
                print("Entered PLAYING state!")
                retries = 0
                # TODO: uncomment when arduino is ready
                # arduino.on_playing()
                # won = FullGame(arduino).run()
                # Uncomment the two lines above once arduino is initialised
                return State.IDLE
                
            case State.CALMING:
                print("Entered CALMING state!")
                retries = 0
                # TODO: uncomment when arduino is ready
                # arduino.on_calming()        # SAD eyes (mirrors calm)
                #TODO: play sound
                #TODO: game?/send to arduino
                return State.ATTRACTING
                
            case State.ERROR:
                print("Entered ERROR state!")
                retries += 1
                if retries >=5:
                    print("Interaction ended")
                    # TODO: uncomment when arduino is ready
                    # arduino.on_idle()
                    retries = 0
                    return State.IDLE
                # TODO: uncomment when arduino is ready
                # arduino.on_error()          # ANGRY eyes?

                print("Retrying...")
                
                return State.LISTENING
            
            case _:
                return State.IDLE
    
