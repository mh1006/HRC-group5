from enum import Enum
import time
from interaction.intent import Intent, detect_intent_mic

class State(Enum):
    """
    These are the possible states of the robot.
    """
    IDLE = "IDLE"            # attract attention
    LISTENING = "LISTENING"  # detect first speech
    ENGAGED = "ENGAGED"      # child responded
    PLAYING = "PLAYING"      # game started
    CALMING = "CALMING"      # calm child if stress detected
    ERROR = "ERROR"          # fallback
    
def handle_intent(intent):
    match intent:
        case Intent.ENGAGEMENT_INTENT.value:
            return State.ENGAGED
        case Intent.PLAY_INTENT.value:
            return State.PLAYING
        case Intent.DISTRESS_INTENT.value:
            return State.CALMING
        case Intent.FALLBACK_INTENT.value:
            return State.ERROR
        
def execute_state(state, project_id, session_id, language_code):
    match state:
            case State.IDLE:
                print("Entered IDLE state!")
                # TODO: detect if someone is passing by?
                # TODO: play sound to attract attention
                time.sleep(3)
                return State.LISTENING

            case State.LISTENING:
                print("Entered LISTENING state!")
                result = detect_intent_mic(project_id, session_id, language_code)
                return handle_intent(result["intent_name"])
            
            case State.ENGAGED:
                print("Entered ENGAGED state!")
                # TODO: play sound
                return State.LISTENING
                
            case State.PLAYING:
                print("Entered PLAYING state!")
                # TODO: play sound?
                # TODO: init game here/send to arduino
                return State.IDLE
                
            case State.CALMING:
                print("Entered CALMING state!")
                #TODO: play sound
                #TODO: game?/send to arduino
                return State.IDLE
                
            case State.ERROR:
                print("Entered ERROR state!")
                print("Retrying...")
                return State.LISTENING
            
            case _:
                return State.IDLE
    
