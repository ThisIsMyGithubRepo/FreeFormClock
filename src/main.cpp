// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
#include "RTClib.h"
RTC_DS3231 rtc;
// char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#include "TM1637Display.h"
// Define the connections pins:
#define CLK 10
#define DIO 11
// Create display object of type TM1637Display:
TM1637Display display = TM1637Display(CLK, DIO);
const uint8_t data[] = {0xff, 0xff, 0xff, 0xff}; // All Segments On
const uint8_t blank[] = {0x00, 0x00, 0x00, 0x00}; // All Segments Off
// You can set the individual segments per digit to spell words or create other symbols:
/* const uint8_t done[] = {
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
}; */
const int display_BrightnessMax = 7;
const int display_BrightnessMin = 1;

#include "Eeprom24C32_64.h"
#define EEPROM_ADDRESS  0x57
static Eeprom24C32_64 eeprom(EEPROM_ADDRESS);
const word addr_Brightness = 0;

// Pin Defs
#define Pin_Hour_Up 3
#define Pin_Hour_Down 2
#define Pin_10Min_Up 5
#define Pin_10Min_Down 4
#define Pin_1Min_Up 7
#define Pin_1Min_Down 6
#define Pin_Brightness 8
#define Pin_Spare 9

// Global Config
uint8_t cfg_Brightness = 7;



struct button
{
  bool raw_old = 0;
  bool raw_new = 0;
  bool value_debounced = 0;
  bool falling_edge_latch = 0;
  bool rising_edge_latch = 0;
};

void ProcessButton(button *b, bool NewValue)
{
    b->raw_old = b->raw_new;
    b->raw_new = NewValue;

    if((b->raw_old == b->raw_new) && (b->raw_new != b->value_debounced))
    {
      // Value has settled, and the new value doesn't match the last debounced value.
      //Capture the debounced value
      b->value_debounced = b->raw_new;

      // Detect the edge
      if(b->raw_new==1)
      {
        b->rising_edge_latch = true;
        //Serial.println("Rising Edge");
      }
      else
      {
        b->falling_edge_latch = true;
        //Serial.println("Falling Edge");
      } 
    }    
}

int get12h_FormatHours(int hours_24h_format)
{
  int hours_12h_format;
  if (hours_24h_format > 12) 
  {
      hours_12h_format = hours_24h_format - 12; // Subtract 12 if the hours are above 12.
  } 
  else
  {
      hours_12h_format = hours_24h_format;
  }
  
  if(hours_12h_format == 0) {hours_12h_format = 12;}
  return hours_12h_format;
}

void ShowTime(DateTime datevalue)
{
  static uint8_t TimeData[4]; 

  // Decode into digits
  int hours_12h_format;
  int h10;
  int h1;
  int m10;
  int m1;
  int seconds; // If we are in an even second

  hours_12h_format = get12h_FormatHours(datevalue.hour());

  h10 = hours_12h_format / 10;
  h1 = hours_12h_format - h10*10;
  m10 = datevalue.minute() / 10;
  m1 = datevalue.minute() - m10*10;
  seconds = datevalue.second();

  // We don't want t leading zero (e.g. 1:23 not 01:23)
  if(h10 !=0)
    { TimeData[0] = display.encodeDigit(h10);} // Not a zero, leave the digits.
  else
    { TimeData[0] = 0;} // Blank the digit.

  TimeData[1] = display.encodeDigit(h1) | (SEG_DP * (seconds/2) ); // Show the : every even second
  TimeData[2] = display.encodeDigit(m10);
  TimeData[3] = display.encodeDigit(m1);

  display.setSegments(TimeData);
}

void startUp_Display()
{
    // Clear the display:
    display.setBrightness(cfg_Brightness);
    display.setSegments(data);
    delay(1000);
    display.clear();
}

void setupPins()
{
    pinMode(Pin_Hour_Up, INPUT_PULLUP);
    pinMode(Pin_Hour_Down, INPUT_PULLUP);
    pinMode(Pin_10Min_Up, INPUT_PULLUP);
    pinMode(Pin_10Min_Down, INPUT_PULLUP);
    pinMode(Pin_1Min_Up, INPUT_PULLUP);
    pinMode(Pin_1Min_Down, INPUT_PULLUP);
    pinMode(Pin_Brightness, INPUT_PULLUP);
    pinMode(Pin_Spare, INPUT_PULLUP);
}

void startUp_RTC()
{
    if (! rtc.begin()) {
      Serial.println("Couldn't find RTC");
      while (1);
    }
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, lets set the time!");
      // If the RTC have lost power it will sets the RTC to the date & time this sketch was compiled in the following line
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
}

void startUp_EEPROM()
{
  eeprom.initialize();
}

void load_EEPROM_Settings()
{    
  cfg_Brightness = eeprom.readByte(addr_Brightness);
  Serial.print("Raw Brightness Value from EEPROM: ");
  Serial.println(cfg_Brightness);
  cfg_Brightness = constrain(cfg_Brightness,1,7);
}

