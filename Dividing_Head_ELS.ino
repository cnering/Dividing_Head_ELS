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

}

int cur_speed = 0;
int dir = 1;


void loop() {
  executeButtonLoops();
  displayCurrentModePage();
  //encoderTester();
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
      if(counts_delta != 0){
        //the encoder has count deltas AND we are in the right mode, time to move by the amount that we're supposed to
        int32_t steps_to_turn = elsStepsFromDroDelta(counts_delta,STATE.helical.metric,STATE.helical.droCountsPerUnit,STATE.helical.stepsPerRev,STATE.helical.teeth,STATE.helical.module_or_DP,STATE.helical.helixDeg);
        move_auto_backlash(steps_to_turn);
      }
    }
    vTaskDelay(1);
  }
}

int32_t elsStepsFromDroDelta(
    int32_t droDeltaCounts,   // Δcounts since last call (from your DRO)
    bool    metric,           // true = mm/module; false = inch/DP
    double  droCountsPerUnit, // e.g. 25400.0 counts/in or 1000.0 counts/mm
    double  stepsPerRev,      // motor steps * microsteps * gearing-to-spindle
    int     teeth,            // gear tooth count z
    double  module_or_DP,     // if metric: module m; else: diametral pitch DP
    double  helixDeg         // helix angle β at pitch dia (constant helix)
) {
  // --- Static cache & accumulator (persists across calls) ---
  struct Cache {
    bool    valid = false;
    bool    metric;
    double  droCountsPerUnit;
    double  stepsPerRev;
    int     teeth;
    double  module_or_DP;
    double  helixDeg;
    double  k_countsToSteps;  // steps per DRO count
    double  fracAccum;        // leftover fractional steps
  };
  static Cache C;

  // Recompute coefficient if first time or any parameter changed
  bool needRecalc = !C.valid
                 || C.metric          != metric
                 || C.droCountsPerUnit!= droCountsPerUnit
                 || C.stepsPerRev     != stepsPerRev
                 || C.teeth           != teeth
                 || C.module_or_DP    != module_or_DP
                 || C.helixDeg        != helixDeg;

  if (needRecalc) {
    C.metric           = metric;
    C.droCountsPerUnit = droCountsPerUnit;
    C.stepsPerRev      = stepsPerRev;
    C.teeth            = teeth;
    C.module_or_DP     = module_or_DP;
    C.helixDeg         = helixDeg;

    // Pitch diameter in same linear units as droCountsPerUnit
    double d;
    if (metric) {
      // d = m * z  (mm)
      d = module_or_DP * (double)teeth;
    } else {
      // d = z / DP (inches)
      d = (double)teeth / module_or_DP;
    }

    // Lead at pitch diameter: L = π * d * tan(β)
    const double beta = helixDeg * (M_PI / 180.0);
    double L = M_PI * d * tan(beta);
    if (!(L > 0.0)) L = 1e-9; // guard against zero/invalid

    // steps per unit of table travel
    const double stepsPerUnit = stepsPerRev / L;

    // steps per DRO count
    C.k_countsToSteps = (stepsPerUnit) / droCountsPerUnit;

    // Reset accumulator when params change
    C.fracAccum = 0.0;
    C.valid = true;
  }

  // Convert counts -> steps (with sign convention)
  double stepsFloat = (double)droDeltaCounts * C.k_countsToSteps;
  C.fracAccum += stepsFloat;

  // Emit whole steps now; keep the remainder for later
  int32_t wholeSteps = (int32_t)((C.fracAccum >= 0.0) ? floor(C.fracAccum) : ceil(C.fracAccum));
  C.fracAccum -= (double)wholeSteps;

  return wholeSteps; // >0 or <0; use to drive DIR+STEP
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
    int calculated_speed_steps_per_second = (speed/360.0) * 400000;
    stepper->setSpeedInHz(calculated_speed_steps_per_second);
    switch(STATE.rotary.current_start_mode){
      case  RotaryTable::CNTR:
        if(STATE.rotary.initial_move){
          move_auto_backlash((total_steps_to_move/2) * STATE.rotary.rotary_direction,false, true, calculated_speed_steps_per_second);
          STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
          STATE.rotary.initial_move = false;
        } else{
          move_auto_backlash((total_steps_to_move) * STATE.rotary.rotary_direction);
          STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
        }
      break;
      case RotaryTable::CW:
        if(STATE.rotary.initial_move){
          STATE.rotary.rotary_direction = -1;
          STATE.rotary.initial_move = false;
        }
        move_auto_backlash((total_steps_to_move) * STATE.rotary.rotary_direction);
        STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
      break;
      case RotaryTable::CCW:
        if(STATE.rotary.initial_move){
          STATE.rotary.rotary_direction = 1;
          STATE.rotary.initial_move = false;
        }
        move_auto_backlash((total_steps_to_move) * STATE.rotary.rotary_direction);
        STATE.rotary.rotary_direction = STATE.rotary.rotary_direction * -1;
    }
  } else if(enumValue == HELICAL){
    if(!STATE.helical.rotation_started){
      STATE.helical.rotation_started = true;
    }
    if(STATE.helical.currentRunStep < STATE.helical.teeth){
      STATE.helical.currentRunStep++;
      /*long steps_to_move = ((1.0/STATE.simple.num_divisions) * 40.0)*10000.0;
      move_auto_backlash(steps_to_move);*/
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
  }
  Serial.println(final_move_amount);
  stepper->move(final_move_amount);
  
  /*Reset speed to defaults*/
  stepper->setAcceleration(ACCELERATION);
  stepper->setSpeedInHz(SPEED_IN_HZ);

}

void changeBacklash(int change){
  STATE.backlash.backlash_steps+= change;
  move_auto_backlash(change,SKIP_BACKLASH);
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
      STATE.helical.metric = !STATE.helical.metric;
    } else if(STATE.helical.current_helical_mode == HelicalGears::HELICAL_TEETH){
      STATE.helical.teeth += change;
    } else if(STATE.helical.current_helical_mode == HelicalGears::HELICAL_DPMOD){
      STATE.helical.module_or_DP += change;
    } else if(STATE.helical.current_helical_mode == HelicalGears::HELICAL_ANGLE){
      STATE.helical.helixDeg += change;
    }
  }
}

