#define FASTLED_ESP8266_RAW_PIN_ORDER //Allows to use D0 instead GPIO14
#define FASTLED_ALLOW_INTERRUPTS 0 //Avoids Flickering of LEDs
//------------------------------------------------------------------------------
// Libraries for DFPlayer Mini MP3 Player
//------------------------------------------------------------------------------
#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h" //https://github.com/DFRobot/DFRobotDFPlayerMini
//------------------------------------------------------------------------------
// Libraries for NeoPixel LED-Matrix
//------------------------------------------------------------------------------
#include "FastLED.h"      //https://github.com/FastLED/FastLED
#include "LEDMatrix.h"    //https://github.com/Jorgen-VikingGod/LEDMatrix
#include "LEDText.h"      //https://github.com/AaronLiddiment/LEDText
#include "FontMatrise.h"  //https://github.com/AaronLiddiment/LEDText
//------------------------------------------------------------------------------
// Libraries for NTP Time over the Internet
//------------------------------------------------------------------------------
#include <NTPtimeESP.h>   //https://github.com/SensorsIot/NTPtimeESP
//------------------------------------------------------------------------------
// I/O PINS
//------------------------------------------------------------------------------
#define DATA_PIN            D4 //NeoPixel LED-Matrix
#define BUSY_PIN            D0 //GPIO14, Playing Status of DFPlayer
#define BUTTON_START_PIN    D7 //Green Button, Start Alarm
#define BUTTON_STOP_PIN     D8 //Red Button, Stop Alarm
#define BUTTON_SHOWTIME_PIN D1 //Yellow Button, Display the time
#define RX_MP3_PIN          D5 //GPIO5, connects to DFPlayer
#define TX_MP3_PIN          D6 //GPIO4, connects to DFPlayer
//------------------------------------------------------------------------------
// MATRIX PARAMETERS
//------------------------------------------------------------------------------
#define COLOR_ORDER         GRB
#define CHIPSET             WS2812B
#define MATRIX_WIDTH        8
#define MATRIX_HEIGHT       8
#define MATRIX_TYPE         VERTICAL_MATRIX
#define BRIGHTNESS          20
//------------------------------------------------------------------------------
// MORE PARAMETERS
//------------------------------------------------------------------------------
#define VOLUME              20        //Initial Volume for Mp3 Files, 0...30
#define FPS                 10        //Frames per Second
#define FPS_DELAY           1000/FPS  //Time in Miliseconds per Frame
#define MAX_COOLDOWN        120       //Amount of Flame Drop, 0...255
//------------------------------------------------------------------------------
// Wi-Fi Settings
//------------------------------------------------------------------------------
const char* ssid      = "INSERT_WIFI_SSID"; // Set your WiFi SSID here
const char* password  = "INSERT_WIFI_PASSWORD"; // Set your WiFi password here
//------------------------------------------------------------------------------
// Alarm Settings
//------------------------------------------------------------------------------
uint8_t alarmHour = 16;     //Set your default Alarm-Time Here
uint8_t alarmMinute = 58;   //Set your default Alarm-Time Here
//------------------------------------------------------------------------------
// Objects
//------------------------------------------------------------------------------
SoftwareSerial mySoftwareSerial(RX_MP3_PIN, TX_MP3_PIN); // RX, TX
DFRobotDFPlayerMini myDFPlayer; //MP3 player module
cLEDMatrix<MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_TYPE> leds;
cLEDText ScrollingMsg; //to sroll the time and display the countdown
NTPtime NTPde("0.de.pool.ntp.org"); //Choosing a german server pool
strDateTime dateTime; //stores date from NTP-Server
WiFiServer server(80); //for the HTML WebInterace
//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------
bool wakeUpProcessStarted = false;
bool wakeUpProcessStopped = false;
bool musicStarted = false;
bool countdownStarted = false;
bool sunriseEndPositionReached = false;
bool firstSongFinished = false;
bool timeIsNTPTime = false;
bool buttonShowTimePressed = false;
bool timeTextUpdated = false;
bool countdownFinished = false;
bool countdownAnimationFinished = false;