void storeBrightness(int val)
{
    Serial.print ("Storing Brightness Value: ");
    Serial.println (val);
    eeprom.writeByte(addr_Brightness,val);
}

void setup () {
  Serial.begin(9600);
  Serial.println("Starting");
  
  setupPins();

  startUp_EEPROM();
  load_EEPROM_Settings();

  startUp_Display();
  startUp_RTC();   
}

void Add1Hr()
{
  rtc.adjust(rtc.now()+TimeSpan(0, 1, 0, 0));
}

void Sub1Hr()
{
  rtc.adjust(rtc.now()+TimeSpan(0, -1, 0, 0));
}

void Add10Min()
{
  DateTime datevalue = rtc.now();
  int mins = datevalue.minute();

  mins = mins +10;
  if (mins > 60) {mins = mins - 60;} // Wrap Minutes
  Serial.println(mins);

  rtc.adjust(DateTime(datevalue.year(),datevalue.month(), datevalue.day(), datevalue.hour(), mins, datevalue.second()));
}

void Sub10Min()
{
  DateTime datevalue = rtc.now();
  int mins = datevalue.minute();

  mins = mins +10;
  if (mins > 60) {mins = mins - 60;} // Wrap Minutes
  Serial.println(mins);

  rtc.adjust(DateTime(datevalue.year(),datevalue.month(), datevalue.day(), datevalue.hour(), mins, datevalue.second()));
}

void Add1Min()
{
  DateTime datevalue = rtc.now();
  int mins = datevalue.minute();
  int m10 = datevalue.minute() / 10;
  int m1 = datevalue.minute() - m10*10;

  m1++;
  if(m1 > 9) {m1 = 0;}
  mins = m10 * 10 + m1;

  rtc.adjust(DateTime(datevalue.year(),datevalue.month(), datevalue.day(), datevalue.hour(), mins, 0)); // Adusting the minutes also sets seconds to 0.
}

void Sub1Min()
{
  DateTime datevalue = rtc.now();
  int mins = datevalue.minute();
  int m10 = datevalue.minute() / 10;
  int m1 = datevalue.minute() - m10*10;

  m1--;
  if(m1 < 0) {m1 = 9;}
  mins = m10 * 10 + m1;

  rtc.adjust(DateTime(datevalue.year(),datevalue.month(), datevalue.day(), datevalue.hour(), mins, 0)); // Adusting the minutes also sets seconds to 0.
}

void loop () 
{
  static DateTime now;
  now = rtc.now();

  ShowTime(now);
  display.setBrightness(cfg_Brightness);

  static button button_Hour_Up;
  ProcessButton(&button_Hour_Up,digitalRead(Pin_Hour_Up));
  if(button_Hour_Up.falling_edge_latch)
  {
    Serial.println("Hour Up Falling Edge");
    button_Hour_Up.falling_edge_latch=false;
    Add1Hr();
  }
 
  static button button_Hour_Down;
  ProcessButton(&button_Hour_Down,digitalRead(Pin_Hour_Down));
  if(button_Hour_Down.falling_edge_latch)
  {
    Serial.println("Hour Down Falling Edge");
    button_Hour_Down.falling_edge_latch=false;
    Sub1Hr();
  }

  static button button_10Min_Up;
  ProcessButton(&button_10Min_Up,digitalRead(Pin_10Min_Up));
  if(button_10Min_Up.falling_edge_latch)
  {
    Serial.println("10Min Up Falling Edge");
    button_10Min_Up.falling_edge_latch=false;
    Add10Min();
  }
  
  static button button_10Min_Down;
  ProcessButton(&button_10Min_Down,digitalRead(Pin_10Min_Down));
  if(button_10Min_Down.falling_edge_latch)
  {
    Serial.println("10Min Down Falling Edge");
    button_10Min_Down.falling_edge_latch=false;
    Sub10Min();
  }
  
  static button button_1Min_Up;
  ProcessButton(&button_1Min_Up,digitalRead(Pin_1Min_Up));
  if(button_1Min_Up.falling_edge_latch)
  {
    Serial.println("1Min Up Falling Edge");
    button_1Min_Up.falling_edge_latch=false;
    Add1Min();
  }
  
  static button button_1Min_Down;
  ProcessButton(&button_1Min_Down,digitalRead(Pin_1Min_Down));
  if(button_1Min_Down.falling_edge_latch)
  {
    Serial.println("1Min Down Falling Edge");
    button_1Min_Down.falling_edge_latch=false;
    Sub1Min();
  }
  
  static button button_Brightness;
  ProcessButton(&button_Brightness,digitalRead(Pin_Brightness));
  if(button_Brightness.falling_edge_latch)
  {
    Serial.println("Brightness Falling Edge");
    button_Brightness.falling_edge_latch=false;
    cfg_Brightness++;
    if (cfg_Brightness > display_BrightnessMax) {cfg_Brightness = display_BrightnessMin;} // Wrap brightness from max back to min.
    storeBrightness(cfg_Brightness); // Persist to the EEPROM.
  }
  
    delay(50);
}


