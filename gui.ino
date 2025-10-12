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
      break;
    case ROTARY_TABLE:
      displayRotaryTable();
      break;
  }
  
}

void displaySimpleDivisions(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // pick your X offset (here 0) and Y positions for each line

  String header = "Simple Indexing";

  u8g2.drawStr(0, 10, header.c_str());

  //u8g2.drawStr(0, 25, String(num_divisions).c_str());

  u8g2.setFont(u8g2_font_profont10_mf);

  String instructions = "# Divs:";

  u8g2.drawStr(0, 25, instructions.c_str());

  String formatted_divisions = "";

  if(STATE.simple.num_divisions < 10){
    formatted_divisions = "00" + String(STATE.simple.num_divisions);
  } else if(STATE.simple.num_divisions < 100){
    formatted_divisions = "0" + String(STATE.simple.num_divisions);
  } else{
    formatted_divisions = String(STATE.simple.num_divisions);
  }
  u8g2.drawStr(0, 35, formatted_divisions.c_str());
  
  //u8g2.drawStr(5 * (3 - STATE.simple.divisions_current_place) ,42 , "_");
  u8g2.drawHLine(5 * (3 - STATE.simple.divisions_current_place) ,37 , 4);

  String status = "Cur Step:";

  u8g2.drawStr(60, 25, String(status).c_str());

  u8g2.drawStr(60, 35, (String(STATE.simple.current_run_step) + "/" + String(STATE.simple.num_divisions)).c_str());
  
  if(STATE.simple.finished_move){
    u8g2.drawStr(0, 60, String("Indexing Finished!").c_str());
  }

  u8g2.sendBuffer();
}

void displayRotaryTable(){
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // pick your X offset (here 0) and Y positions for each line

  String header = "Rotary Table";

  u8g2.drawStr(0, 10, header.c_str());

  //u8g2.drawStr(0, 25, String(num_divisions).c_str());

  u8g2.setFont(u8g2_font_profont10_mf);

  String instructions = "Degrees";

  u8g2.drawStr(0, 25, instructions.c_str());

  String formatted_degrees = "";

  String moveSign = "+";
  if(STATE.rotary.num_degrees < 0){
    moveSign = "-";
  }

  int absDegrees = abs(STATE.rotary.num_degrees);

  if(absDegrees < 10){
    formatted_degrees = moveSign + "00" + String(absDegrees);
  } else if(abs(STATE.rotary.num_degrees) < 100){
    formatted_degrees = moveSign + "0" + String(absDegrees);
  } else{
    formatted_degrees = moveSign + String(absDegrees);
  }
  u8g2.drawStr(0, 35, formatted_degrees.c_str());
  
  //u8g2.drawStr(5 * (3 - STATE.rotary.degrees_current_place) ,42 , "_");

  if(STATE.rotary.current_adjust_mode == RotaryTable::ROTARY_DEGREES){
    u8g2.drawHLine(5 * (4 - STATE.rotary.degrees_current_place) ,37 , 4);
  } else if (STATE.rotary.current_adjust_mode == RotaryTable::ROTARY_SPEED){
    u8g2.drawHLine(40 ,37 , 7);
  } else if(STATE.rotary.current_adjust_mode == RotaryTable::ROTARY_MODE){
    u8g2.drawHLine(80 ,37 , 15);
  }

  String status = "Speed";


  u8g2.drawStr(40, 25, String(status).c_str());

  u8g2.drawStr(40, 35, (String(STATE.rotary.current_speed_degrees_per_second)  + " DPS").c_str());
  
  status = "Mode";


  u8g2.drawStr(80, 25, String(status).c_str());

  String curRotaryMode = "";

  switch (STATE.rotary.current_start_mode){
    case RotaryTable::CNTR:
      curRotaryMode = "Center";
      break;
    case RotaryTable::CW:
      curRotaryMode = "CW";
      break;
    case RotaryTable::CCW:
      curRotaryMode = "CCW";
      break;
  }

  u8g2.drawStr(80, 35, (String(curRotaryMode)).c_str());

  if(STATE.rotary.finished_move){
    u8g2.drawStr(0, 60, String("Indexing Finished!").c_str());
  }

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