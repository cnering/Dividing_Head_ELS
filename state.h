#pragma once
#include <Arduino.h>

enum Mode : uint8_t { SIMPLE, ROTARY_TABLE, BACKLASH_ADJUST, HELICAL, ENCODER_TEST };

struct Common { 
  boolean rotation_started = false; 
};
struct SimpleState { 
  int num_divisions = 1;
  int num_divisions_ones = 1;
  int num_divisions_tens = 0;
  int num_divisions_hundreds = 0;
  int divisions_current_place = 1;
  int current_run_step = 0;
  boolean finished_move = false;
};
struct RotaryTable{
  int num_degrees = 0;
  int num_degrees_ones = 1;
  int num_degrees_tens = 0;
  int num_degrees_hundreds = 0;
  int degrees_current_place = 1;
  int current_run_step = 0;
  int current_speed_IPM = 1;
  enum rotary_adjust_mode : int { ROTARY_DEGREES, ROTARY_SPEED };
  rotary_adjust_mode current_adjust_mode = ROTARY_DEGREES;
  boolean finished_move = false;
};
struct BacklashAdjust { 
  int backlash_steps = 0;
};
struct HelicalState {
  float setpoint=0, measured=0; 
  };
struct AdvBState { uint32_t jobsQueued=0, jobsDone=0; };

struct AppState {
  Common common;
  Mode mode = SIMPLE;
  SimpleState simple;
  RotaryTable rotary;
  HelicalState   helical;
  AdvBState   advB;
  BacklashAdjust backlash;
};

extern AppState STATE;