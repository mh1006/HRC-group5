#include "config.h"

AnimationStep default_animation_step = {servo_horizontal_default_pos, servo_vertical_default_pos, 500};

class Animation {
public:
  Animation(AnimationStep* animation_steps, int animation_count)
    : steps(animation_steps), count(animation_count), index(-1), running(false) {}

  void play() {
    start_time = millis();
    index = -1;
    running = true;
  }

  void stop(){
    running = false;
  }

  bool has_update(){
    if (!running || index > count) return false;

    if (index == count) {
      running = false;  
      return true;
    }

    if ( millis() - start_time >= currentStep().timeMs){
      start_time = millis();
      index ++;
      if (index > count) running = false;
      return true;
    }
    return false;
  }

  AnimationStep currentStep() const { 
    AnimationStep s;
    if (index == count || index == -1){
      return default_animation_step;
    }
    memcpy_P(&s, &steps[index], sizeof(AnimationStep));
    return s; 
  }

private:
  AnimationStep* steps;
  int count;
  int index;
  bool running;
  unsigned long start_time;
};