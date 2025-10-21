void executeButtonLoops(){
  button_up_arrow.loop();
  button_down_arrow.loop();
  button_left_arrow.loop();
  button_right_arrow.loop();
  button_center_arrow.loop();
  button_mode.loop();
  button_ok.loop();
  button_cancel.loop();
}

void left_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:          STATE.simple.divisions_current_place = min(STATE.simple.divisions_current_place+1,3);  break;
    case ROTARY_TABLE:          STATE.rotary.degrees_current_place = min(STATE.rotary.degrees_current_place+1,3);  break;
    case BACKLASH_ADJUST: move_auto_backlash(-200,SKIP_BACKLASH); break;
    //case MANUAL:          STATE.manual.degrees_per_second-= 5; break;
  }
  test_encoder = false;
  
}

void left_arrow_press(Button2& btn){
  switch (STATE.mode){
    case MANUAL:          STATE.manual.isBeingHeld = false; STATE.manual.degrees_per_second-= 1; break;
  }
}
void left_arrow_hold(Button2& btn){
    switch (STATE.mode){
      case MANUAL:
        STATE.manual.isBeingHeld = true;
        Serial.println(STATE.manual.isBeingHeld);
        STATE.manual.next_hold_tick = millis();
  }
}

void right_arrow_press(Button2& btn){
  switch (STATE.mode){
    case MANUAL:          STATE.manual.isBeingHeld = false; STATE.manual.degrees_per_second+= 1; break;
  }
}
void right_arrow_release(Button2& btn){
  switch (STATE.mode){
    case MANUAL:          STATE.manual.isBeingHeld = false; break;
  }
}
void left_arrow_release(Button2& btn){
  switch (STATE.mode){
    case MANUAL:          STATE.manual.isBeingHeld = false; break;
  }
}


void right_arrow_hold(Button2& btn){
    switch (STATE.mode){
      case MANUAL:
        STATE.manual.isBeingHeld = true;
        STATE.manual.next_hold_tick = millis();
  }
}

void right_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:          STATE.simple.divisions_current_place = max(STATE.simple.divisions_current_place-1,1);  break;
    case ROTARY_TABLE:          STATE.rotary.degrees_current_place = max(STATE.rotary.degrees_current_place-1,1);  break;
    case BACKLASH_ADJUST: move_auto_backlash(200,SKIP_BACKLASH); break;
    //case MANUAL:          STATE.manual.degrees_per_second+= 1; break;
    //case MANUAL:          STATE.manual.degrees_per_second+= 5; break;
  }
  test_encoder = true;
}
void up_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case BACKLASH_ADJUST: changeBacklash(1,STATE.backlash.apply_backlash_on_direction_change); break;
    case SIMPLE:          changeDigit(1,SIMPLE);  break;
    case ROTARY_TABLE:    changeDigit(1,ROTARY_TABLE); break;
    //case HELICAL:         changeDigit(1,HELICAL); break;
  }
}

void up_arrow_click(Button2& btn) {
  switch (STATE.mode){
    /*case HELICAL:         changeDigit(1,HELICAL); break;*/
  }
}

void up_arrow_press(Button2& btn) {
  switch (STATE.mode){
    case HELICAL: STATE.helical.hold_mode = false; changeDigit(1,HELICAL); break;
  }
}
void up_arrow_release(Button2& btn) {
  switch (STATE.mode){
    case HELICAL: STATE.helical.hold_mode = false; break;
  }
}

void up_arrow_hold(Button2& btn){
    switch (STATE.mode){
    case HELICAL:
      //changeDigit(1,HELICAL);
      STATE.helical.hold_mode = true;
      STATE.helical.next_hold_tick = millis();
      break;
  } 
}

void down_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case BACKLASH_ADJUST: changeBacklash(-1,STATE.backlash.apply_backlash_on_direction_change); break;
    case SIMPLE:          changeDigit(-1,SIMPLE); break;
    case ROTARY_TABLE:    changeDigit(-1,ROTARY_TABLE); break;
    case MANUAL:         STATE.manual.isBeingHeld = true; break;
    //case HELICAL:         changeDigit(-1,HELICAL); break;
  }
}
void down_arrow_click(Button2& btn) {
  switch (STATE.mode){
    /*case HELICAL:         changeDigit(-1,HELICAL); break;*/
  }
}

void down_arrow_press(Button2& btn) {
  switch (STATE.mode){
    case HELICAL: STATE.helical.hold_mode = false; changeDigit(-1,HELICAL); break;
  }
}
void down_arrow_release(Button2& btn) {
  switch (STATE.mode){
    case HELICAL: STATE.helical.hold_mode = false; break;
  }
}

void down_arrow_hold(Button2& btn){
    switch (STATE.mode){
    case HELICAL:
      STATE.helical.hold_mode = true;
      STATE.helical.next_hold_tick = millis();
      break;
  }
}

void center_arrow_tap(Button2& btn) {
  Serial.println("tapped");
  switch (STATE.mode){
    case BACKLASH_ADJUST: STATE.backlash.apply_backlash_on_direction_change = !STATE.backlash.apply_backlash_on_direction_change; break;
    case SIMPLE:          return; break;
    case ROTARY_TABLE:    if(STATE.rotary.current_adjust_mode + 1 == RotaryTable::ROTARY_END){STATE.rotary.current_adjust_mode = RotaryTable::ROTARY_DEGREES;}else{STATE.rotary.current_adjust_mode = static_cast<RotaryTable::rotary_adjust_mode>((STATE.rotary.current_adjust_mode + 1));}; break;
    case HELICAL:  if(STATE.helical.current_helical_mode + 1 == HelicalGears::HELICAL_END){STATE.helical.current_helical_mode =  HelicalGears::HELICAL_UNITS;}else{STATE.helical.current_helical_mode = static_cast<HelicalGears::helical_adjust_mode>((STATE.helical.current_helical_mode + 1));}; break;
  }
  Serial.println(STATE.mode);
}
void cancel_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:            cancel_simple(); break;
    case HELICAL:            STATE.helical.currentRunStep = 0; break;
  }
}
void mode_tap(Button2& btn) {
  STATE.mode = static_cast<Mode>((STATE.mode + 1) % MODE_COUNT);
  if(STATE.mode == Mode::HELICAL){
    STATE.helical.begin_helical_mode = true;
  }
}
void ok_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:          advanceIndex(SIMPLE); break;
    case ROTARY_TABLE:          advanceIndex(ROTARY_TABLE); break;
    case HELICAL:          advanceIndex(HELICAL); break;
    case BACKLASH_ADJUST:      acceptBacklash(); break;
  }
}

