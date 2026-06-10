#include "config.h"
#include "movement.h"
#include "vision.h"
#include "audio.h"
#include "led_matrices.h"
#include "colorGame.h"

// Timing of loop
long timer;
long timer_comm;
long timer_test;
int rainbow_timer;

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
struct FaceResult {
  int xCenter, yCenter, width, height;
};
FaceResult last_face;// To track if the face info changed from the last message we sent
bool face_detected = false;
bool huskylens_connected = true;
bool tracking_enabled = false;

// Audio player
SoftwareSerial mp3(AUDIO_PIN_1, AUDIO_PIN_2);

// Led matrices
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN);
int LED_BRIGHTNESS = 8;
Emotion eye_emotion = NEUTRAL;

// Color Game
rgb_lcd lcd; 
Adafruit_NeoPixel matrix1(NUMPIXELS, MATRIX_PIN_1);
Adafruit_NeoPixel matrix2(NUMPIXELS, MATRIX_PIN_2);

char btnColors[3] = {'O', 'O', 'O'}; 
char allColors[6] = {'R', 'B', 'G', 'Y', 'C', 'P'};
bool lastBtnState[3] = {LOW, LOW, LOW}; 
char targetColor = 'O';

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
  Serial.setTimeout(2000);

  // Wire is for communication with I2C ports, which is what huskylens should be plugged into
  Wire.begin();
  if (!huskylens.begin(Wire)) {
      Serial.println(F("HuskyLens failed to start! Please check the I2C connection."));
      delay(1000);
      if (!huskylens.begin(Wire)) {
        Serial.println("Failed to start HuskyLens twice, continuing without camera.");
        huskylens_connected = false;
        Wire.end();
      }
  }

  // Set the pin of the servos
  servo_horizontal.attach(SERVO_PIN_1);
  servo_vertical.attach(SERVO_PIN_2);
  // Set the servos to their default position on start
  servo_horizontal.write(servo_horizontal_default_pos);
  servo_vertical.write(servo_vertical_default_pos);

  // Initialise the led matrices
  pixels.begin();
  rainbow_timer = 0;

  // Start the connection with the audio module
  mp3.begin(9600);
  delay(100);                 // Required for booting up
  SelectPlayerDevice(0x02);   // Select SD card.
  SetVolume(0x1E);            // Set the volume to the max.

   // Color Game (Command exp: EYES_COLOR,RED;.)
  lcd.begin(16, 2);
  for (int i = 0; i < 3; i++) {
    pinMode(btnPins[i], INPUT);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  setLcdColor('W'); 
  matrix1.begin(); matrix1.setBrightness(LED_BRIGHTNESS); matrix1.show();
  matrix2.begin(); matrix2.setBrightness(LED_BRIGHTNESS); matrix2.show();
}

void loop() {
  if (millis() - timer >= 20){
    timer = millis();
    move_servos();
    if (huskylens_connected) husky_lens();
    if (tracking_enabled) track_face();
    run_emotions();
  }
  if (Serial.available()) receive_communication();
  send_communication();
  checkButtons();
}


// ============================================
// ----------------- Emotions -----------------
// ============================================

Emotion string_to_emotion(String emotion_string){
    if(emotion_string == "SAD") return SAD;
    else if(emotion_string == "SURPRISED") return SURPRISED;
    else if(emotion_string == "HAPPY") return HAPPY;
    else if(emotion_string == "ANGRY") return ANGRY;
    else if (emotion_string == "RAINBOW") return RAINBOW;
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
    case RAINBOW:
      pixels.rainbow(rainbow_timer);
      rainbow_timer += 256;
      break;
  }
  pixels.show();
}

// ============================================
// --------------- Face tracking --------------
// ============================================

// Proportional gain: how aggressively the head chases the face.
// 0.1 means a 160px error (half the screen) moves the servo 16°. 
// TODO: Tune as needed.
#define TRACKING_GAIN 0.1f

