from google.cloud import dialogflow
import sounddevice as sd

def trigger_event(project_id, session_id, event_name):
    session_client = dialogflow.SessionsClient()
    session = session_client.session_path(project_id, session_id)

    event_input = dialogflow.EventInput(name=event_name, language_code="en")

    query_input = dialogflow.QueryInput(event=event_input)

    response = session_client.detect_intent(
        request={"session": session, "query_input": query_input}
    )

    print("Triggered intent:", response.query_result.intent.display_name)
    print("Response:", response.query_result.fulfillment_text)

    return response

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

    try:
        response = session_client.detect_intent(request=request)
    except Exception as e:
        print(f"Dialogflow error: {e}")
        return {"intent_name": None, "user_input": None, "robot_response": None}

    print("=" * 20)
    print("Query text:", response.query_result.query_text)
    print("Detected intent:", response.query_result.intent.display_name)
    print("Confidence:", response.query_result.intent_detection_confidence)
    print("Response:", response.query_result.fulfillment_text)

    return {"intent_name": response.query_result.intent.display_name,
            "user_input": response.query_result.query_text,
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

