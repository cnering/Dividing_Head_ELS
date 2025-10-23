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

void move_auto_backlash(int steps, boolean override_backlash = false, boolean override_speed = false, int speed_in_steps_per_second = 1000 );


volatile int32_t sim_speed = 20;    // counts per update
int32_t max_speed = 500;           // max counts per update (adjust to taste)
unsigned long lastSpeedChange = 0;
unsigned long speedChangeInterval = 500; // ms

void setup() {

  prefs.begin("backlash_steps", false);
  STATE.backlash.backlash_steps = prefs.getInt("backlash_steps", 5);

  button_up_arrow.begin(BUTTON_UP_ARROW);
  button_up_arrow.setTapHandler(up_arrow_tap);
  button_up_arrow.setClickHandler(up_arrow_click);
  button_up_arrow.setPressedHandler(up_arrow_press);
  button_up_arrow.setLongClickDetectedHandler(up_arrow_hold);
  button_up_arrow.setReleasedHandler(up_arrow_release);
  button_up_arrow.setLongClickTime(250);
  button_up_arrow.setDebounceTime(5);

  button_down_arrow.begin(BUTTON_DOWN_ARROW);
  button_down_arrow.setTapHandler(down_arrow_tap);
  button_down_arrow.setClickHandler(down_arrow_click);
  button_down_arrow.setPressedHandler(down_arrow_press);
  button_down_arrow.setLongClickDetectedHandler(down_arrow_hold);
  button_down_arrow.setReleasedHandler(down_arrow_release);
  button_down_arrow.setLongClickTime(250);
  button_down_arrow.setDebounceTime(5);

  button_left_arrow.begin(BUTTON_LEFT_ARROW);
  button_left_arrow.setTapHandler(left_arrow_tap);
  button_left_arrow.setPressedHandler(left_arrow_press);
  button_left_arrow.setReleasedHandler(left_arrow_release);
  button_left_arrow.setLongClickDetectedHandler(left_arrow_hold);
  button_left_arrow.setLongClickTime(250);
  button_left_arrow.setDebounceTime(5);

  button_right_arrow.begin(BUTTON_RIGHT_ARROW);
  button_right_arrow.setTapHandler(right_arrow_tap);
  button_right_arrow.setPressedHandler(right_arrow_press);
  button_right_arrow.setReleasedHandler(right_arrow_release);
  button_right_arrow.setLongClickDetectedHandler(right_arrow_hold);
  button_right_arrow.setLongClickTime(250);
  button_right_arrow.setDebounceTime(5);

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
    encoderReader, "encoderReader", 4096, NULL, 1, NULL, 0
  );

  recalculateHelicalParameters();

}

int cur_speed = 0;
int dir = 1;


void loop() {
  executeButtonLoops();
  displayCurrentModePage();

  if(STATE.mode == HELICAL){
    if (STATE.helical.hold_mode && button_up_arrow.isPressed()) {
      uint32_t now = millis();
      if (now >= STATE.helical.next_hold_tick) {
        changeDigit(1,HELICAL);
        STATE.helical.next_hold_tick = now + 80;
      }
    }
    if (STATE.helical.hold_mode && button_down_arrow.isPressed()) {
      uint32_t now = millis();
      if (now >= STATE.helical.next_hold_tick) {
        changeDigit(-1,HELICAL);
        STATE.helical.next_hold_tick = now + 80;
      }
    }
  }
  if(STATE.mode == MANUAL){    
    if(STATE.manual.isBeingHeld && button_left_arrow.isPressed()){
      uint32_t now = millis();
      if (now >= STATE.manual.next_hold_tick) {
        STATE.manual.degrees_per_second--;
        STATE.manual.next_hold_tick = now + 80;
      }
    }
    if(STATE.manual.isBeingHeld && button_right_arrow.isPressed()){
      uint32_t now = millis();
      if (now >= STATE.manual.next_hold_tick) {
        STATE.manual.degrees_per_second++;
        STATE.manual.next_hold_tick = now + 80;
      }
    }
    if (button_down_arrow.isPressed()) {
        stepper->setSpeedInHz(400000.0*(STATE.manual.degrees_per_second/360.0));
        stepper->moveByAcceleration(200000);
    }
    else if (button_up_arrow.isPressed()) {
        stepper->setSpeedInHz(400000.0*(STATE.manual.degrees_per_second/360.0));
        stepper->moveByAcceleration(-200000,true);
    } else{
      stepper->stopMove();
    }
  }

}

void cancel_simple(){
  STATE.common.rotation_started = false; 
  STATE.simple.current_run_step = 0;
  STATE.simple.finished_move = false;
}

