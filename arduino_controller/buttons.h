#include "config.h"
#include "util.h"
#include "led_matrices.h"

void read_buttons(){
  
}

// Button 1 and 2 are the led matrices, 3 is the LCD display
void display_button_colors(char button_1, char button_2, char button_3){
  Serial.print(button_1);
  Serial.print(F(","));
  Serial.print(button_2);
  Serial.print(F(","));
  Serial.println(button_3);
  display_matrices(surprised, button_1, button_2);
  Color b3 = char_to_color(button_3);
  lcd.setRGB(b3.r, b3.g, b3.b);
}