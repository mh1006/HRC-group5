/*  Example code for the Social Robot Interaction course
    Code Written by Sjoerd de Jong

    Connections:
    MP3 MODULE    -> D2
    LED EYES      -> D3
    SERVOS        -> D6
    TOUCH_SENSOR  -> A0
    HUSKY_LENS    -> I2C
*/

// --------------------------------------------------------------------------------- //
// ----------------------------------- VARIABLES ----------------------------------- //
// --------------------------------------------------------------------------------- //
// Let's start by including the needed libraries
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <MP3Player_KT403A.h>
#include "HUSKYLENS.h"
#include <Servo.h> 

// Then we define global constants
#define LED_PIN       4
#define NUMPIXELS    74
#define TOUCH_PIN     A0
#define SERVO_PIN_1   6
#define SERVO_PIN_2   7

// And the rest
SoftwareSerial mp3(2, 3);                     // The MP3 module is connected on pins 2 and 3
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN);

int LED_BRIGHTNESS = 8;  // 0-255

HUSKYLENS huskylens;
HUSKYLENSResult face;
bool face_detected = false;
bool prev_touch_value = 0;

enum Emotion {NEUTRAL, SUPRISED, HAPPY, ANGRY, SAD};
Emotion emotion = NEUTRAL;

Servo servo1, servo2;
float servo1_pos = 90, servo2_pos = 90;
float servo1_target = 90, servo2_target = 90;
float servo1_speed = 0, servo2_speed = 0;

long timer1, timer2, timer3;

bool pc_connected = false;
float servo1_target_pc = 90, servo2_target_pc = 90;

// --------------------------------------------------------------------------------- //
// ---------------------------------- EYE PATTERNS --------------------------------- //
// --------------------------------------------------------------------------------- //
// We store the different eyes as a byte array, which is both space-efficient and readable
// You can also use https://sjoerd.tech/eyes/ to quickly design your own eye patterns
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

// --------------------------------------------------------------------------------- //
// ------------------------------------- SETUP ------------------------------------- //
// --------------------------------------------------------------------------------- //
void setup() {
  // put your setup code here, to run once:
  pinMode(TOUCH_PIN, INPUT);

  // Initialize the leds
  pixels.begin();

  // Serial communication
  Serial.begin(115200);

  // MP3
  mp3.begin(9600);
  delay(100); // Wait 0.1 seconds for the MP3 player to boot up
  SelectPlayerDevice(0x02);       // Select SD card as the player device.
  SetVolume(0x1E);                // Set the volume, the range is 0x00 to 0x1E.

  // HuskyLens
  Wire.begin();
  while (!huskylens.begin(Wire)) {
      Serial.println(F("Begin failed!"));
      Serial.println(F("1.Please recheck the \"Protocol Type\" in HUSKYLENS (General Settings>>Protocol Type>>I2C)"));
      Serial.println(F("2.Please recheck the connection."));
      delay(100);
  }

  // Servos
  servo1.attach(SERVO_PIN_1);
  servo2.attach(SERVO_PIN_2);
  servo1.write(90);
  servo2.write(90);
}

// --------------------------------------------------------------------------------- //
// ----------------------------------- MAIN LOOP ----------------------------------- //
// --------------------------------------------------------------------------------- //
void loop() {
  // put your main code here, to run repeatedly:

  // Ever 20 milliseconds, update the servos
  if (millis() - timer1 >= 20){
    timer1 = millis();
    move_servos();
    husky_lens();
    touch_sensor();
    run_emotions();
  }

  // Every 10 milliseconds, update the huskylens and touch sensor
  if (millis() - timer2 >= 10){
    timer2 = millis();
    communication();
  }
}

// --------------------------------------------------------------------------------- //
// ---------------------------------- TOUCH SENSOR --------------------------------- //
// --------------------------------------------------------------------------------- //
void run_emotions(){
  pixels.clear();  

  switch (emotion) {
    case NEUTRAL:
      if (millis() % 5000 < 150) display_eyes(blink1, 125);
      else if (millis() % 5000 < 300) display_eyes(blink2, 125);
      else if (millis() % 5000 < 450) display_eyes(blink1, 125);
      else display_eyes(neutral, 125);

      if (face_detected) {
        servo1_target = 90.0 + float(face.xCenter - 160) / 320.00 * -50.00;
        servo2_target = 90.0 + float(face.yCenter - 120) / 240.00 * 50.00;
      }
      break;
    case HAPPY:
      display_eyes(happy, 80);
      
      servo1_target = 90 + 10.0 * sin(millis() / 500.00);
      servo2_target = 80 + 15.0 * cos(millis() / 400.00);
      break;
    case SAD:
      display_eyes(sad, 150);
      
      servo1_target = 90 + 3.0 * sin(millis() / 400.00);
      servo2_target = 120 + 20.0 * cos(millis() / 500.00);
      break;
    case ANGRY:
      display_eyes(angry, 0);

      servo1_target = 90 + 10.0 * sin(millis() / 250.00);
      servo2_target = 110 + 15.0 * cos(millis() / 175.00);
      break;
    case SUPRISED:
      display_eyes(suprised, 125);

      servo1_target = 90;
      servo2_target = 80 + 10.0 * cos(millis() / 500.00);
      break;
  }

  pixels.show();
}


