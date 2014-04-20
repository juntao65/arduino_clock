#include <Wire.h>
#include "RTClib.h" // chronodot for timekeeping
#include <Adafruit_MCP23017.h>  // lcd
#include <Adafruit_RGBLCDShield.h>    // lcd
#include <Time.h>      // for time manipulation
#include <EEPROM.h>    // data persistence
#include "EEPROMAnything.h"  // data persistence
#include <IRLib.h>   //  infrared

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7


// Data persistentce variables
struct config_t
{
    time_t alarm_t;      // unix timestamp for alarm
    time_t closeTime_t;   // unix timestamp for close time (wh
    time_t lighton_t_start;    // timestamp for turning on light starting
    time_t lighton_t_stop;    // timestamp for turning on light stopping
    unsigned long autostop_delay;  // num millisec to delay autostop for curtain
    int bgColor;  // stores bgcolor integer
    byte eventStatus; // if alarm is on or off
    
} conf;

RTC_DS1307 RTC;

// IR setup
int STATUS_PIN = 13;

IRsend My_Sender;

// set raw IR codes to PROGMEM
PROGMEM unsigned int Signal_Open[] = {1157,421,1262,421,421,1262,1262,421,421,1262,421,1262,421,1262,1262,421,421,1262,421,1262,421,1262,421,34895,1262,421,1262,421,421,1262,1262,421,421,1262,421,1262,421,1262,1262,421,421,1262,421,1262,421,1262,421,88722,1262,421,1262,421,421,1262,1262,421,421,1262,421,1262,421,1262,1262,421,421,1262,421,1262,421,1262,421,34895,1262,421,1262,421,421,1262,1262,421,421,1262,421,1262,421,1262,1262,421,421,1262,421,1262,421,1262,421,99924};
//PROGMEM unsigned int Signal_Close[] = {1175,400,1200,400,400,1200,1200,400,400,1200,400,1200,1200,400,400,1200,400,1200,400,1200,400,1200,400,33175,1200,400,1200,400,400,1200,1200,400,400,1200,400,1200,850,750,100,1575,400,1200,400,1200,400,1200,400,84350,1200,400,1200,400,400,1200,1200,400,400,1200,400,1200,925,675,100,1575,400,1200,400,1200,400,1200,400,33175,1200,400,1200,400,400,1200,1200,400,400,1200,400,1200,400,1200,400,1200,400,1200,400,1200,400,1200,400,95000};
PROGMEM unsigned int Signal_Close[] = {1175,400,1200,400,400,1200,1200,400,400,1200,400,1200,1200,400,400,1200,400,1200,400,1200,400,1200,400,408,1200,400,1200,400,400,1200,1200,400,400,1200,400,1200,850,750,100,1575,400,1200,400,1200,400,1200,400,2448,1200,400,1200,400,400,1200,1200,400,400,1200,400,1200,925,675,100,1575,400,1200,400,1200,400,1200,400,408,1200,400,1200,400,400,1200,1200,400,400,1200,400,1200,400,1200,400,1200,400,1200,400,1200,400,1200,400,13160};
PROGMEM unsigned int Signal_Stop[] = {1196,416,1248,416,416,1274,1248,416,416,1274,416,1274,416,1274,416,1274,1248,416,416,1274,416,1274,416,16383,1248,416,1248,416,416,1274,1248,416,416,1274,416,1274,416,1274,416,1274,1248,416,416,1274,416,1274,416,16383,1248,416,1248,416,416,1274,1248,416,416,1274,416,1274,416,1274,416,1274,1248,416,416,1274,416,1274,416,16383,1248,416,1248,416,416,1274,416,1274,416,1274,416,1274,416,1274,416,1274,416,1274,416,1274,416,1274,416,16383}; //AnalysIR Batch Export - RAW
PROGMEM unsigned int Signal_DimUp[] =  {20250,6750,2250,6750,2250,11250,2250,6750,2250,6750,2250,11250,20250,6750,2250,6750,2250,11250,2250,6750,2250,6750,2250,11250};