uint8_t songCounter = 0;      //Song counter for diagnostics
uint8_t currentNumber = 10;   //Countdown, start out at 10
uint8_t currentHour = 0;      //for the Clock
uint8_t currentMinute = 0;    //for the Clock
uint8_t currentSecond = 0;    //for the Clock
int8_t sunposition = -6;      //Start outside the view field
uint8_t heatIndex = 0;        //Color of the sun

char timeTxt[] = "23:59:59";  //init Time
String readString = "";       //For the HTTP Request
//------------------------------------------------------------------------------
// Forward Declarations
//------------------------------------------------------------------------------
void setupSerial(); //starts a Serial Connection for diagnostics
void setupWiFi(); //connects to WiFI HotSpot and sets up a Webserver
void setupTime(); //initializes the NTPServer and gets the current time
void setupDFPlayer(); //initializes the Mp3 Module
void setupLEDMatrix(); //initializes the LED-Matrix
void setupLEDText(); //sets Font and Color for the LED-Text to be displayed
void setupDigitalInputPins(); //sets pinMode for every I/O Pin

void readInputPins(); //reads digital Input Pins and sets variables accordingly
void checkAlarmTime(); //checks if current time is alarm time and starts alarm

void controlShowTimeSequence(); //displays the time, when button is pressed
void controlWakeupSequence(); //controls the states when alarm is started
void stopWakeUpProcess(); //stops the wakeupSequence
void controlWebsite(); //Builds a website when a client connects

void playFirstSong(); //Plays the first song on the SD-Card, 0001.mp3
void playCountDown(); //Plays the second song on the SD-Card, 0002.mp3
void playNextSongWhenFinished(); //Plays every song on the SD-Card

void showSunrise(); //shows a rising sun on the LED-Matrix
void showCountdown(); //shows a Countdown from 10 to 1 on the LED-Matrix
void showFireAnimation(); //shows a simple Fire Simulation on the LED-Matrix
void showTime(); //shows the time on the LED-matrix
void movingDot();//shows a moving red dot on the LED-Matrix
void drawSun(int16_t xc, int16_t yc, uint16_t r, CRGB Col); //draws a filled circle

void updateTime(); //Gets current time from an NTP Server
void updateTimeText(); //Updates the time to be displayed on the LED-matrix
//------------------------------------------------------------------------------
// setup
//------------------------------------------------------------------------------
void setup(){
  setupSerial();
  setupWiFi();
  setupDFPlayer();
  setupLEDMatrix();
  setupLEDText();
  setupDigitalInputPins();
  setupTime();
  Serial.println(F("Finished Initializing.\n"));
  delay(100);
}

void setupSerial(){
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("#######################"));
  Serial.println(F("  NodeMCU Lichtwecker"));
  Serial.println(F("#######################"));
}

void setupWiFi(){
  Serial.print(F("Connecting to Wi-Fi "));
  Serial.print(ssid);
  Serial.println(F("..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin (ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(F("connected\n"));
  Serial.print(F("Setting up Server..."));
  server.begin();
  Serial.print(F("done\n"));

  Serial.print(F("You can now connect to: http://"));
  Serial.print(WiFi.localIP());
  Serial.println();
}

void setupTime(){
  Serial.print(F("Initializing time..."));
  dateTime = NTPde.getNTPtime(1.0, 1);
  if(dateTime.valid){
    currentMinute = dateTime.minute;
    currentHour = dateTime.hour;
    currentSecond = dateTime.second;
    if(!timeIsNTPTime){
      timeIsNTPTime = true;
    }
  }
  Serial.print(F("done\n"));
}

void setupDFPlayer(){
  Serial.print(F("Initializing DFRobot DFPlayer (May take 3~5 seconds)... "));
  mySoftwareSerial.begin(9600);

  if (!myDFPlayer.begin(mySoftwareSerial)) {
      Serial.println(F("Unable to communicate with DFPlayer"));
    while(true);
  }
  Serial.print(F("done\n"));
  myDFPlayer.volume(VOLUME);  //Set volume value. From 0 to 30
}

void setupLEDMatrix(){
    Serial.print(F("Initializing NeoPixel LED-Matrix... "));
    FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds[0], leds.Size()).setCorrection(TypicalSMD5050);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);
    FastLED.show();
    FastLED.delay(1500);
    Serial.print(F("done\n"));
}

