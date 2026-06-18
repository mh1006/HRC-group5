#ifndef UTIL
#define UTIL
#include "config.h"

struct Color{
  int r, g, b;

  // Brightness is a value from 0-255, so after multiplication, we convert back to the correct range for color values by dividing by 255
  Color operator*(int brightness) const {
    return {
      (int)(r * brightness)/255,
      (int)(g * brightness)/255,
      (int)(b * brightness)/255
      };
    }

  uint32_t toNeoPixel(Adafruit_NeoPixel& pixels) const {
        return pixels.Color(r, g, b);
    }

  void print() const {
    Serial.print(F("("));
    Serial.print(r);
    Serial.print(F(", "));
    Serial.print(g);
    Serial.print(F(", "));
    Serial.print(b);
    Serial.println(F(")"));
  }
};

Color char_to_color(char color_char){
    switch(color_char) {
      case 'R': return {255, 0, 0};
      case 'G': return {0, 255, 0};
      case 'B': return {0, 0, 255};
      case 'Y': return {255, 255, 0};
      case 'C': return {0, 255, 255};
      case 'P': return {128, 0, 255};
      case 'W': return {255, 255, 255}; // White
      default:  return {0, 0, 0};       // Off
  }
}

Emotion string_to_emotion(String emotion_string){
    if(emotion_string == "SAD") return SAD;
    else if(emotion_string == "SURPRISED") return SURPRISED;
    else if(emotion_string == "HAPPY") return HAPPY;
    else if(emotion_string == "ANGRY") return ANGRY;
    else if (emotion_string == "RAINBOW") return RAINBOW;
    return NEUTRAL;
}

#endif