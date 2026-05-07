from enum import Enum
import time
from interaction.intent import Intent
from interaction.dialogflow_handler import trigger_event, detect_intent_mic
from playsound import playsound

# check if silent
retries = 0

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
       
def execute_state(state, project_id, session_id, language_code):
    global retries
    match state:
            case State.IDLE:
                print("Entered IDLE state!")
                # TODO: detect if someone is passing by?
                approaches = True
                if approaches:
                    time.sleep(3)
                    return State.ATTRACTING
                
            case State.ATTRACTING:
                print("Entered Attracting state")
                playsound('sounds/chameleon_sound.mp3')
                return State.LISTENING
            
            case State.LISTENING:
                result = detect_intent_mic(project_id, session_id, language_code)
                intent_name = result["intent_name"]
                
                if not intent_name:
                    return State.ERROR  # or State.LISTENING to retry
                
                try:
                    intent = Intent(intent_name)
                    return intent.to_state()
                except ValueError:
                    print(f"Unknown intent: '{intent_name}'")
                    return State.ERROR
            
            case State.ENGAGED:
                print("Entered ENGAGED state!")
                retries = 0
                # TODO: play sound
                return State.LISTENING
                
            case State.PLAYING:
                print("Entered PLAYING state!")
                retries = 0
                # TODO: play sound?
                # TODO: init game here/send to arduino
                return State.IDLE
                
            case State.CALMING:
                print("Entered CALMING state!")
                retries = 0
                #TODO: play sound
                #TODO: game?/send to arduino
                return State.IDLE
                
            case State.ERROR:
                retries += 1
                if retries >=5:
                    print("Interaction ended")
                    retries = 0
                    return State.IDLE
                print("Entered ERROR state!")
                print("Retrying...")
                
                return State.LISTENING
            
            case _:
                return State.IDLE
    
