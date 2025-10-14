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
    case SIMPLE:          STATE.simple.divisions_current_place++;  break;
    case ROTARY_TABLE:          STATE.rotary.degrees_current_place++;  break;
  }
  test_encoder = false;
  
}
void right_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:          STATE.simple.divisions_current_place--;  break;
    case ROTARY_TABLE:          STATE.rotary.degrees_current_place--;  break;
  }
  test_encoder = true;
}
void up_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case BACKLASH_ADJUST: changeBacklash(1); break;
    case SIMPLE:          changeDigit(1,SIMPLE);  break;
    case ROTARY_TABLE:    changeDigit(1,ROTARY_TABLE); break;
    case HELICAL:         changeDigit(1,HELICAL); break;
  }
}
void down_arrow_tap(Button2& btn) {
  switch (STATE.mode){
    case BACKLASH_ADJUST: changeBacklash(-1); break;
    case SIMPLE:          changeDigit(-1,SIMPLE); break;
    case ROTARY_TABLE:    changeDigit(-1,ROTARY_TABLE); break;
    case HELICAL:         changeDigit(-1,HELICAL); break;
  }
}

void center_arrow_tap(Button2& btn) {
  Serial.println("tapped");
  switch (STATE.mode){
    case BACKLASH_ADJUST: acceptBacklash(); break;
    case SIMPLE:          return; break;
    case ROTARY_TABLE:    STATE.rotary.current_adjust_mode = static_cast<RotaryTable::rotary_adjust_mode>((STATE.rotary.current_adjust_mode + 1) % 3); break;
    case HELICAL:    STATE.helical.current_helical_mode = static_cast<HelicalGears::helical_adjust_mode>((STATE.helical.current_helical_mode + 1) % 4); break;
  }
  Serial.println(STATE.mode);
}
void cancel_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:            cancel_simple(); break;
  }
}
void mode_tap(Button2& btn) {
  STATE.mode = static_cast<Mode>((STATE.mode + 1) % MODE_COUNT);
}
void ok_tap(Button2& btn) {
  switch (STATE.mode){
    case SIMPLE:          advanceIndex(SIMPLE); break;
    case ROTARY_TABLE:          advanceIndex(ROTARY_TABLE); break;
    case HELICAL:          advanceIndex(HELICAL); break;
  }
}

