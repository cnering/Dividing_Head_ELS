#include "FastAccelStepper.h"
#include "config.h"
#include <Wire.h>
#include <U8g2lib.h>
#include "Button2.h"
#include "state.h"
#include <Preferences.h>
#include <ESP32Encoder.h>

Preferences prefs;

AppState STATE;

ESP32Encoder encoder;
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

U8G2_SSD1309_128X64_NONAME2_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

Button2 button_up_arrow, button_down_arrow, button_left_arrow, button_right_arrow, button_center_arrow, button_mode, button_ok, button_cancel;

void left_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:          STATE.simple.divisions_current_place++;  break;
  }
  simulate_mill_left();
  
}
void right_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:          STATE.simple.divisions_current_place--;  break;
  }
  simulate_mill_right();
}
void up_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case BACKLASH_ADJUST: changeBacklash(1); break;
    case SIMPLE:          changeSimpleDigit(1);  break;
  }
}
void down_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case BACKLASH_ADJUST: changeBacklash(-1); break;
    case SIMPLE:          changeSimpleDigit(-1); break;
  }
}

void center_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case BACKLASH_ADJUST: acceptBacklash(); break;
    case SIMPLE:          return; break;
  }
}
void cancel_tap(Button2& btn) {
  STATE.common.rotation_started = false;
  STATE.simple.num_divisions = 0;
  STATE.simple.current_run_step = 0;
}
void mode_tap(Button2& btn) {
  STATE.mode = static_cast<Mode>((STATE.mode + 1) % MODE_COUNT);
}
void ok_tap(Button2& btn) {
  showNewNumber();
}



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
  // put your main code here, to run repeatedly:

  button_up_arrow.loop();
  button_down_arrow.loop();
  button_left_arrow.loop();
  button_right_arrow.loop();
  button_center_arrow.loop();
  button_mode.loop();
  button_ok.loop();
  button_cancel.loop();

  displayCurrentModePage();

}

void displayCurrentModePage(){
  switch (STATE.mode){
    case BACKLASH_ADJUST:
      displayBacklashAdjust();
      break;
    case SIMPLE:
      displaySimpleDivisions();
      break;
    case ENCODER_TEST:
      displayEncoderTest();
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

void displaySimpleDivisions(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // pick your X offset (here 0) and Y positions for each line

  String header = "# of Divisions";

  u8g2.drawStr(0, 10, header.c_str());

  //u8g2.drawStr(0, 25, String(num_divisions).c_str());
  u8g2.drawStr(0, 25, (String(STATE.simple.num_divisions_hundreds) + String(STATE.simple.num_divisions_tens) + String(STATE.simple.num_divisions_ones)).c_str());
  
  u8g2.drawStr(5 * (3 - STATE.simple.divisions_current_place) ,27 , "_");

  u8g2.drawStr(0, 50, (String(STATE.simple.current_run_step) + "/" + String(STATE.simple.num_divisions)).c_str());

  u8g2.sendBuffer();
}

void displayBacklashAdjust(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // pick your X offset (here 0) and Y positions for each line

  String header = "Backlash Adjust";

  u8g2.drawStr(0, 10, header.c_str());

  
  u8g2.drawStr(0, 25, String(STATE.backlash.backlash_steps).c_str());

  u8g2.sendBuffer();
}

void showNewNumber() {
  if(!STATE.common.rotation_started){
    remove_backlash();
    STATE.common.rotation_started = true;
  }
  Serial.println(STATE.simple.num_divisions);
  Serial.println(1.0/STATE.simple.num_divisions);
  long steps_to_move = ((1.0/STATE.simple.num_divisions) * 40.0)*10000.0;
  Serial.print("moving steps ");
  Serial.println(steps_to_move);
  stepper->move(steps_to_move);
  STATE.simple.current_run_step++;
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

void changeSimpleDigit(int change){
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
  STATE.simple.num_divisions = 100 * STATE.simple.num_divisions_hundreds + 10 * STATE.simple.num_divisions_tens + STATE.simple.num_divisions_ones;
}

unsigned long microtime = 0;
int inches_to_move = 3;
int seconds_to_move = 3;
int time_skip = 1000;

void simulate_mill_left(){
  for(int i = 0; i < seconds_to_move * 3000000; i+= time_skip){
    if (micros() - microtime > time_skip){
      microtime = micros();
      encoder.setCount(encoder.getCount() + 25);
    }
  }
}

void simulate_mill_right(){
  if (micros() - microtime > time_skip){
    microtime = micros();
    encoder.setCount(encoder.getCount() + 25);
  }
}