void setupLEDText(){
  Serial.print(F("Initializing LEDText..."));
  //char testTxt[] = "Welcome";
  ScrollingMsg.SetFont(MatriseFontData);
  ScrollingMsg.Init(&leds, leds.Width(), ScrollingMsg.FontHeight() + 1, 0, 0);
  //ScrollingMsg.SetText((unsigned char *)testTxt, sizeof(testTxt) - 1);
  ScrollingMsg.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0xff, 0x00, 0xff);
  Serial.print(F("done\n"));
  while(ScrollingMsg.UpdateText() != -1){
    FastLED.show();
    FastLED.delay(50);
  }
}

void setupDigitalInputPins(){
  Serial.print(F("Initializing Input Pins..."));
  pinMode(BUSY_PIN, INPUT);
  pinMode(BUTTON_START_PIN, INPUT);
  pinMode(BUTTON_STOP_PIN, INPUT);
  pinMode(BUTTON_SHOWTIME_PIN, INPUT);
  Serial.print(F("done\n"));
}
//------------------------------------------------------------------------------
// Helper Functions
//------------------------------------------------------------------------------
/*
Necessary because the DrawFilledCircle Function from the
LEDMatrix Library causes a Crash at the NodeMCU
*/
void drawSun(int16_t xc, int16_t yc, uint16_t r, CRGB Col){
  for(int i = r; i >= 0; i--){
    leds.DrawCircle(xc, yc, i, Col);
      if(i ==  1){
          leds.DrawFilledRectangle(xc-1, yc-1, xc+1, yc+1, Col);
      }
    }
}

//Depracted, use showCountdown instead
//Draws the Countdown manually
void showCountdownOld(){
  EVERY_N_SECONDS(1) {
    if(currentNumber == 10){
      Serial.println("ich bin in 10");
      FastLED.clear(true);
      leds.DrawLine(0, 1, 7, 1, CRGB::Red);
      leds.DrawRectangle(0, 3, 7, 6, CRGB::Red);
      FastLED.show();
    } else if(currentNumber == 9){
      FastLED.clear(true);
      leds.DrawRectangle(4, 2, 7, 5, CRGB::Gold); //y1, x1, y2, x2
      leds.DrawLine(0, 5, 7, 5, CRGB::Gold); //rechts
      leds.DrawLine(0, 2, 0, 5, CRGB::Gold);
      FastLED.show();
    } else if(currentNumber == 8){
      FastLED.clear(true);
      leds.DrawRectangle(4, 2, 7, 5, CRGB::Green); //y1, x1, y2, x2
      leds.DrawRectangle(0, 2, 4, 5, CRGB::Green); //y1, x1, y2, x2
      FastLED.show();
    } else if(currentNumber == 7){
      FastLED.clear(true);
      leds.DrawLine(0, 2, 7, 5, CRGB::Aqua);
      leds.DrawLine(7, 2, 7, 5, CRGB::Aqua);
      FastLED.show();
    } else if(currentNumber == 6){
      FastLED.clear(true);
      leds.DrawRectangle(0, 2, 4, 5, CRGB::LemonChiffon); //y1, x1, y2, x2
      leds.DrawLine(0, 2, 7, 2, CRGB::LemonChiffon); //links
      leds.DrawLine(7, 2, 7, 5, CRGB::LemonChiffon);
      FastLED.show();
    } else if(currentNumber == 5){
      FastLED.clear(true);
      leds.DrawLine(0, 2, 0, 5, CRGB::RosyBrown); //boden
      leds.DrawLine(4, 2, 7, 2, CRGB::RosyBrown); //links unten
      leds.DrawLine(4, 2, 4, 5, CRGB::RosyBrown); //mitte
      leds.DrawLine(0, 5, 4, 5, CRGB::RosyBrown); //rechts oben
      leds.DrawLine(7, 2, 7, 5, CRGB::RosyBrown); //oben
      FastLED.show();
    } else if(currentNumber == 4){
      FastLED.clear(true);
      leds.DrawLine(0, 4, 7, 4, CRGB::BurlyWood); //rechts
      leds.DrawLine(3, 1, 3, 5, CRGB::BurlyWood); //mitte
      leds.DrawLine(3, 1, 7, 4, CRGB::BurlyWood); //links oben
      FastLED.show();
    } else if(currentNumber == 3){
      FastLED.clear(true);
      leds.DrawLine(0, 5, 7, 5, CRGB::Gold); //rechts
      leds.DrawLine(7, 2, 7, 5, CRGB::Gold); //oben
      leds.DrawLine(0, 2, 0, 5, CRGB::Gold); //unten
      leds.DrawLine(4, 2, 4, 5, CRGB::Gold); //mitte
      FastLED.show();
    } else if(currentNumber == 2){
      FastLED.clear(true);
      leds.DrawLine(0, 2, 0, 5, CRGB::Amethyst); //boden
      leds.DrawLine(0, 2, 4, 2, CRGB::Amethyst); //links unten
      leds.DrawLine(4, 2, 4, 5, CRGB::Amethyst); //mitte
      leds.DrawLine(4, 5, 7, 5, CRGB::Amethyst); //rechts oben
      leds.DrawLine(7, 2, 7, 5, CRGB::Amethyst); //oben
      FastLED.show();
    } else if(currentNumber == 1){
      FastLED.clear(true);
      leds.DrawLine(0, 3, 7, 3, CRGB::RoyalBlue);
      FastLED.show();
    }
    currentNumber--;
    if(currentNumber > 10 && currentNumber < 1){
      currentNumber = 10;
    }
  }
}