void setup() {
  // Begin the Serial connection 
  Serial.begin(9600);

  // Instantiate the RTC
  Wire.begin();
  RTC.begin();
  
  // Read from EEPROM
  EEPROM_readAnything(0, conf);
 
  // Check if the RTC is running.
  if (! RTC.isrunning()) {
    Serial.println(F("RTC is NOT running"));
  }

  // This section grabs the current datetime and compares it to
  // the compilation time.  If necessary, the RTC is updated.
  DateTime now = RTC.now();
  DateTime compiled = DateTime(__DATE__, __TIME__);

  if (now.unixtime() < compiled.unixtime()) {
    Serial.println(F("RTC is older than compile time! Updating"));
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  
 
  Serial.println(F("RTC Setup complete."));
  
  
  // LCD SETUP
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  lcd.setBacklight(conf.bgColor);
  
  Serial.print("Setting lcd bgcolor to ");
  Serial.println(conf.bgColor);
  
  // IR setup
  pinMode(STATUS_PIN, OUTPUT);
  
  showCurrTime();
}

byte lastMinute;
byte is_lcdBacklight_on = 1;

void loop() {
  uint8_t buttons = lcd.readButtons();
  
  if (buttons) {
    
    switch(buttons){
      case BUTTON_SELECT:                      // Main Menu
        mainMenu();      // goes to main menu
        showCurrTime();  // now we're back, so lets go back to clock
        delay(200);
        break;
      case BUTTON_UP:                          // Toggle alarm status
        conf.eventStatus++;
        if(conf.eventStatus > 3)  conf.eventStatus = 0;
        save(1);      // silent save
        showCurrTime(); 
        delay(200);
        break;
      case BUTTON_LEFT:
        curtain_move(1,conf.autostop_delay);   // Open
        showCurrTime();  // reshow time
        delay(200);
        break;
      case BUTTON_RIGHT:
        curtain_move(2,conf.autostop_delay);  // Close
        showCurrTime();  // reshow time
        delay(200);
        break;
        
      case BUTTON_DOWN:                      // Toggle LCD backlight
        if(is_lcdBacklight_on){
          lcd.setBacklight(0);
          is_lcdBacklight_on=0;
        }
        else{
          lcd.setBacklight(conf.bgColor);
          is_lcdBacklight_on=1;
        }
        delay(200);
        break;
    }
  }
 
  // Check events if they're on
  // See showCurrTime() fxn for more info
  if(conf.eventStatus == 0 || conf.eventStatus == 1){
    eventCheck(conf.alarm_t,1);    // Check alarm
  }
  if(conf.eventStatus == 1 || conf.eventStatus == 2){
    eventCheck(conf.closeTime_t,2); // Check close time
  }
  
  // check if time to turn dim light on
  eventCheck(conf.lighton_t_start, conf.lighton_t_stop, 3);
  
  
  // Get the current time
  DateTime now = RTC.now(); 
   
  // Refresh clock at every minute change
  if(lastMinute != now.minute()){
    showCurrTime();
    lastMinute=now.minute();
  }
}


// Sends raw code to open curtain
// by retrieving from PGM
// 1 for open
// 2 for close
// 3 for dim up
// 0 for stop
void sendRawCode(byte code_type){
  // get the relevant code from PROGMEM
  int rawLen = sizeof(Signal_Open) / sizeof(unsigned int);;
  unsigned int rawCode[rawLen];
  
  switch(code_type){
    case 0:  for(int i=0; i<rawLen;i++){ rawCode[i]=(pgm_read_word(&Signal_Stop[i]));  } break; // stop
    case 1:  for(int i=0; i<rawLen;i++){ rawCode[i]=(pgm_read_word(&Signal_Open[i]));  } break; // open
    case 2:  for(int i=0; i<rawLen;i++){ rawCode[i]=(pgm_read_word(&Signal_Close[i]));  } break; // close
    
    case 3:  for(int i=0; i<rawLen;i++){ rawCode[i]=(pgm_read_word(&Signal_DimUp[i]));  } break; // dim up

  }
  
  Serial.print(F("Sending raw codetype: "));
  Serial.println(code_type);
  
  My_Sender.IRsendRaw::send(rawCode, sizeof(rawCode)/sizeof(int), 40);
  
}


// Lets user know that it is indeed
// it mane
void dasItMane(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("DAS IT MANE"));
  
  lcd.setCursor(0,1);
  lcd.print(F("..das it"));
  
  delay(1000);
  
}

