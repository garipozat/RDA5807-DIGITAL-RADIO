
/// Arduino port | SI4703 signal | RDA5807M signal
/// :----------: | :-----------: | :-------------:
///          GND | GND           | GND   
///         3.3V | VCC           | -
///           5V | -             | VCC
///           A5 | SCLK          | SCLK
///           A4 | SDIO          | SDIO
///           D2 | RST           | -


#include <LiquidCrystal.h>
#include <Wire.h>
#include <radio.h>
#include <rda5807M.h>
//#include <si4703.h>
//#include <si4705.h>
//#include <tea5767.h>
#include <RDSParser.h>

// The keys available on the keypad
enum KEYSTATE {  KEYSTATE_NONE, KEYSTATE_SELECT, KEYSTATE_LEFT, KEYSTATE_UP, KEYSTATE_DOWN, KEYSTATE_RIGHT } __attribute__((packed));

// ----- forwards -----
// You need this because the function is not returning a simple value.
KEYSTATE getLCDKeypadKey();



// Define some stations available at your locations here:
// 89.40 MHz as 8940
int deneme [15]={
8800,
9270,
9440,
9690,
9880,
9910,
10200,
10400,
10430,
10470,
10550,
10630,
10800
};



int i=0;
/// The radio object has to be defined by using the class corresponding to the used chip.
/// by uncommenting the right radio object definition.
// RADIO radio;       ///< Create an instance of a non functional radio.
RDA5807M radio;    ///< Create an instance of a RDA5807 chip radio
// SI4703   radio;    ///< Create an instance of a SI4703 chip radio.
// SI4705   radio;    ///< Create an instance of a SI4705 chip radio.
// TEA5767  radio;    ///< Create an instance of a TEA5767 chip radio.


/// get a RDS parser
RDSParser rds;
/// State definition for this radio implementation.
enum RADIO_STATE {
  STATE_PARSECOMMAND, ///< waiting for a new command character.
  
  STATE_PARSEINT,     ///< waiting for digits for the parameter.
  STATE_EXEC          ///< executing the command.
};

RADIO_STATE state; ///< The state variable is used for parsing input characters.

// - - - - - - - - - - - - - - - - - - - - - - - - - -


// LCD PİNLERİ   (RS  E   D4 D5 D6 D7)
LiquidCrystal lcd(10, 11, 9, 8, 7, 6);


/// This function will be when a new frequency is received.
/// Update the Frequency on the LCD display.
void DisplayFrequency(RADIO_FREQ f)
{
  char s[12];
  radio.formatFrequency(s, sizeof(s));
  lcd.setCursor(0, 0); 
  lcd.print(s);

} // DisplayFrequency()


/// This function will be called by the RDS module when a new ServiceName is available.
/// Update the LCD to display the ServiceName in row 1 chars 0 to 7.
void DisplayServiceName(char *name)
{
  size_t len = strlen(name);
  lcd.setCursor(0, 1);
  lcd.print(name);
  while (len < 8) {
    lcd.print(' ');
    len++;  
  } // while
  lcd.setCursor(8, 1);
  lcd.print("--TOMTAL");
} // DisplayServiceName()

/// This function will be called by the RDS module when a rds time message was received.
/// Update the LCD to display the time in right upper corner.
void DisplayTime(uint8_t hour, uint8_t minute) {
  lcd.setCursor(11, 0); 
  if (hour < 10) lcd.print('0');
  lcd.print(hour);
  lcd.print(':');
  if (minute < 10) lcd.print('0');
  lcd.println(minute);
} // DisplayTime()


// - - - - - - - - - - - - - - - - - - - - - - - - - -


void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) {
  rds.processData(block1, block2, block3, block4);
}


/// This function determines the current pressed key but
/// * doesn't return short key down situations that are key bouncings.
/// * returns a specific key down only once.
KEYSTATE getLCDKeypadKey() {
  static unsigned long lastChange = 0;
  static KEYSTATE lastKey = KEYSTATE_NONE;

  unsigned long now = millis();
  KEYSTATE newKey;

  // Get current key state
  int v = analogRead(A0); // the buttons are read from the analog0 pin
  
  if (v < 100) {
    newKey = KEYSTATE_RIGHT;
  } else if (v < 200) {
    newKey = KEYSTATE_UP;
  } else if (v < 400) {
    newKey = KEYSTATE_DOWN;
  } else if (v < 600) {
    newKey = KEYSTATE_LEFT;
  } else if (v < 800) {
    newKey = KEYSTATE_SELECT;
  } else {
    newKey = KEYSTATE_NONE;
  }

  if (newKey != lastKey) {
    // a new situation - just remember but do not return anything pressed.
    lastChange = now;
    lastKey = newKey;
    return (KEYSTATE_NONE);

  } else if (lastChange == 0) {
    // nothing new.
    return (KEYSTATE_NONE);

  } if (now > lastChange + 50) {
    // now its not a bouncing button any more.
    lastChange = 0; // don't report twice
    return (newKey);

  } else {
    return (KEYSTATE_NONE);

  } // if
} // getLCDKeypadKey()


/// Setup a FM only radio configuration with I/O for commands and debugging on the Serial port.
void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("   GARIP OZAT  ");
  lcd.setCursor(0, 1);
  lcd.print("TURKOZU OGUZHAN");
  delay(1000);
  lcd.clear();

  // Initialize the Radio 
  radio.init();
  radio.setBandFrequency(RADIO_BAND_FM, 10470); // AÇILIŞTA GÖSTERİLECEK KANAL
  radio.setMono(false);
  radio.setMute(false);
  radio.setVolume(3);
  state = STATE_PARSECOMMAND;
  // setup the information chain for RDS data.
  radio.attachReceiveRDS(RDS_process);
  rds.attachServicenNameCallback(DisplayServiceName);
  rds.attachTimeCallback(DisplayTime);
} // Setup


/// Constantly check for serial input commands and trigger command execution.
void loop() {
  int newPos;
  unsigned long now = millis();
  static unsigned long nextFreqTime = 0;
  static unsigned long nextRadioInfoTime = 0;
  
  // some internal static values for parsing the input
  static char command;
  static int16_t value;
  static RADIO_FREQ lastf = 0;
  RADIO_FREQ f = 0;
  KEYSTATE k = getLCDKeypadKey();
  if (k == KEYSTATE_RIGHT) 
    {
    radio.seekUp(true);
    } 
   else if (k == KEYSTATE_UP)
    {
    int v = radio.getVolume();
    if (v < 15) radio.setVolume(++v);
    }
   else if (k == KEYSTATE_DOWN) 
   {
    // decrease volume
    int v = radio.getVolume();
    if (v > 0) radio.setVolume(--v);
    }
    else if (k == KEYSTATE_LEFT) 
    {
    radio.seekDown(true);
    }
    else if (k == KEYSTATE_SELECT) 
    {
           if (i < 15)
            { i=i+1;
              radio.setFrequency(deneme[i]);
            }
           else{i=0;
                radio.setFrequency(deneme[i]);
            }
      } 
else {
    // 
     }

  // check for RDS data
  radio.checkRDS();

  // update the display from time to time
  if (now > nextFreqTime) {
    f = radio.getFrequency();
    if (f != lastf) {
      // print current tuned frequency
      DisplayFrequency(f);
      lastf = f;
    } // if
    nextFreqTime = now + 400;
  } // if  

} // loop

// End.

