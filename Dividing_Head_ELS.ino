#include "FastAccelStepper.h"
#include "config.h"
#include <Wire.h>
#include <U8g2lib.h>
#include "Button2.h"
#include "state.h"
#include <Preferences.h>
#include <ESP32Encoder.h>

boolean test_encoder = false;

uint64_t last_encoder_counts = 0;

Preferences prefs;

AppState STATE;

ESP32Encoder encoder;
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

U8G2_SSD1309_128X64_NONAME2_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

Button2 button_up_arrow, button_down_arrow, button_left_arrow, button_right_arrow, button_center_arrow, button_mode, button_ok, button_cancel;



volatile int32_t sim_speed = 20;    // counts per update
int32_t max_speed = 500;           // max counts per update (adjust to taste)
unsigned long lastSpeedChange = 0;
unsigned long speedChangeInterval = 500; // ms

void setup() {

  prefs.begin("backlash_steps", false);
  STATE.backlash.backlash_steps = prefs.getInt("backlash_steps", 5);

  button_up_arrow.begin(BUTTON_UP_ARROW);
  button_up_arrow.setTapHandler(up_arrow_tap);

  button_down_arrow.begin(BUTTON_DOWN_ARROW);
  button_down_arrow.setTapHandler(down_arrow_tap);

  button_left_arrow.begin(BUTTON_LEFT_ARROW);
  button_left_arrow.setTapHandler(left_arrow_tap);

  button_right_arrow.begin(BUTTON_RIGHT_ARROW);
  button_right_arrow.setTapHandler(right_arrow_tap);

  button_center_arrow.begin(BUTTON_CENTER_ARROW);
  button_center_arrow.setTapHandler(center_arrow_tap);

  button_mode.begin(BUTTON_MODE);
  button_mode.setTapHandler(mode_tap);

  button_ok.begin(BUTTON_OK);
  button_ok.setTapHandler(ok_tap);

  button_cancel.begin(BUTTON_CANCEL);
  button_cancel.setTapHandler(cancel_tap);

  Serial.begin(115200);

  encoder.attachFullQuad(ENCODER_PIN_A, ENCODER_PIN_B);

    // Give PCNT a modest glitch filter (APB clock cycles). Tune 100–1000.
  

  // Ensure HW limits & software accumulator are initialized
  encoder.clearCount();
  encoder.setCount(500);
  encoder.resumeCount();

  

  /*!!!STEPPER DRIVER!!!*/
  engine.init();
  stepper = engine.stepperConnectToPin(STEPPER_STEP_PIN,DRIVER_RMT);
  if (stepper) {
      stepper->setDirectionPin(STEPPER_DIRECTION_PIN);
      stepper->setAutoEnable(true);

      stepper->setAcceleration(ACCELERATION);
      stepper->setSpeedInHz(SPEED_IN_HZ);
    } else {
      Serial.println("Stepper Not initialized!");
      delay(1000);
    }

    // I²C on ESP32 DevKit1 default pins
  Wire.begin(/* SDA=*/ OLED_SDA, /* SCL=*/ OLED_SCL);
  // bump I2C up to 400 kHz for faster transfers
  Wire.setClock(400000UL);

  /*DISPLAY*/
  u8g2.begin();

  xTaskCreatePinnedToCore(
    simulateManualMotion, "simMotion", 4096, NULL, 1, NULL, 1
  );

}

int cur_speed = 0;
int dir = 1;


void loop() {
  executeButtonLoops();
  displayCurrentModePage();
  encoderTester();
}

void cancel_simple(){
  STATE.common.rotation_started = false; 
  STATE.simple.current_run_step = 0;
}

void encoderTester(){
  uint64_t cur_steps = encoder.getCount();
  //Serial.println(cur_steps);
  if(last_encoder_counts != cur_steps){
    stepper->move(cur_steps - last_encoder_counts);
    last_encoder_counts = cur_steps;
  }
}

void simulateManualMotion(void *param) {
  bool direction = true;  // true = forward, false = backward

  while (true) {
    //Serial.println("hello");
    if(test_encoder){
      unsigned long now = millis();

      // Occasionally change speed and direction
      if (now - lastSpeedChange > speedChangeInterval) {
        lastSpeedChange = now;
        // Random new speed (human-like, slower more often)
        sim_speed = random(0, max_speed);
        // Occasionally go backwards
        if (random(0, 100) < 20) direction = !direction;
        // Occasionally STOP completely
        if (random(0, 100) < 10) sim_speed = 0;
      }

      // Apply motion
      int32_t current = encoder.getCount();
      if (direction)
        current += sim_speed;
      else
        current -= sim_speed;
      
      encoder.setCount(current);

      // Update rate controls smoothness
    }
    vTaskDelay(pdMS_TO_TICKS(20)); // 50 Hz updates
  }
}