// Opens or closes curtain, until button press
// if code_type = 1, opens
// otherwise, closes

// Blasts open IR signal
// until either interval is up or button press
void curtain_move(byte code_type, long move_interval){
  // Change LCD
  lcd.clear();
  lcd.setCursor(0,1);
  
  if(code_type == 1){
    lcd.print(F("...opening curts"));
    Serial.println(F("Opening curts"));
  }
  else{
    lcd.print(F("...closing curts"));
    Serial.println(F("Closing curtains"));
  }
  
  // blast code 3 times
  for(int i=0; i<5;i++){
    sendRawCode(code_type);
  }
  
  // When interval time elapses
  // auto stop curtains
  unsigned long currentMillis = millis();
  long previousMillis = currentMillis; 
  
  do{
    currentMillis = millis();    // get current time
    if(currentMillis - previousMillis > move_interval) {
      // save the last time you blinked the number 
      previousMillis = currentMillis;   
     
      curtain_stop();
      break;
     
    }
    // END INTERVAL 
    
    
    // Check for button presses
    uint8_t buttons = lcd.readButtons();
    
    if(buttons){
      curtain_stop();
      delay(200);
      break;

    }
    
  }while(1==1);
  
}

// curtain stop
void curtain_stop(){
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(F("...stopping"));
  Serial.println(F("Stopping curtain"));
  
  // blast code 5 times 
  for(int i=0;i<4;i++){
    sendRawCode(0);
  }
  
}


// EEPROM save
// silent On will make nothing happen to LCD
void save(byte silentOn){
  // Actually save
  EEPROM_writeAnything(0, conf);
  
  // tell user we're saving
  if(!silentOn){
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print(F("...saving"));
    
    delay(1000);
  }
}

// Sets current time to LCD screen
void showCurrTime(){
  DateTime now = RTC.now(); // get curr time
  String print_str;
  
  lcd.clear();            // clean wahtevers on screen
  lcd.setCursor(0,0);
  
  // Get formatted time
  print_str = getFormattedTime(now.hour(),now.minute(),12);
  
  // Log it
  Serial.print(F("Setting the time as: "));
  Serial.print(print_str);
  Serial.println();
  
  // write datdere time
  lcd.print(print_str);
  
  // show alarm status
  lcd.setCursor(10,1);
  switch(conf.eventStatus){
    case 0:
      lcd.print(F("(A)"));
      break;
    case 1:
      lcd.print(F("(A)(C)"));
      break;
    case 2:
      lcd.print(F("   (C)"));
      break;
    case 3:
      lcd.print(F(""));
      break;
  }
  
  
  
  
}

// Takes hour and min and returns in 
// string in 00:00 format.
// Set tFormat to 12 for 12 hour clock
String getFormattedTime(int tHour, int tMin, int tFormat){
  
  String hour_suffix = "";
  
  // If on 12H, conv tHour to 12H
  if(tFormat == 12){
    if(tHour > 12){
      tHour = tHour - 12;
      hour_suffix = "PM";
      
    }else{
      hour_suffix = "AM";
      
    }
    
    if(tHour == 0){
      tHour = 12;
    }
  }
  
  
  // conv single digit min to
  // double digit
  String minute_prefix = "";
  if(tMin <= 9){
    minute_prefix = "0";
    
  }
  
  // setup output string
  String output = String(tHour);
  output = String(output + ":" + minute_prefix + tMin + " " + hour_suffix);
  
  // Return it
  return String(output);
  
}