void encoderTester(){
  uint64_t cur_steps = encoder.getCount();
  //Serial.println(cur_steps);
  if(last_encoder_counts != cur_steps){
    move_auto_backlash(cur_steps - last_encoder_counts);
    last_encoder_counts = cur_steps;
  }
}

void encoderReader(void *param) {
  for(;;){
    if(STATE.mode == HELICAL){
      /*only listen to the encoder in helical mode, otherwise you can just ignore it*/
      int64_t cur_count = encoder.getCount();
      int64_t counts_delta = cur_count - STATE.helical.last_encoder_count;
      STATE.helical.last_encoder_count = cur_count;
      if(counts_delta != 0 && STATE.helical.currentRunStep > 0){
        //the encoder has count deltas AND we are in the right mode, time to move by the amount that we're supposed to
        int32_t steps_to_turn = 0;
        steps_to_turn = helicalStepsToMove(counts_delta, STATE.helical.metric);
        move_auto_backlash(steps_to_turn);
      }
    }
    vTaskDelay(1);
  }
}

int32_t helicalStepsToMove(int64_t counts_delta, bool metric){
  double ratio = STATE.helical.steps_per_step * counts_delta;
  int int_steps = int(ratio);
  STATE.helical.overflow += ratio - (double)int_steps; 

  if(STATE.helical.overflow >= 1.0){
    int overflow_steps = int(STATE.helical.overflow);
    int_steps += overflow_steps;
    STATE.helical.overflow -= int(STATE.helical.overflow);
  }
    int handedness = 1;
    if(STATE.helical.left_hand_teeth){
      handedness = -1;
    }
    return int_steps * handedness; 
}

void advanceIndex(int enumValue) {
  if(enumValue == SIMPLE){
    if(!STATE.common.rotation_started){
      STATE.common.rotation_started = true;
    }
    if(STATE.simple.current_run_step < STATE.simple.num_divisions){
      STATE.simple.current_run_step++;
      long steps_to_move = ((1.0/STATE.simple.num_divisions) * 40.0)*10000.0;
      move_auto_backlash(steps_to_move);
    }
    if(STATE.simple.current_run_step == STATE.simple.num_divisions){
      STATE.simple.finished_move = true;
    }
  } else if(enumValue == ROTARY_TABLE){
    float total_degrees = STATE.rotary.num_degrees;
    float fraction_of_circle = total_degrees/360.0;
    float speed = STATE.rotary.current_speed_degrees_per_second;
    int total_steps_to_move = (400000)*fraction_of_circle;
    int calculated_speed_steps_per_second = (speed/360.0) * 400000.0;
    switch(STATE.rotary.current_start_mode){
      case  RotaryTable::CNTR:
        if(STATE.rotary.initial_move){
          move_auto_backlash((total_steps_to_move/2) * STATE.rotary.rotary_direction,false, true, calculated_speed_steps_per_second);
          STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
          STATE.rotary.initial_move = false;
        } else{
          move_auto_backlash((total_steps_to_move) * STATE.rotary.rotary_direction,false, true, calculated_speed_steps_per_second);
          STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
        }
      break;
      case RotaryTable::CW:
        if(STATE.rotary.initial_move){
          STATE.rotary.rotary_direction = -1;
          STATE.rotary.initial_move = false;
        }
        move_auto_backlash((total_steps_to_move) * STATE.rotary.rotary_direction,false, true, calculated_speed_steps_per_second);
        STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
      break;
      case RotaryTable::CCW:
        if(STATE.rotary.initial_move){
          STATE.rotary.rotary_direction = 1;
          STATE.rotary.initial_move = false;
        }
        move_auto_backlash((total_steps_to_move) * STATE.rotary.rotary_direction,false, true, calculated_speed_steps_per_second);
        STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
    }
  } else if(enumValue == HELICAL){
    if(!STATE.helical.rotation_started){
      STATE.helical.rotation_started = true;
    }
    if(STATE.helical.currentRunStep < STATE.helical.teeth){
      STATE.helical.currentRunStep++;
      long steps_to_move = ((1.0/STATE.helical.teeth) * 40.0)*10000.0;
      int handedness = 1;
      if(STATE.helical.left_hand_teeth){
        handedness = -1;
      }
      move_auto_backlash(steps_to_move*handedness);
    }
    if(STATE.helical.currentRunStep == STATE.helical.teeth){
      STATE.helical.finished_move = true;
    }
  }
}

