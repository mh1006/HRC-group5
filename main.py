import os
from google.cloud import dialogflow
import sounddevice as sd
import numpy as np

credential_path = (r'project-631e036d-75af-4f9e-b4b-44751f4edea7.json')
os.environ['GOOGLE_APPLICATION_CREDENTIALS'] = credential_path

def detect_intent_texts(project_id, session_id, texts, language_code):
    """Returns the result of detect intent with texts as inputs.

    Using the same `session_id` between requests allows continuation
    of the conversation."""

    session_client = dialogflow.SessionsClient()

    session = session_client.session_path(project_id, session_id)
    print("Session path: {}\n".format(session))

    for text in texts:
        text_input = dialogflow.TextInput(text=text, language_code=language_code)

        query_input = dialogflow.QueryInput(text=text_input)

        response = session_client.detect_intent(
            request={"session": session, "query_input": query_input}
        )

        print("=" * 20)
        print("Query text: {}".format(response.query_result.query_text))
        print(
            "Detected intent: {} (confidence: {})\n".format(
                response.query_result.intent.display_name,
                response.query_result.intent_detection_confidence,
            )
        )
        print("Fulfillment text: {}\n".format(response.query_result.fulfillment_text))

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
     
def detect_intent_mic(project_id, session_id, language_code):
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

if __name__ == '__main__':
    while(True):
     detect_intent_mic('project-631e036d-75af-4f9e-b4b','3', "en-GB")

