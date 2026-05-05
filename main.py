import os
from google.cloud import dialogflow
import numpy as np
import time
import serial
from interaction.state import State, execute_state

# Check which COM you are conected to in the arduino IDE
# arduino = serial.Serial(port='COM7', baudrate=115200, timeout=.1)

PROJECT_ID = 'project-631e036d-75af-4f9e-b4b'
SESSION_ID = '3'
LANGUAGE_CODE = "en-GB"

credential_path = (r'project-631e036d-75af-4f9e-b4b-44751f4edea7.json')

os.environ['GOOGLE_APPLICATION_CREDENTIALS'] = credential_path

# def arduino_write_read(x):
#     arduino.write(bytes(x, 'utf-8'))
#     time.sleep(0.05)
#     data = arduino.readline()
#     return data


if __name__ == '__main__':
    state = State.IDLE
    while(True):
        state = execute_state(state, PROJECT_ID, SESSION_ID, LANGUAGE_CODE)
                
        # Commands should be formatted as E.G.: "EMOTION,SAD;SERVO,10;."
        # Input_text = input("Enter a command to send to the arduino: ")  # Taking input from user
        # value = arduino_write_read(Input_text)
        # print(value)  # printing the output value from the arduino