// Checks if is time to fire event
// 1 - is alarm/open curtain
// 2 - close curtain
// 3 - dim lights on
time_t lastEvent_t=0;  // to prevent from firing multiple times
void eventCheck(time_t eventTime_t_start, time_t eventTime_t_stop, byte action_type){
   // Get the current time
   DateTime now = RTC.now(); 
  
   if( 
   hour(eventTime_t_start) >= now.hour() && minute(eventTime_t_start) == now.minute() 
   && hour(eventTime_t_stop) < now.hour() && minute(eventTime_t_stop) == now.minute() 
   && ((now.unixtime()-lastEvent_t) > 60)        // if hasnt been run since a minute ago
   ){
     
     switch(action_type){
       case 1:
         curtain_move(1,conf.autostop_delay);   // Open
         break;
       case 2:
         curtain_move(2,conf.autostop_delay);   // Close
         break;
       case 3:
         //lights_on();                          // Turn lights on
         sendRawCode(3);  // send dim up signal
         break;
     }
     
     showCurrTime();  // refresh time
     Serial.println(F("Alarm ON DING DING IDNG"));
     lastEvent_t = now.unixtime();
   }
   
  
}



void mainMenu() {
  String menu_firstrow_str = "MENU";
  String menu_secondrow_str;
  
  int menuposition = 0;
  byte num_submenus = 5; //count starts from 0
  byte leave_menu=0;
  
  // counting # times hit select Btn cuz
  // of glitch that would auto fire select 
  // and go to submenu
  byte selectButtonCount = 0;
   
 
  do{
    uint8_t buttons = lcd.readButtons();
    byte go_submenu =0;
   
    // Sift through menu when up/down gets pressed
    switch(buttons) {
      case BUTTON_UP:
        menuposition += 1;
        if(menuposition > num_submenus) menuposition=0;
        delay(200);
        break;
      case BUTTON_DOWN:
        menuposition -= 1;
        if(menuposition < 0) menuposition=num_submenus;
        delay(200);
        break;
      case BUTTON_LEFT:
        leave_menu = 1;
        delay(200);
        break;
      case BUTTON_RIGHT:
        go_submenu = 1;
        delay(200);
        break;
      case BUTTON_SELECT:
        selectButtonCount++;      // to prevent early prefire
        if(selectButtonCount>1){
          go_submenu = 1;
        }
        delay(200);
        break;
 
    }
    
    if(leave_menu == 1){
      break;
    }
    
    //If button is pressed, refresh display
    if(buttons){
      switch(menuposition){
        case 0:                     // Set alarm
          if(go_submenu == 1) showSetTimeMenu(conf.alarm_t,0);
          menu_secondrow_str = " Set alarm";
          break;
        case 1:                    // Set lcd color
          if(go_submenu==1) showSetLCDColorMenu();
          menu_secondrow_str = " Set LCD color";
          break;
        case 2:                    // Set clock time
          {
          time_t clockTime_t = RTC.now().unixtime();   // get time from RTC
          if(go_submenu == 1) showSetTimeMenu(clockTime_t,1);
          menu_secondrow_str = " Set clock time";
          break;
          }
        case 3:                    // Set curtain interval
          if(go_submenu==1) showSetDelayMenu();
          menu_secondrow_str = " Set stop delay";
          break;
        case 4:                    // Set close curtain time
          if(go_submenu==1) showSetTimeMenu(conf.closeTime_t,0);
          menu_secondrow_str = " Set close time";
          break;
        case 5:
          if(go_submenu==1) showSetTimeMenu(conf.lighton_t_start,0);
          menu_secondrow_str = " Set light time";
          break;
        case 6:
          if(go_submenu==1) showSetTimeMenu(conf.lighton_t_stop,0);
          menu_secondrow_str = " Set light time";
          break;
      
      }
      
      // Refresh LCD with relevent text
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(menu_firstrow_str);
      lcd.setCursor(0,1);
      lcd.print(menu_secondrow_str);
      
    }
    
    
    //delay(200);
  }while(1==1);
  
}


