#include "config.h"
#include "movement.h"
#include "vision.h"
#include "led_matrices.h"

// Timing of loop
long timer;
long timer_comm;

// Servos
float smoothing_speed = 0.05;
Servo servo_horizontal, servo_vertical;
float servo_horizontal_default_pos = 90, servo_vertical_default_pos = 30;
float servo_horizontal_pos = servo_horizontal_default_pos, servo_vertical_pos = servo_vertical_default_pos; 
float servo_horizontal_target = servo_horizontal_pos, servo_vertical_target = servo_vertical_pos;
float servo_horizontal_speed = 0, servo_vertical_speed = 0;

// Huskylens
HUSKYLENS huskylens;
HUSKYLENSResult face;
bool face_detected = false;
bool huskylens_connected = true;

// Led matrices
Adafruit_NeoPixel pixels;
int LED_BRIGHTNESS = 8;
Emotion eye_emotion = NEUTRAL;

// Hues of different eye emotions
int neutral_hue = 125;
int happy_hue = 80;
int angry_hue = 0;
int sad_hue = 150;

// Different eye byte arrays
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

byte surprised[] = {
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

  // Wire is for communication with I2C ports, which is what huskylens should be plugged into
  Wire.begin();
  if (!huskylens.begin(Wire)) {
      Serial.println(F("HuskyLens failed to start! Please check the I2C connection."));
      delay(1000);
      if (!huskylens.begin(Wire)) {
        Serial.println("Failed to start HuskyLens twice, continuing without camera.");
        huskylens_connected = false;
      }
  }

  // Set the pin of the servos
  servo_horizontal.attach(SERVO_PIN_1);
  servo_vertical.attach(SERVO_PIN_2);
  // Set the servos to their default position on start
  servo_horizontal.write(servo_horizontal_default_pos);
  servo_vertical.write(servo_vertical_default_pos);

  // Initialise the led matrices
  pixels.updateLength(LED_COUNT);
  pixels.setPin(LED_PIN);
  pixels.updateType(NEO_GRB + NEO_KHZ800);
  pixels.begin();
  pixels.show();
}

void loop() {
  if (millis() - timer >= 20){
    timer = millis();
    move_servos();
    if (huskylens_connected) husky_lens();
    run_emotions();
  }
  if (millis() - timer_comm >= 10){
    timer_comm = millis();
    receive_communication();
    send_communication();
  }
}

// ============================================
// ----------------- Emotions -----------------
// ============================================

Emotion string_to_emotion(String emotion_string){
    if(emotion_string == "SAD") return SAD;
    else if(emotion_string == "SURPRISED") return SURPRISED;
    else if(emotion_string == "HAPPY") return HAPPY;
    else if(emotion_string == "ANGRY") return ANGRY;
    return NEUTRAL;
}

void run_emotions(){
  pixels.clear();  

  switch (eye_emotion) {
    case NEUTRAL:
      if (millis() % 5000 < 150) display_eyes(blink1, neutral_hue);
      else if (millis() % 5000 < 300) display_eyes(blink2, neutral_hue);
      else if (millis() % 5000 < 450) display_eyes(blink1, neutral_hue);
      else display_eyes(neutral, neutral_hue);
      break;

    case HAPPY:
      display_eyes(happy, happy_hue);
      break;

    case SAD:
      display_eyes(sad, sad_hue);  
      break;

    case ANGRY:
      display_eyes(angry, angry_hue);
      break;

    case SURPRISED:
      display_eyes(surprised, neutral_hue);
      break;
      
  }
  pixels.show();
}

// ============================================
// ----------- Python communication -----------
// ============================================

void receive_communication() {
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
  // Example of what the data could look like: "SERVO,(10,10);EYES,sad;."
  if (data.length() > 1 && data.charAt(data.length() - 1) == '.') {
    // Send the received data back for debugging
    Serial.print("Received: " + data);

    String command_pair;
    String type;
    String value;
    for (int i = 0; data.length() > 1; i++){
      command_pair = data.substring(0, data.indexOf(';'));
      data = data.substring(data.indexOf(';') + 1, data.length());
      
      type = command_pair.substring(0,command_pair.indexOf(','));
      value = command_pair.substring(command_pair.indexOf(',') + 1,command_pair.indexOf(';'));

      if (type == "SERVO") {
        int first_value = value.substring(value.indexOf('(') + 1, value.indexOf(',')).toInt();
        int second_value = value.substring(value.indexOf(',') + 1, value.indexOf(')')).toInt();
        servo_horizontal_target = first_value;
        servo_vertical_target = second_value;
        Serial.print("Setting servo 1 to: " + String(servo_horizontal_target) + "" + "Setting servo 2 to: " + String(servo_vertical_target) + "\n");
      }
      if (type == "EYES") {
        Serial.print("Setting emotion to: " + value + "\n");
        eye_emotion = string_to_emotion(value);
      }
      // if (type == "EMOTION") {
      //   Serial.print("Setting emotion to: " + value + "\n");
      //   emotion = string_to_emotion(value);
      //   }
      }
    }
}

void send_communication(){
  if (face_detected){
      String message_json = "{\"detected_face\": {";
      // Serial.println(String() + F("Block:xCenter=") + result.xCenter + F(",yCenter=") + result.yCenter + F(",width=") + result.width + F(",height=") + result.height + F(",ID=") + result.ID);
      message_json += "\"xCenter\": " + String(face.xCenter) + ",";
      message_json += "\"yCenter\": " + String(face.yCenter) + ",";
      message_json += "\"width\": " + String(face.width) + ",";
      message_json += "\"height\": " + String(face.height);
      message_json += "}}";
      Serial.println(message_json);
  }
}