void showCountdown(){
  EVERY_N_SECONDS(1) {
    if(currentNumber == 10){
      FastLED.clear(true);
      //Drawing the 10 manually, because text doesnt fit in the Matrix
      leds.DrawLine(1, 0, 1, 7, CRGB::Red);
      leds.DrawRectangle(3, 0, 6, 7, CRGB::Red);
    } else if(currentNumber == 9){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 9", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
    } else if(currentNumber == 8){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 8", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
    } else if(currentNumber == 7){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 7", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
    } else if(currentNumber == 6){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 6", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
    } else if(currentNumber == 5){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 5", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
    } else if(currentNumber == 4){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 4", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
    } else if(currentNumber == 3){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 3", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
    } else if(currentNumber == 2){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 2", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
    } else if(currentNumber == 1){
      FastLED.clear(true);
      ScrollingMsg.SetText((unsigned char *)" 1", 2);
      for(int i = 0; i < 6; i++){
        ScrollingMsg.UpdateText();
      }
      countdownAnimationFinished = true;
    }
    FastLED.show();
    if(currentNumber > 1)
      currentNumber--;
  }
}

/*
This functions plays the first Song on the SD-Card.
It is the Backgroundmusic for the Sunrise Sequence.
The filename should be 0001.mp3
*/
void playFirstSong(){
  bool noSongPlaying = digitalRead(BUSY_PIN);
  if (noSongPlaying && !musicStarted) {
    Serial.print(F("Playing first song... ["));
    Serial.print(songCounter = 0);  //reset Counter
    Serial.print(F("]\n"));
    myDFPlayer.play(1);  //Play the first mp3 0001.mp3
    musicStarted = true;
    delay(100);
  } else if (noSongPlaying && musicStarted){
    firstSongFinished = true;
  }
}

/*
This function plays the countDown Voice on the SD-Card.
The filename should be 0002.mp3
*/
void playCountDown(){
  bool noSongPlaying = digitalRead(BUSY_PIN);
  if (noSongPlaying && firstSongFinished & !countdownStarted) {
    Serial.print(F("Playing Coutndown... ["));
    Serial.print(songCounter++);  //reset Counter
    Serial.print(F("]\n"));
    myDFPlayer.play(2);  //Play the first mp3 0001.mp3
    countdownStarted = true;
    delay(100);
  } else if (noSongPlaying && countdownStarted){
    countdownFinished = true;
  }
}

