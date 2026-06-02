#include "config.h"

void husky_lens(){
  if (!huskylens.request() || !huskylens.available()) {
    face_detected = false;
  } else {
    face_detected = false;
    // We loop through all detected faces starting with index 0
    int face_index = 0;
    while (huskylens.available()) {
      // Read all of the detected objects
      HUSKYLENSResult result = huskylens.read();
      // If an object is a block (so a rectangle which is how faces are detected)
      if (result.command == COMMAND_RETURN_BLOCK) {
        face_detected = true;
        // If it is the first face that is detected, save it as the face we are following. 
        // Otherwise, if it is the face we have already learned (ID==1) follow that.
        if (face_index == 0 || result.ID == 1) face = result;

        // All useful info of a block:
        // Serial.println(String() + F("Block:xCenter=") + result.xCenter + F(",yCenter=") + result.yCenter + F(",width=") + result.width + F(",height=") + result.height + F(",ID=") + result.ID);
        face_index++;
      }
    }
  }
}