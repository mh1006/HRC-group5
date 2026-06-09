import serial
import time

class Arduino:
    def __init__(self, port, baudrate):
        self.arduino = serial.Serial(port=port, baudrate=baudrate, timeout=.1)

    def write_read(self, input):
        self.arduino.write(bytes(input, 'utf-8'))
        time.sleep(0.05)
        data = self.arduino.readline()
        return data

    def read(self):
        data = self.arduino.readline()
        if data:
            return data
        return None