/*
keeps Playing every Song on the SD-Card until the STOP button is pressed
*/
void playNextSongWhenFinished(){
  bool noSongPlaying = digitalRead(BUSY_PIN);
  if (noSongPlaying) {
    Serial.print(F("Playing next song... ["));
    Serial.print(++songCounter);
    Serial.print(F("]\n"));
    myDFPlayer.next();
    delay(100);
  }
}

void readInputPins(){
  if(digitalRead(BUTTON_START_PIN)){
    wakeUpProcessStarted = true;
    wakeUpProcessStopped = false;
  }
  if(digitalRead(BUTTON_STOP_PIN)){
    wakeUpProcessStarted = false;
    wakeUpProcessStopped = true;
  }
  if(digitalRead(BUTTON_SHOWTIME_PIN)){
    buttonShowTimePressed = true;
  }
}

/*Very simplified Version of a Fire Simulation, completly random8
Idea based on Mark Kriegsmans "Fire2012" and Darren Meyers modified verison "FireBoard"
https://github.com/FastLED/FastLED/tree/master/examples/Fire2012
https://github.com/darrenpmeyer/Arduino-FireBoard
*/
void showFireAnimation () {
  static uint8_t base_heat[MATRIX_WIDTH];    //1 temperature per Column
  CRGB array[MATRIX_HEIGHT][MATRIX_WIDTH]; //for later mapping to the LED-Matrix
  random16_add_entropy(random8());

  //new temperature
  for(int i = 0; i < 8; i++){
    base_heat[i] = random8(); //Assigning random number between 0 and 255
  }

  //Filling the Array with a Flame
  for( int i = 0; i < MATRIX_WIDTH; i++) {
    uint8_t heat = base_heat[i];
    //lowering the temperatur every cycle the higher the pixel goes up
    for( int j = 0; j < MATRIX_HEIGHT; j++) {
      array[i][j] = ColorFromPalette( HeatColors_p, heat ); //Fire Colors
      //Cooling down the next higher pixel in the column
      uint8_t cooldown = random8(0,MAX_COOLDOWN);
      if (cooldown > heat)
        heat = 0;
      else
        heat -= cooldown;
    }
  }

  // Mapping the array to the LED-Matrix
  for( int i = 0; i < MATRIX_WIDTH; i++) {
    for( int j = 0; j < MATRIX_HEIGHT; j++) {
       leds[0][(i*MATRIX_WIDTH) + j] = array[i][j];
    }
  }
}

void stopWakeUpProcess(){
  if(musicStarted){
    Serial.print(F("Stopping music... "));
    myDFPlayer.stop();
    musicStarted = false; //so next time the first song will be played
    Serial.print(F("done\n"));
    delay(100);
  }

  FastLED.clear();
  wakeUpProcessStarted = false;
  wakeUpProcessStopped = false;
  firstSongFinished = false;
  sunriseEndPositionReached = false;
  countdownStarted = false;
  countdownFinished = false;
  sunposition = -6;
  heatIndex = 0;
  currentNumber = 10;
  Serial.println(F("Alarm stopped."));
}

void showSunrise(){
  //static int sunposition = -6; //Start outside the view field
  //static uint8_t heatIndex = 0;
  CRGB color = ColorFromPalette(HeatColors_p, heatIndex);
  if(sunposition < 4){
    EVERY_N_MILLISECONDS(1000){
      FastLED.clear(true);
      //leds.DrawFilledCircle(3,++sunposition,3, color);
      drawSun(3, ++sunposition, 3, color);
      heatIndex+=5;
      FastLED.show();
    }
  } else {
    //leds.DrawFilledCircle(3,sunposition,3, color);
    EVERY_N_MILLISECONDS(30){
      drawSun(3,sunposition,3, color);
      FastLED.show();
      if(heatIndex < 254) {
        heatIndex++;
      } else {
        sunriseEndPositionReached = true;
        //sunposition = -6; //Default for next time
        //heatIndex = 0; //Default for next time
      }
    }
  }

}

