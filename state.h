#pragma once
#include <Arduino.h>

enum Mode : volatile uint8_t { SIMPLE, ROTARY_TABLE, HELICAL, BACKLASH_ADJUST, ENCODER_TEST };

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
  boolean started_move = false;
};
struct RotaryTable{
  int num_degrees = 0;
  int num_degrees_ones = 1;
  int num_degrees_tens = 0;
  int num_degrees_hundreds = 0;
  int degrees_current_place = 1;
  int current_run_step = 0;
  int current_speed_degrees_per_second = 1;
  int rotary_direction = 1;
  enum rotary_adjust_mode : int { ROTARY_DEGREES, ROTARY_SPEED, ROTARY_MODE};
  enum rotary_start_mode : int { CNTR, CW, CCW};
  rotary_adjust_mode current_adjust_mode = ROTARY_DEGREES;
  rotary_start_mode current_start_mode = CNTR;
  boolean initial_move = true;
  boolean finished_move = false;
};
struct BacklashAdjust { 
  int backlash_steps = 0;
  int last_step_direction = 0;
};
struct HelicalGears {

  int64_t last_encoder_count = 0;
    // DRO scale
  double droCountsPerUnit = 25400.0; // e.g., counts per inch (or per mm if metric)

  // Work spindle drive
  double stepsPerRev = 10000*40; // motor steps * microsteps * gearing to spindle

  // Gear geometry input
  bool   metric = false;     // true = metric (module), false = imperial (DP)
  int    teeth = 40;         // z
  int module_or_DP = 16;  // if metric==true -> module m; else -> DP
  int leadUnits = 0.0;    // linear travel per 1 rev (same units as droCountsPerUnit)
  int helixDeg = 20.0;    // β at reference diameter
  int currentRunStep = 0;

  enum helical_adjust_mode : int { HELICAL_UNITS, HELICAL_TEETH, HELICAL_DPMOD, HELICAL_ANGLE};
  helical_adjust_mode current_helical_mode = HELICAL_UNITS;

  boolean finished_move = false;
  boolean rotation_started = false;

};

struct AppState {
  Common common;
  Mode mode = SIMPLE;
  SimpleState simple;
  RotaryTable rotary;
  HelicalGears   helical;
  BacklashAdjust backlash;
};

extern AppState STATE;