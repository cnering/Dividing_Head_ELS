#pragma once
#include <Arduino.h>

enum Mode : uint8_t { SIMPLE, ROTARY_TABLE, BACKLASH_ADJUST, HELICAL, ENCODER_TEST };

struct Common { 
  boolean rotation_started = false; 
};
struct SimpleState { 
  int num_divisions = 0;
  int num_divisions_ones = 0;
  int num_divisions_tens = 0;
  int num_divisions_hundreds = 0;
  int divisions_current_place = 1;
  int current_run_step = 0;
};
struct BacklashAdjust { 
  int backlash_steps = 0;
};
struct AdvAState { float setpoint=0, measured=0; };
struct AdvBState { uint32_t jobsQueued=0, jobsDone=0; };

struct AppState {
  Common common;
  Mode mode = SIMPLE;
  SimpleState simple;
  AdvAState   advA;
  AdvBState   advB;
  BacklashAdjust backlash;
};

extern AppState STATE;