/*
Phase 1: Rising sun + first song
Phase 2: Countdown + second song
Phase 3: FireAnimation + remaining songs
*/
void controlWakeupSequence(){
  if(wakeUpProcessStarted && (!sunriseEndPositionReached || !firstSongFinished)){
    playFirstSong();
    showSunrise();
  } else if(wakeUpProcessStarted && sunriseEndPositionReached && firstSongFinished && (!countdownFinished || !countdownAnimationFinished)){
    showCountdown();
    playCountDown();
  } else if(wakeUpProcessStarted && sunriseEndPositionReached && countdownFinished ){
    showFireAnimation();
    playNextSongWhenFinished();
  }
  if(wakeUpProcessStopped){
    stopWakeUpProcess();
    currentNumber = 10;
  }
}

/*
First Checks if the current time is updated. If its not updated, then it refreshes
the time and displays the Time on the LED-Matrix.
*/
void controlShowTimeSequence(){
  if(buttonShowTimePressed && !timeTextUpdated){
    updateTimeText();
  }
  if(buttonShowTimePressed && timeTextUpdated){
    showTime();
  }
}


/*
Shows a Moving Red Dot on the LED-Matrix
*/
void movingDot(){
  for(int i = 0; i < 64; i++){
    leds[0][i] = CRGB::Red;
    FastLED.show();
    leds[0][i] = CRGB::Black;
    delay(50);
  }
}

void updateTime(){
  //Increase time manually, without the internet when time is invalid
  EVERY_N_SECONDS(1){
    currentSecond += 1;
    currentSecond %= 60;
    if(currentSecond == 0){
      currentMinute += 1;
      currentMinute %= 60;
      if(currentMinute == 0){
        currentHour += 1;
        currentHour %= 24;
      }
    }
  }

  //Get the Exact time from an NTP Server every 10 Seconds.
  EVERY_N_SECONDS(10){
    dateTime = NTPde.getNTPtime(1.0, 1);
    if(dateTime.valid){
      currentMinute = dateTime.minute;
      currentHour = dateTime.hour;
      currentSecond = dateTime.second;
      if(!timeIsNTPTime){
        //ScrollingMsg.SetTextColrOptions(COLR_RGB,  0x2e, 0x8b, 0x57); //SeaGreen
        timeIsNTPTime = true;
      }
      return;
    } else {
      if(timeIsNTPTime){
        //ScrollingMsg.SetTextColrOptions(COLR_RGB,  0xff, 0x00, 0x00); //Red
        timeIsNTPTime = false;
      }
    }
  }
}

void updateTimeText(){
  String stringCache = "";
  if(currentHour  < 10) stringCache += "0"; //leading zero
  stringCache += String(currentHour);
  stringCache += ":";
  if(currentMinute < 10) stringCache += "0"; //leading zero
  stringCache +=  String(currentMinute);
  stringCache += ":";
  if(currentSecond  < 10) stringCache += "0"; //leading zero
  stringCache += String(currentSecond);
  stringCache.toCharArray(timeTxt, stringCache.length() + 1);
  ScrollingMsg.SetText((unsigned char *)timeTxt, sizeof(timeTxt) - 1);
  timeTextUpdated = true;
}

void showTime(){
  if (ScrollingMsg.UpdateText() == -1){
    buttonShowTimePressed = false;
    timeTextUpdated = false;
  }
}

void checkAlarmTime(){
  if(currentHour == alarmHour && currentMinute == alarmMinute && wakeUpProcessStarted == false){
    Serial.println(F("!! ALARM STARTED !!"));
    wakeUpProcessStarted = true;
    wakeUpProcessStopped = false;
  }
}

void printDebug(){
  static int zaehler = 0;
  EVERY_N_SECONDS(1){
    Serial.println(zaehler++);
    Serial.print("\nwakeUpProcessStarted: ");
    Serial.print(wakeUpProcessStarted);
    Serial.print("\nsunriseEndPositionReached: ");
    Serial.print(sunriseEndPositionReached);
    Serial.print("\nfirstSongFinished: ");
    Serial.print(firstSongFinished);
    Serial.print("\nwakeUpProcessStopped: ");
    Serial.print(wakeUpProcessStopped);
    Serial.print("\ncountdownFinished: ");
    Serial.print(countdownFinished);
    Serial.print("\ncountdownAnimationFinished: ");
    Serial.print(countdownAnimationFinished);
    Serial.println();
  }
}