void move_auto_backlash(int steps, boolean override_backlash, boolean override_speed, int speed_in_steps_per_second){
  /*first move ever, always apply backlash*/
  double final_move_amount = steps;
  double final_backlash_amount = 0;

  Serial.print("Commanded Steps: ");
  Serial.println(steps);

  if(!override_backlash){
    final_backlash_amount = STATE.backlash.backlash_steps;
  }

  switch(STATE.backlash.last_step_direction){
    case 0:
    /*we haven't moved yet, compensate for backlash in whatever direction we're commanded to move*/
      /*positive move, add backlash*/
      Serial.println("First move, always backlash");
      if(steps >= 0){
        final_move_amount = final_move_amount + final_backlash_amount;
        Serial.println("Setting direction to POSITIVE");
        STATE.backlash.last_step_direction = 1;
      /*negative move, subtract backlash*/
      } else{
        final_move_amount = final_move_amount - final_backlash_amount;
        Serial.println("Setting direction to NEGATIVE");
        STATE.backlash.last_step_direction = -1;
      }
      break;
    case 1:
    /*we've already moved once, decide if we need to compensate for backlash in the FORWARD direction*/
    Serial.println("last move was POSITIVE");
    if(steps >= 0){
      /*we are travelling in the SAME direction, so no need to apply backlash*/
      Serial.println("Travelling same POSITIVE");
      final_move_amount = final_move_amount;
      STATE.backlash.last_step_direction = 1;
    } else{
      /*we have changed directions, apply backlash before moving*/
      Serial.println("POSITIVE TO NEGATIVE");
      final_move_amount = final_move_amount - final_backlash_amount;
      STATE.backlash.last_step_direction = -1;
    }
    break;
    case -1:
    Serial.println("last move was NEGATIVE");
    /*we've already moved once, decide if we need to compensate for backlash in the REVERSE direction*/
    if(steps <= 0){
      /*we are travelling in the SAME direction, so no need to apply backlash*/
      Serial.println("Travelling same NEGATIVE");
      final_move_amount = final_move_amount;
      STATE.backlash.last_step_direction = -1;
    } else{
      /*we have changed directions, apply backlash before moving*/
      Serial.println("NEGATIVE to POSITIVE");
      final_move_amount = final_move_amount + final_backlash_amount;
      STATE.backlash.last_step_direction = 1;
    }
    break;
  }

  if(override_speed){ 
    stepper->setAcceleration(ACCELERATION);
    stepper->setSpeedInHz(speed_in_steps_per_second);
  } else{
    stepper->setAcceleration(ACCELERATION);
    stepper->setSpeedInHz(SPEED_IN_HZ);
  }
  stepper->move(final_move_amount);

}

void changeBacklash(int change, bool skip_backlash){
  STATE.backlash.backlash_steps+= change;
  move_auto_backlash(change,skip_backlash);
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
  } else if (enumItem == HELICAL){
    if(STATE.helical.current_helical_mode == HelicalGears::HELICAL_UNITS){
      //STATE.helical.metric = !STATE.helical.metric;
      if(STATE.helical.current_helical_unit_mode + 1 == HelicalGears::HELICAL_UNITS_END){
        STATE.helical.current_helical_unit_mode = HelicalGears::IN_N;
      } else{
        STATE.helical.current_helical_unit_mode = static_cast<HelicalGears::helical_units>((STATE.helical.current_helical_unit_mode + 1));
      }
    } else if(STATE.helical.current_helical_mode == HelicalGears::HELICAL_TEETH){
      STATE.helical.teeth += change;
    } else if(STATE.helical.current_helical_mode == HelicalGears::HELICAL_DPMOD){
      STATE.helical.module_or_DP += change;
    } else if(STATE.helical.current_helical_mode == HelicalGears::HELICAL_ANGLE){
      STATE.helical.helixDeg += change;
    } else if(STATE.helical.current_helical_mode == HelicalGears::HELICAL_HANDEDNESS){
      STATE.helical.left_hand_teeth = !STATE.helical.left_hand_teeth;
    }
    recalculateHelicalParameters();
  }
}

void recalculateHelicalParameters(){
  /*recalculate helical params*/

    double units_in_normal_module = 0;
    switch (STATE.helical.current_helical_unit_mode){
      case HelicalGears::IN_N:
        units_in_normal_module = (25.4/STATE.helical.module_or_DP);
        break;
      case HelicalGears::IN_T:
        units_in_normal_module = (25.4/STATE.helical.module_or_DP)/cos(STATE.helical.helixDeg * (M_PI / 180.0));
        break;
      case HelicalGears::MM_N:
        units_in_normal_module = (STATE.helical.module_or_DP);
        break;
      case HelicalGears::MM_T:
        units_in_normal_module = (STATE.helical.module_or_DP)/cos(STATE.helical.helixDeg * (M_PI / 180.0));
        break;
    }
    STATE.helical.lead = (M_PI * units_in_normal_module * STATE.helical.teeth) / sin(STATE.helical.helixDeg * (M_PI / 180.0));
    double counts_per_mm = 1000;
    double counts_per_lead  = STATE.helical.lead * 1000;
    STATE.helical.steps_per_step = 400000.0/counts_per_lead;

}

