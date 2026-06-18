#include "config.h"
#include "util.h"
#include "led_matrices.h"

char current_button_colors[3] = {'O', 'O', 'O'};

// Button 1 and 2 are the led matrices, 3 is the LCD display
void display_button_colors(char button_1, char button_2, char button_3){
  led_matrices.setPin(BUTTON_MATRICES_PIN);
  led_matrices.clear();
  current_button_colors[0] = button_1;
  current_button_colors[1] = button_2;
  current_button_colors[2] = button_3;
  Serial.print(button_1);
  Serial.print(F(","));
  Serial.print(button_2);
  Serial.print(F(","));
  Serial.println(button_3);
  display_matrices(surprised, button_1, button_2);
  led_matrices.show();
  led_matrices.setPin(EYE_PIN);
  Color b3 = char_to_color(button_3);
  lcd.setRGB(b3.r, b3.g, b3.b);
}

void read_buttons(){
  for (int i =0; i < 3; i++){
    bool reading = digitalRead(BUTTON_PINS[i]); 
    if (reading != buttonStates[i]){
      buttonStates[i] = reading;
      char send_colors[3];
      for (int j = 0; j < 3; j++){
        if (j == i){
          send_colors[i] = 'C';
        } else{
          send_colors[j] = current_button_colors[j];
        }
      }   
      display_button_colors(send_colors[0], send_colors[1], send_colors[2]);
    }
  }
}