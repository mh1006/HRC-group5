int x;
long timer;

bool updating_actuators;

// Libraries
#include <Adafruit_NeoPixel.h>

// Constants
#define LED_PIN       4
#define NUMPIXELS    74

// Variables
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN);

int LED_BRIGHTNESS = 8;

enum Emotion {NEUTRAL, SUPRISED, HAPPY, ANGRY, SAD};
Emotion emotion = NEUTRAL;


// We store the different eyes as a byte array
// You can use https://sjoerd.tech/eyes/ to design more eye patterns

byte neutral[] = {
  B0000,
  B01110,
  B011110,
  B0111110,
  B011110,
  B01110,
  B0000
};

byte blink1[] = {
  B0000,
  B00000,
  B011110,
  B0111110,
  B011110,
  B00000,
  B0000
};

byte blink2[] = {
  B0000,
  B00000,
  B000000,
  B1111111,
  B000000,
  B00000,
  B0000
};

byte suprised[] = {
  B1111,
  B11111,
  B111111,
  B1111111,
  B111111,
  B11111,
  B1111
};

byte happy[] = {
  B1111,
  B11111,
  B111111,
  B1100011,
  B000000,
  B00000,
  B0000
};

byte angry[] = {
  B0000,
  B10000,
  B110000,
  B1111000,
  B111110,
  B11111,
  B1111
};

byte sad[] = {
  B0000,
  B00001,
  B000011,
  B0001111,
  B011111,
  B11111,
  B1111
};

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(1);

  // Initialize the leds
  pixels.begin();

  updating_actuators = true;
}

void  loop() {

   // Every 10 milliseconds, update the commication with the PC
  if (millis() - timer >= 10){
    timer = millis();
    communication();
    if(updating_actuators){
      update_actuators();
    }
    updating_actuators = !updating_actuators;
  }
}

void update_actuators() {
  run_emotions();
}

// ============================================
// ----------------- Emotions -----------------
// ============================================

void run_emotions(){
  pixels.clear();  

  switch (emotion) {
    case NEUTRAL:
      if (millis() % 5000 < 150) display_eyes(blink1, 125);
      else if (millis() % 5000 < 300) display_eyes(blink2, 125);
      else if (millis() % 5000 < 450) display_eyes(blink1, 125);
      else display_eyes(neutral, 125);

      // if (face_detected) {
      //   servo1_target = 90.0 + float(face.xCenter - 160) / 320.00 * -50.00;
      //   servo2_target = 90.0 + float(face.yCenter - 120) / 240.00 * 50.00;
      // }
      break;
    case HAPPY:
      display_eyes(happy, 80);
      
      // servo1_target = 90 + 10.0 * sin(millis() / 500.00);
      // servo2_target = 80 + 15.0 * cos(millis() / 400.00);
      break;
    case SAD:
      display_eyes(sad, 150);
      
      // servo1_target = 90 + 3.0 * sin(millis() / 400.00);
      // servo2_target = 120 + 20.0 * cos(millis() / 500.00);
      break;
    case ANGRY:
      display_eyes(angry, 0);

      // servo1_target = 90 + 10.0 * sin(millis() / 250.00);
      // servo2_target = 110 + 15.0 * cos(millis() / 175.00);
      break;
    case SUPRISED:
      display_eyes(suprised, 125);

      // servo1_target = 90;
      // servo2_target = 80 + 10.0 * cos(millis() / 500.00);
      break;
  }

  pixels.show();
}

// ============================================
// ------------------- Eyes -------------------
// ============================================

void display_eyes(byte arr[], int hue){
   display_eye(arr, hue, true);
   display_eye(arr, hue, false);
}

void display_eye(byte arr[], int hue, bool left) {
  // We will draw a circle on the display
  // It is a hexagonal matrix, which means we have to do some math to know where each pixel is on the screen

  int rows[] = {4, 5, 6, 7, 6, 5, 4};      // The matrix has 4, 5, 6, 7, 6, 5, 4 rows.
  int NUM_COLUMNS = 7;                     // There are 7 columns
  int index = (left) ? 0 : 37;             // If we draw the left eye, we have to add an offset of 37 (4+5+6+7+6=5+4)
  for (int i = 0; i < NUM_COLUMNS; i++) {
    for (int j = 0; j < rows[i]; j++) {
      int brightness = LED_BRIGHTNESS * bitRead(arr[i], (left) ? rows[i] - 1 - j : j);
      pixels.setPixelColor(index, pixels.ColorHSV(hue * 256, 255, brightness));
      index ++;
    }
  }
}

// ============================================
// ----------- Python communication -----------
// ============================================

void communication() {
  char val = ' ';
  String data = "";
  if (Serial.available()) {
    do {
      val = Serial.read();
      if (val != -1) data = data + val;
    }
    while ( val != -1);
  }

  // Data is a string of what we received, we will split it into the different values
  // Each command is sent as "command_type", command; The final command in the data ends in "." to indicate the end of the command chain.
  // Example of what the data could look like: "SERVO,10;EYES,sad;."
  if (data.length() > 1 && data.charAt(data.length() - 1) == '.') {
    // Send the received data back for debugging
    // Serial.print("Received: " + data);

    String command_pair;
    String type;
    String value;
    for (int i = 0; data.length() > 1; i++){
      command_pair = data.substring(0, data.indexOf(';'));
      data = data.substring(data.indexOf(';') + 1, data.length());
      
      type = command_pair.substring(0,command_pair.indexOf(','));
      value = command_pair.substring(command_pair.indexOf(',') + 1,command_pair.indexOf(';'));

      if (type == "SERVO") Serial.print("Setting servo to: " + value + "\n");
      if (type == "EYES") Serial.print("Setting eyes to: " + value + "\n");
      if (type == "EMOTION") {
        Serial.print("Setting emotion to: " + value + "\n");
        if(value == "NEUTRAL") emotion = NEUTRAL;
        else if(value == "SURPRISED") emotion = SUPRISED;
        else if(value == "HAPPY") emotion = HAPPY;
        else if(value == "ANGRY") emotion = ANGRY;
        else emotion = SAD;
        }
      }
    }
}