// --------------------------------------------------------------------------------- //
// ---------------------------------- TOUCH SENSOR --------------------------------- //
// --------------------------------------------------------------------------------- //
void touch_sensor() {
  // Read the value of the touch sensor. If it's being touched and wasn't previously, then that means someone just touched it
  // We can then play the next audio fragment and loop through the emotions
  bool touch_value = digitalRead(TOUCH_PIN);
  if (touch_value && !prev_touch_value) {
    PlayNext();
    switch (emotion) {
      case NEUTRAL:
        emotion = SUPRISED;
        break;
      case SUPRISED:
        emotion = HAPPY;
        break;
      case HAPPY:
        emotion = ANGRY;
        break;
      case ANGRY:
        emotion = SAD;
        break;
      case SAD:
        emotion = NEUTRAL;
        break;
    }
  }
  prev_touch_value = touch_value;
}

// --------------------------------------------------------------------------------- //
// -------------------------------------- EYES ------------------------------------- //
// --------------------------------------------------------------------------------- //
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


// --------------------------------------------------------------------------------- //
// ----------------------------------- HUSKY LENS ---------------------------------- //
// --------------------------------------------------------------------------------- //
void husky_lens() {
  if (!huskylens.request()) {}
  else if (!huskylens.available()) {
//    Serial.println(F("No face appears on the screen!"));
    face_detected = false;
  } else {
//    Serial.println(F("###########"));

    // We loop through all faces received by the HuskyLens. If it's a face that we've learned (ID=1), we will track that face.
    // If no learned face is on the screen, we take the first face returned (which is the face closest to the center)
    face_detected = false;
    int face_index = 0;
    while (huskylens.available()) {
      HUSKYLENSResult result = huskylens.read();
      if (result.command == COMMAND_RETURN_BLOCK) {
//        Serial.println(String() + F("Block:xCenter=") + result.xCenter + F(",yCenter=") + result.yCenter + F(",width=") + result.width + F(",height=") + result.height + F(",ID=") + result.ID);
        if (face_index == 0 || result.ID == 1) face = result;
        face_index ++;
        face_detected = true;
      }
    }
//    Serial.println(String() + F("Block:xCenter=") + face.xCenter + F(",yCenter=") + face.yCenter + F(",width=") + face.width + F(",height=") + face.height + F(",ID=") + face.ID);
  }
}

// --------------------------------------------------------------------------------- //
// ---------------------------------- SERVO MOTORS --------------------------------- //
// --------------------------------------------------------------------------------- //
void move_servos(){
  // We apply some smoothing to the servos and limit the speed
  // We do this because abrubt movements cause a big spike in current draw
  // If we are connected to the PC, we use the PC angles. Otherwise we use the angles from the Arduino
  float servo1_target_ = (pc_connected) ? servo1_target_pc : servo1_target;
  float servo2_target_ = (pc_connected) ? servo2_target_pc : servo2_target;

  if (abs(servo1_target_ - servo1_pos) < 1) {
    servo1.write(servo1_target_);
    servo1_pos = servo1_target_;
  } else {
    servo1_speed = constrain(constrain(servo1_target_ - servo1_pos, servo1_speed - 0.1, servo1_speed + 0.1), -1.0, 1.0);
    servo1_pos += servo1_speed;
    servo1.write(servo1_pos);
  }

  if (abs(servo2_target_ - servo2_pos) < 1) {
    servo2.write(servo2_target_);
    servo2_pos = servo2_target_;
  } else {
    servo2_speed = constrain(constrain(servo2_target_ - servo2_pos, servo2_speed - 0.1, servo2_speed + 0.1), -1.0, 1.0);
    servo2_pos += servo2_speed;
    servo2.write(servo2_pos);
  }
}

// --------------------------------------------------------------------------------- //
// --------------------------------- COMMUNICATION --------------------------------- //
// --------------------------------------------------------------------------------- //
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

  // data is a string of what we received, we will split it into the different values
  // We receive multiple values from our PC as in "123,abc,123,"
  // We can then split this string and extract the values out.
  if (data.length() > 1 && data.charAt(data.length() - 1) == ',') {
    Serial.print(data);
    pc_connected = true; // Once we get a message from the PC, we turn off the touch sensor and do everything with input from the PC

    String value;
    for (int i = 0; data.length() > 0; i++){
      value = data.substring(0, data.indexOf(','));
      data = data.substring(data.indexOf(',') + 1, data.length());

      if (i == 0) servo1_target_pc = value.toInt();
      if (i == 1) servo2_target_pc = value.toInt();
      if (i == 2) {
        if (value == "NEUTRAL") emotion = NEUTRAL;
        if (value == "SUPRISED") emotion = SUPRISED;
        if (value == "HAPPY") emotion = HAPPY;
        if (value == "ANGRY") emotion = ANGRY;
        if (value == "SAD") emotion = SAD;
      }
      // If more values are needed, add other lines here, e.g. if (i == 3) ...
    }
  }
}