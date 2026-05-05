from enum import Enum
from google.cloud import dialogflow
import sounddevice as sd

class Intent(Enum):
    ENGAGEMENT_INTENT = "EngagementIntent"   # kid noticed a robot / reacts / talks
    PLAY_INTENT = "PlayIntent"               # kid wants to play
    DISTRESS_INTENT = "DistressIntent"       # kid is upset 
    FALLBACK_INTENT = "FallbackIntent"
    
     
def detect_intent_mic(project_id, session_id, language_code):
    """
    Returns recognized intent from audio input
    """
    session_client = dialogflow.SessionsClient()

    audio_encoding = dialogflow.AudioEncoding.AUDIO_ENCODING_LINEAR_16
    sample_rate_hertz = 16000

    session = session_client.session_path(project_id, session_id)

    # Get audio directly from mic
    input_audio = record_audio(duration=5)

    audio_config = dialogflow.InputAudioConfig(
        audio_encoding=audio_encoding,
        language_code=language_code,
        sample_rate_hertz=sample_rate_hertz,
    )

    query_input = dialogflow.QueryInput(audio_config=audio_config)

    request = dialogflow.DetectIntentRequest(
        session=session,
        query_input=query_input,
        input_audio=input_audio,
    )

    response = session_client.detect_intent(request=request)

    print("=" * 20)
    print("Query text:", response.query_result.query_text)
    print("Detected intent:", response.query_result.intent.display_name)
    print("Confidence:", response.query_result.intent_detection_confidence)
    print("Response:", response.query_result.fulfillment_text)
    
    return {"intent_name": response.query_result.intent.display_name,
            "robot_response": response.query_result.fulfillment_text}
    
def record_audio(duration=5, sample_rate=16000):
    print("Recording...")

    audio = sd.rec(
        int(duration * sample_rate),
        samplerate=sample_rate,
        channels=1,
        dtype='int16'
    )

    sd.wait()

    print("Done recording!")

    return audio.tobytes()