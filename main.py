import os
from google.cloud import dialogflow
import numpy as np
import serial
from hardware.arduino import Arduino
from hardware.arduino_controller import ArduinoController
from interaction.state import State, execute_state

# Check which COM you are conected to in the arduino IDE
# TODO: uncomment and set correct port when arduino is ready
# arduino_controller = ArduinoController(port="/dev/tty.usbmodemXXXX", baud=115200)
arduino_controller = None
arduino = Arduino('COM4', 115200) #TODO: remove once arduino_controller is used everywhere

PROJECT_ID = 'project-631e036d-75af-4f9e-b4b'
SESSION_ID = '3'
LANGUAGE_CODE = "en-GB"

credential_path = (r'project-631e036d-75af-4f9e-b4b-44751f4edea7.json')

os.environ['GOOGLE_APPLICATION_CREDENTIALS'] = credential_path


if __name__ == '__main__':
    # state = State.IDLE
    # try:
    #     while True:
    #         state = execute_state(state, PROJECT_ID, SESSION_ID, LANGUAGE_CODE, arduino_controller)
    # except KeyboardInterrupt:
    #     print("\nShutting down.")
    #
    #     # Commands should be formatted as E.G.: "EMOTION,SAD;SERVO,10;."
    #     Input_text = input("Enter a command to send to the arduino: ")  # Taking input from user
    #     value = arduino.write_read(Input_text)
    #     print(value)  # printing the output value from the arduino
    previous_message = ""
    while True:
        arduino_message = arduino.read()
        if arduino_message and ( arduino_message != previous_message ) :
            print(arduino_message)
            previous_message = arduino_message