void displayEncoderTest(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  int64_t cur_count = encoder.getCount();

  u8g2.drawStr(0, 10, String((int)cur_count).c_str());

  u8g2.sendBuffer();
  Serial.println(cur_count);

}

void advanceIndex(int enumValue) {
  if(enumValue == SIMPLE){
    if(!STATE.common.rotation_started){
      remove_backlash();
      STATE.common.rotation_started = true;
    }
    if(STATE.simple.current_run_step < STATE.simple.num_divisions){
      STATE.simple.current_run_step++;
      long steps_to_move = ((1.0/STATE.simple.num_divisions) * 40.0)*10000.0;
      stepper->move(steps_to_move);
    }
    if(STATE.simple.current_run_step == STATE.simple.num_divisions){
      STATE.simple.finished_move = true;
    }
  } else if(enumValue == ROTARY_TABLE){
    float total_degrees = STATE.rotary.num_degrees;
    float fraction_of_circle = total_degrees/360.0;
    float speed = STATE.rotary.current_speed_degrees_per_second;
    int total_steps_to_move = (400000)*fraction_of_circle;
    int calculated_speed_steps_per_second = (speed/360.0) * 400000;
    stepper->setSpeedInHz(calculated_speed_steps_per_second);
    switch(STATE.rotary.current_start_mode){
      case  RotaryTable::CNTR:
        if(STATE.rotary.initial_move){
          stepper->move((total_steps_to_move/2) * STATE.rotary.rotary_direction);
          STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
          STATE.rotary.initial_move = false;
        } else{
          stepper->move((total_steps_to_move) * STATE.rotary.rotary_direction);
          STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
        }
      break;
      case RotaryTable::CW:
        if(STATE.rotary.initial_move){
          STATE.rotary.rotary_direction = -1;
          STATE.rotary.initial_move = false;
        }
        stepper->move((total_steps_to_move) * STATE.rotary.rotary_direction);
        STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
      break;
      case RotaryTable::CCW:
        if(STATE.rotary.initial_move){
          STATE.rotary.rotary_direction = 1;
          STATE.rotary.initial_move = false;
        }
        stepper->move((total_steps_to_move) * STATE.rotary.rotary_direction);
        STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
    }
  }
}

void remove_backlash(){
  stepper->move(STATE.backlash.backlash_steps);
}

void changeBacklash(int change){
  STATE.backlash.backlash_steps+= change;
  stepper->move(change);
}

void acceptBacklash(){
  prefs.begin("backlash_steps", false);
  prefs.putInt("backlash_steps", STATE.backlash.backlash_steps);
  prefs.end();
}

void changeDigit(int change, int enumItem){
  if(enumItem == SIMPLE){
    STATE.simple.finished_move = false;
    STATE.simple.current_run_step = 0;
    if((100 * STATE.simple.num_divisions_hundreds + 10 * STATE.simple.num_divisions_tens + STATE.simple.num_divisions_ones) + change > 0){
        switch(STATE.simple.divisions_current_place){
          case 1:
            STATE.simple.num_divisions_ones+= change;
            break;
          case 2:
            STATE.simple.num_divisions_tens+= change;
            break;
          case 3:
            STATE.simple.num_divisions_hundreds+= change;
            break;
        }
      STATE.simple.num_divisions = max(100 * STATE.simple.num_divisions_hundreds + 10 * STATE.simple.num_divisions_tens + STATE.simple.num_divisions_ones,1);
    }
  } else if(enumItem == ROTARY_TABLE){
    if(STATE.rotary.current_adjust_mode == RotaryTable::ROTARY_DEGREES){
      STATE.rotary.finished_move = false;
          switch(STATE.rotary.degrees_current_place){
            case 1:
              STATE.rotary.num_degrees_ones+= change;
              break;
            case 2:
              STATE.rotary.num_degrees_tens+= change;
              break;
            case 3:
              STATE.rotary.num_degrees_hundreds+= change;
              break;
          }
        STATE.rotary.num_degrees = 100 * STATE.rotary.num_degrees_hundreds + 10 * STATE.rotary.num_degrees_tens + STATE.rotary.num_degrees_ones;
    } else if(STATE.rotary.current_adjust_mode == RotaryTable::ROTARY_SPEED){
      STATE.rotary.current_speed_degrees_per_second += change;
    } else if(STATE.rotary.current_adjust_mode == RotaryTable::ROTARY_MODE){
      STATE.rotary.current_start_mode = static_cast<RotaryTable::rotary_start_mode>((STATE.rotary.current_start_mode + 1*change) % 3);
    }
  }
}