// pass the time_t object, and it will get
// modified in set menu
void showSetTimeMenu(time_t &setTime_t,byte is_clock){
  lcd.clear();
  lcd.setCursor(0,0);
  
  lcd.print(F("Set alarm"));
  
  
  int blinkState = LOW;             // used to track last state
  long previousMillis = 0;        // will store last time state was updated
  long interval = 500;           // interval at which to blink (milliseconds)
  
  // where to set lcd.cursor to flash 2 chars
  // start positions of Hrs, Min, and AM/PM
  byte flash_coordinates[] = {1,4,7};  
  int flash_coord_key = 0;
  
  do{
    
    // START TEXT FLASHING
    // check to see if it's time to blink the number; that is, if the 
    // difference between the current time and last time you blinked 
    // the number is bigger than the interval at which you want to 
    // blink the number.
    unsigned long currentMillis = millis();
    
    if(currentMillis - previousMillis > interval) {
      // save the last time you blinked the number 
      previousMillis = currentMillis;   
  
      // if the number is off turn it on and vice-versa:
      if (blinkState == LOW){
        blinkState = HIGH;
        
        // if single digit hour, push cursor 1 over

        if((0 < hour(setTime_t) && hour(setTime_t) <= 9) || (12 < hour(setTime_t) && hour(setTime_t) <= 21))
          lcd.setCursor(2,1);
        else
          lcd.setCursor(1,1);
          
        lcd.print(getFormattedTime(hour(setTime_t),minute(setTime_t),12));

      }
      else{
        blinkState = LOW;
        
        // flash 2 chars starting at
        // the cursor coordinates
        lcd.setCursor(flash_coordinates[flash_coord_key],1);
        lcd.print("  ");
        
      }
      
    }
    // END TEXT FLASHING

    uint8_t buttons = lcd.readButtons();
    byte leave_menu = 0;
   
    // Change numbers when up/down gets pressed
    switch(buttons) {
      case BUTTON_UP:
        switch(flash_coord_key){
          case 0:        // Add hour
            changeTime(setTime_t,60*60);
            break;
          case 1:        // Add minute
            changeTime(setTime_t,60);
            break;
          case 2:        // Toggle AM/PM by adding 12 hours
            changeTime(setTime_t,60*60*12L);
            break;
        }
        delay(200);
        break;
        
      case BUTTON_DOWN:
        switch(flash_coord_key){
          case 0:       // subtract hour
            changeTime(setTime_t,60*60*-1);
            break;
          case 1:        // subtract minute
            changeTime(setTime_t,60*-1);
            break;
          case 2:        // Toggle AM/PM by adding 12 hours
            changeTime(setTime_t,60*60*-12L);
            break;

        }
        delay(200);
        break;
      
      // Go through time elements with LEFT & RIGHT
      case BUTTON_RIGHT:     
        flash_coord_key += 1;
        if(flash_coord_key > 2) flash_coord_key=0;
        delay(200);
        break;
        
      case BUTTON_LEFT:
        flash_coord_key -= 1;
        if(flash_coord_key<0)flash_coord_key=2;
        delay(200);
        break;
      
      // save & exit on SELECT
      case BUTTON_SELECT:
      {
        // Lets save
        if(is_clock)      // update RTC for clock
        {
          lcd.clear();
          lcd.setCursor(0,1);
          lcd.print(F("...saving"));
          RTC.adjust(setTime_t);
          delay(1000);
        }
        else
          save(0);    // update eeprom
        leave_menu = 1;
        delay(200);
        break;
      }
    }
    
    if(leave_menu == 1){
      break;
    }
    
    
  }while(1==1);
 
  
}

