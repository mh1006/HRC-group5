#include "config.h"
#include "movement.h"
#include "vision.h"
#include "audio.h"
#include "led_matrices.h"
#include "colorGame.h"
#include <avr/pgmspace.h>

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
int LED_BRIGHTNESS = 1;
Emotion eye_emotion = NEUTRAL;

// Color Game
rgb_lcd lcd; 
Adafruit_NeoPixel matrix1(NUMPIXELS, MATRIX_PIN_1);
Adafruit_NeoPixel matrix2(NUMPIXELS, MATRIX_PIN_2);

char btnColors[3] = {'O', 'O', 'O'}; 
char allColors[6] = {'R', 'B', 'G', 'Y', 'C', 'P'};
bool lastBtnState[3] = {LOW, LOW, LOW}; 
bool stableBtnState[3] = {LOW, LOW, LOW};
char targetColor = 'O';

// Hues of different eye emotions
int neutral_hue = 125;
int happy_hue = 80;
int angry_hue = 0;
int sad_hue = 150;

// Different eye byte arrays
const byte neutral[] PROGMEM = { B0000, B01110, B011110, B0111110, B011110, B01110, B0000 };
const byte blink1[]  PROGMEM = { B0000, B00000, B011110, B0111110, B011110, B00000, B0000 };
const byte blink2[]  PROGMEM = { B0000, B00000, B000000, B1111111, B000000, B00000, B0000 };
const byte surprised[] PROGMEM = { B1111, B11111, B111111, B1111111, B111111, B11111, B1111 };
const byte happy[]   PROGMEM = { B1111, B11111, B111111, B1100011, B000000, B00000, B0000 };
const byte angry[]   PROGMEM = { B0000, B10000, B110000, B1111000, B111110, B11111, B1111 };
const byte sad[]     PROGMEM = { B0000, B00001, B0000011, B0001111, B011111, B11111, B1111 };


void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);

  // Wire is for communication with I2C ports, which is what huskylens should be plugged into
  Wire.begin();
  if (!huskylens.begin(Wire)) {
      Serial.println(F("HuskyLens failed to start! Please check the I2C connection."));
      delay(1000);
      if (!huskylens.begin(Wire)) {
        Serial.println(F("Failed to start HuskyLens twice, continuing without camera."));
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
  pixels.begin();
  rainbow_timer = 0;

  // Start the connection with the audio module
  mp3.begin(9600);
  delay(100);                 // Required for booting up
  SelectPlayerDevice(0x02);   // Select SD card.
  SetVolume(0x1E);            // Set the volume to the max.

   // Color Game
  lcd.begin(16, 2);
  for (int i = 0; i < 3; i++) {
    pinMode(btnPins[i], INPUT_PULLUP);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  setLcdColor('W');
  matrix1.begin(); matrix1.setBrightness(LED_BRIGHTNESS);
  setMatrixColor(1, 'W'); matrix1.show();
  matrix2.begin(); matrix2.setBrightness(LED_BRIGHTNESS);
  setMatrixColor(2, 'W'); matrix2.show();
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

Emotion string_to_emotion(char* value){
  if (strcmp(value, "SAD") == 0) return SAD;
  if (strcmp(value, "SURPRISED") == 0) return SURPRISED;
  if (strcmp(value, "HAPPY") == 0) return HAPPY;
  if (strcmp(value, "ANGRY") == 0) return ANGRY;
  if (strcmp(value, "RAINBOW") == 0) return RAINBOW;
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
  static char buf[64];
  int len = Serial.readBytesUntil('.', buf, sizeof(buf) - 1);
  if (len == 0) return;
  buf[len] = '\0';

  Serial.print(F("Received: ["));
  Serial.print(buf);
  Serial.println(F("]"));

  char *data = buf;
  char *semicolon;

  while ((semicolon = strchr(data, ';')) != NULL) {
    *semicolon = '\0';

    char *comma = strchr(data, ',');
    if (!comma) { data = semicolon + 1; continue; }
    *comma = '\0';

    char *cmd   = data;
    char *value = comma + 1;

    if (strcmp(cmd, "SERVO") == 0) {
      char *c = strchr(value, ',');
      servo_horizontal_target = atoi(value + 1);
      servo_vertical_target   = c ? atoi(c + 1) : 0;
      // Serial.print(F("Setting servo 1 to: "));
      // Serial.print(servo_horizontal_target);
      // Serial.print(F(", Setting servo 2 to: "));
      // Serial.println(servo_vertical_target);
    } else if (strcmp(cmd, "EYES") == 0) {
      // Serial.print(F("Setting eye emotion to: "));
      // Serial.println(value);
      eye_emotion = string_to_emotion(value);
      run_emotions();
    } else if (strcmp(cmd, "AUDIO") == 0) {
      // Serial.print(F("Playing audio file: "));
      // Serial.println(value);
      play_audio(atoi(value));
    } else if (strcmp(cmd, "TRACKING") == 0) {
      tracking_enabled = (strcmp(value, "ON") == 0);
      if (!tracking_enabled) {
        servo_horizontal_target = servo_horizontal_default_pos;
        servo_vertical_target   = servo_vertical_default_pos;
      }
    } else if (strcmp(cmd, "GAME_SET") == 0) {
      // ex: GAME_SET,RGBG;.
      // Serial.print(F("Setting game colors to: "));
      // Serial.println(value);
      if (strlen(value) >= 4)
        setGameColors(value[0], value[1], value[2], value[3]);
    }

    data = semicolon + 1;
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