void track_face() {
  if (face_detected) {
    // HuskyLens resolution: 320×240, so screen centre is (160, 120).
    // Horizontal: face left of centre → servo turns left (target decreases).
    // Vertical:   face above centre  → servo tilts up  (target increases).
    servo_horizontal_target = constrain(
      servo_horizontal_default_pos - (face.xCenter - 160) * TRACKING_GAIN, 0, 180);
    servo_vertical_target = constrain(
      servo_vertical_default_pos + (face.yCenter - 120) * TRACKING_GAIN, 0, 180);
  } else {
    // No face visible — drift back to default so the robot looks forward.
    servo_horizontal_target = servo_horizontal_default_pos;
    servo_vertical_target   = servo_vertical_default_pos;
  }
}

// ============================================
// ----------- Python communication -----------
// ============================================

void receive_communication() {

  String data = Serial.readStringUntil('.');
  data.trim();

  // Data is a string of what we received, we will split it into the different values
  // Each command is sent as "command_type", command; The final command in the data ends in "." to indicate the end of the command chain.
  // Example of what the data could look like: "SERVO,(10,10);EYES,sad;."
  if ((data.length() > 1) && (data.indexOf(";") > 0) && (data.indexOf(",") > 0)) {
    // Send the received data back for debugging
    // Serial.print(F("Received: ["));
    // Serial.print(data);
    // Serial.println(F("]"));

    String command_pair;
    String command_type;
    String value;
    for (int i = 0; data.length() > 2; i++){
      command_pair = data.substring(0, data.indexOf(';'));
      data = data.substring(data.indexOf(';') + 1, data.length());

      command_type = command_pair.substring(0,command_pair.indexOf(','));
      value = command_pair.substring(command_pair.indexOf(',') + 1);

      if (command_type.equals("SERVO")) {
        int first_value = value.substring(value.indexOf('(') + 1, value.indexOf(',')).toInt();
        int second_value = value.substring(value.indexOf(',') + 1, value.indexOf(')')).toInt();
        servo_horizontal_target = first_value;
        servo_vertical_target = second_value;
        // Serial.print(F("Setting servo 1 to: "));
        // Serial.print(servo_horizontal_target);
        // Serial.print(F(", Setting servo 2 to: "));
        // Serial.println(servo_vertical_target);
      }else if (command_type.equals("EYES")) {
        // Serial.print(F("Setting eye emotion to: "));
        // Serial.println(value);
        eye_emotion = string_to_emotion(value);
      }else if (command_type.equals("AUDIO")) {
        // Serial.print(F("Playing audio file: "));
        // Serial.println(value);
        play_audio(value.toInt());
      }else if (command_type.equals("TRACKING")) {
        tracking_enabled = value.equals("ON");
        if (!tracking_enabled) {
          servo_horizontal_target = servo_horizontal_default_pos;
          servo_vertical_target   = servo_vertical_default_pos;
        }
      }else if (command_type.equals("GAME_SET")) {
        // Expected format from Python: "GAME_SET,RGB;." 
        // 'value' will be "RGB"
        if (value.length() >= 3) {
          char lcd_color = value.charAt(0);
          char m1_color = value.charAt(1);
          char m2_color = value.charAt(2);
          Serial.println(value);
          setGameColors(lcd_color, m1_color, m2_color);
        }
      }
    }
  }
}

bool same_face(HUSKYLENSResult new_face){
  if (new_face.xCenter != last_face.xCenter) return false;
  else if (new_face.yCenter != last_face.yCenter) return false;
  else if (new_face.width != last_face.width) return false;
  else if (new_face.height != last_face.height) return false;
  else return true;
}

void send_communication(){
  if (face_detected && !same_face(face)){
      // Serial.println(String() + F("Block:xCenter=") + result.xCenter + F(",yCenter=") + result.yCenter + F(",width=") + result.width + F(",height=") + result.height + F(",ID=") + result.ID);
      last_face = {face.xCenter, face.yCenter, face.width, face.height};
      // As string concatenation is very bad for performance and made the memory run out, this is sadly the best way I've found to do this :(
      Serial.print(F("{\"detected_face\": {\"xCenter\": "));
      Serial.print(face.xCenter);
      Serial.print(F(", \"yCenter\": "));
      Serial.print(face.yCenter);
      Serial.print(F(", \"width\": "));
      Serial.print(face.width);
      Serial.print(F(", \"height\": "));
      Serial.print(face.height);
      Serial.println(F("}}"));
  }
}