// for adding/subtracting seconds on time_t object
// pass by ref changeTime_t
// DOES NOT SAVE TO PERSISTANCE, make sure you save()
void changeTime(time_t &changeTime_t, long add_seconds){
  
  // add secs to unix
  changeTime_t = changeTime_t + add_seconds;
  
  Serial.print(F("Changed alarm to go off at: "));
  Serial.print(hour(changeTime_t));
  Serial.print(F(":"));
  Serial.println(minute(changeTime_t));
  
}

// Set curtain stop delay for opening/closing
void showSetDelayMenu(){
  lcd.clear();
  lcd.setCursor(0,0);
  
  lcd.print(F("Set delay"));

  do{
    lcd.setCursor(1,1);
    lcd.print(conf.autostop_delay/1000);  // show seconds on screen
    lcd.print(F(" secs"));
  
    uint8_t buttons = lcd.readButtons();
    byte leave_menu = 0;
   
    // Change numbers when up/down gets pressed
    switch(buttons) {
      case BUTTON_UP:
         conf.autostop_delay+=1000;
         LCDclearSecondrow();
         delay(200);
         break;
       case BUTTON_DOWN:
         if(conf.autostop_delay>0)
           conf.autostop_delay-=1000;
         LCDclearSecondrow();
         delay(200);
         break;
       case BUTTON_LEFT:
         leave_menu=1;
         delay(200);
         break;
       case BUTTON_SELECT:    // save and exit only on select
         leave_menu=1;
         save(0);
         delay(200);
         break;
     }
     
     if(leave_menu == 1){
       break;
     }
    
  }while(1==1);
  
}

// Clears second row of LCD. 
void LCDclearSecondrow(){
  lcd.setCursor(0,1);
  lcd.print(F("                "));
}


/**
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7
**/
void showSetLCDColorMenu(){
  lcd.clear();
  lcd.setCursor(0,0);
  
  String colors[] = {"OFF","RED","GREEN","YELLOW","BLUE","VIOLET","TEAL","WHITE"};
  
  lcd.print(F("Set LCD color"));
  
  do{
    lcd.setCursor(1,1);
    lcd.print(colors[conf.bgColor]);
    
    uint8_t buttons = lcd.readButtons();
    byte leave_menu = 0;
    
    // Change colors when up/down gets pressed
    switch(buttons) {
      case BUTTON_UP:
        conf.bgColor++;
        if(conf.bgColor>7)  conf.bgColor=0;
        lcd.setBacklight(conf.bgColor);
        LCDclearSecondrow();
        delay(200);
        break;
      case BUTTON_DOWN:
        conf.bgColor--;
        Serial.print("bg color is ");
        Serial.println(conf.bgColor);
        if(conf.bgColor<0)  conf.bgColor=7;
        lcd.setBacklight(conf.bgColor);
        LCDclearSecondrow();
        delay(200);
        break;
      case BUTTON_LEFT:
        save(0);
        leave_menu=1;
        delay(200);
        break;
      case BUTTON_SELECT:
        save(0);
        leave_menu=1;
        delay(200);
        break;
    }
    
    if(leave_menu==1)
      break;
    
  }while(1==1);
  
}

void lights_on(){
  lcd.clear();
  lcd.setCursor(1,1);
  
  lcd.print(F("...lights on"));
  
  int dim_count=1;
  
  
  // INTERVAL
  unsigned long currentMillis = millis();
  long previousMillis = currentMillis; 
  long dim_interval = 60000;    // interval between each dim up
    

  do{
    currentMillis = millis();    // get current time
    if(currentMillis - previousMillis > dim_interval) {
      // save the last time you blinked the number 
      previousMillis = currentMillis;   
     
      sendRawCode(3);  // send dim up signal
      Serial.println(F("Sending dim"));
      dim_count++;
 
     
    }
    // END INTERVAL 
    
    uint8_t buttons = lcd.readButtons();
    
    
    // If press anything then cancel
    if(buttons){
      Serial.println("BUTTON IS PRESSED");
      break;
    }
    
    // After dimmed up fully, stahp
    if(dim_count > 20){
      break;
    }
    
  }while(1==1);
}