/*
waits for a client and builds a webseite when someone is connected
*/
void controlWebsite(){
  //Check for Clients connected to the Website
  WiFiClient client = server.available();
  if(client){ //only when client is available
    String request = client.readStringUntil('\r');
    Serial.println("!! Request: ");
    Serial.println(request);

    //evaluating the HTTP Request and setting the variables respectively
    if(request.indexOf("/ALARM_ON") != -1){
      wakeUpProcessStarted = true;
      wakeUpProcessStopped = false;
    }
    if(request.indexOf("/ALARM_OFF") != -1){
      wakeUpProcessStarted = false;
      wakeUpProcessStopped = true;
    }

    if(request.indexOf("HOUR") != -1){
      Serial.println("\n!!NEW ALARM HOUR: ");
      int index = request.indexOf("HOUR");
      uint8_t newHour = request.substring(index + 5, index + 7).toInt();  //convert String into Integer
      alarmHour = newHour;
      Serial.println(request.substring(index));
      Serial.print(request.charAt(request.indexOf("HOUR")+5));
      Serial.print(request.charAt(request.indexOf("HOUR")+6));
      Serial.print(" ");
      Serial.print(newHour);
      Serial.print("ende\n");
    }

    if(request.indexOf("MINUTE") != -1){
      Serial.println("\n!!NEW ALARM MINUTE: ");
      int index = request.indexOf("MINUTE");
      uint8_t newMinute = request.substring(index + 7, index + 9).toInt();
      alarmMinute = newMinute;  //convert String into Integer
      Serial.println(request.substring(index));
      Serial.print(request.charAt(request.indexOf("MINUTE")+7));
      Serial.print(request.charAt(request.indexOf("MINUTE")+8));
      Serial.print(" ");
      Serial.print(newMinute);
      Serial.print("ende\n");
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");
    client.println("<head>");
    client.println("<title>Lichtwecker by Peter Stein</title>");
    client.println("<style>");
    client.println("button {width:150px;height:50px;}");
    client.println(".stop{color: white;background-color:#f44336;}");
    client.println(".start{color: white;background-color:#4caf50;}");
    client.println("</style>");
    client.println("</head>");
    client.println("<body>");
    client.println("<h1>Welcome to the Lichtwecker!</h1>");
    client.println("<p><a href=\"/ALARM_OFF\"><button class=\"stop\">Alarm Stoppen</button></a>");
    client.println("<a href=\"/ALARM_ON\"><button class=\"start\">Alarm Starten</button></a></p>");
    client.println("<br/>");
    client.println("<form method=GET>");
    client.println("<h2> Set Wake Up Time: </h2>");

    client.print("hour: <input type=\"text\" name=\"HOUR\" maxlength=\"2\" size=\"2\" value=\"");
    client.print(alarmHour);
    client.print("\">");

    client.println("minute: <input type=\"text\" name=\"MINUTE\" maxlength=\"2\" size=\"2\" value=\"");
    client.print(alarmMinute);
    client.print("\">");

    client.println("<input type=\"submit\" value=\"set\">");
    client.println("</form>");
    client.println("<br/><br/>");
    client.print("The current time is: ");
    client.print(currentHour);
    client.print(":");
    client.print(currentMinute);
    client.print(":");
    client.print(currentSecond);

    client.println("<br/><br/>");
    client.print("wakeUpProcessStarted: ");
    client.print(wakeUpProcessStarted);
    client.print("<br/>");
    client.print("wakeUpProcessStopped: ");
    client.print(wakeUpProcessStopped);
    client.print("<br/>");
    client.print("musicStarted: ");
    client.print(musicStarted);
    client.print("<br/>");
    client.print("countdownStarted: ");
    client.print(countdownStarted);

    client.println("</body>");
    client.println("</html>");
    delay(10);
  }
}

//------------------------------------------------------------------------------
// loop
//------------------------------------------------------------------------------
void loop(){
  readInputPins();
  updateTime();
  readInputPins();
  controlWebsite();
  controlWakeupSequence();
  controlShowTimeSequence();
  checkAlarmTime();
  FastLED.show();
  FastLED.delay(FPS_DELAY); //Refresh Rate
}
