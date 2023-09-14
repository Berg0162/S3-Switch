/**
 *      S3-Switch (Smart-Solar-Surplus-Switch) with LilyGo ESP32S3 T-display
 *
 *      Created during the summer of 2023
 *      Published at: https://github.com/Berg0162/S3-Switch
 *
 * This board and ESP32S3 processor was selected for its excellent specifications and crisp display. 
 * Aside of gaining experience with the TFT_eSPI library (https://github.com/Bodmer/TFT_eSPI),
 * creating a nice visual user interface was a dominant incentive. A lot of inspiration and practical knowledge was obtained 
 * by studying on Youtube: the Volos Projects (https://www.youtube.com/c/VolosProjects). 
 * This ended up in a good working and visually attractive S3-Switch but definitively not the most cost-effective or 
 * simple solution to build!
 *
 * Functionality:
 * Connects to your local WiFi network (you have to supply SSID and its Passphrase)
 * Will autodetect the Homewizard P1 Meter on the same network and connects to it. Every 5 seconds it will poll for new info.
 * Displays actual time retrieved from Internet
 * Displays Switch Status in addition to the Led status of the Relay board
 * Displays Net Power in digits and gauge presentation (surplus: green CCW and consumption: brown/orange CW)
 * Switches the relay ON when Net Power reaches more than or equal -500 kW surplus power (value of your choice)
 * Allows for 4 fixed clock switch moments (and duration) independent of Net Power level, visible in the display (red blocks)
 * Displays time intervals when the smart switch was activated (green ribbon)
 * Has a builtin simple webserver for local access to show status and edit remotely clock switch moments and duration
 * Point a browser to the local fixed IP address: 192.168.2.100 or to: esp32s3.local 
 *
**/

#include <Arduino.h>
#include <bitset>

// You need to set the board specification to handle correct pin assigments and power !!!
#define LILYGO_T_DISPLAY_ESP32_S3

// COMPILER DIRECTIVE to allow/suppress DEBUG messages that help debugging...
// Uncomment "#define DEBUG" to activate
#define DEBUG
// Include these debug utility macros in all cases!
#include "DebugUtils.h"

// COMPILER DIRECTIVE to test constant switching between ON and OFF for detecting 
// spontaneous reset or boot, caused by EMI ?
// Uncomment to activate
//#define TESTONOFF

// IsSwitchedBitArray is stored persistently and normally only reset at midnight,
// COMPILER DIRECTIVE resets the array everytime when booting!!!
// Uncomment to activate
//#define RESETBITARRAYATBOOT

// LittleFS for internal storage of persistent data on the ESP32
#include <FS.h>
#define FILE_READ       "r"
#define FILE_WRITE      "w"
#include <LittleFS.h>
// Managing persistence of some data with LittleFS system
// clockSwitchTime array Data is written to a file 
#define SWITCHDATAFILENAME "/switchprsdata.txt"
#define ISSWITCHEDFILENAME "/switcheddata.txt"
#define FORMAT_LITTLEFS_IF_FAILED true
// LittleFS--------------------------------------------------------------

// Necessary libraries for use of TFT display and/or connected devices
#include <SPI.h>
#include <TFT_eSPI.h>
#include "Orbitron_Medium_20.h"
#include "Orbitron_Bold_32.h"
#include "NotoSansBold15.h"
#define AA_FONT_SMALL NotoSansBold15
#include "pin_config.h"

#define DISPLAY_BACKLIGHT PIN_LCD_BL
#define MINBRIGHTNESS   0
#define NORMBRIGHTNESS  7
#define MAXBRIGHTNESS   9
const int TFT_BRIGHTNESS[MAXBRIGHTNESS+1]={0,46,47,48,50,55,60,100,175,255};

TFT_eSPI TFT = TFT_eSPI();
TFT_eSprite aTime = TFT_eSprite(&TFT);
TFT_eSprite gauge = TFT_eSprite(&TFT);
//TFT_eSprite data = TFT_eSprite(&TFT);
TFT_eSprite span = TFT_eSprite(&TFT);
TFT_eSprite socket = TFT_eSprite(&TFT);

#define GAUGEMARKERS
// Markers are mutual excluding!
#ifdef GAUGEMARKERS
 #define MARKEVERY90    // Set a gauge marker at every 90 degrees OR
 #ifndef MARKEVERY90
  #define MARKEVERY10   // Set a gauge marker at every 10 degrees
 #endif
#endif

#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>

// STAtion Mode: Use a static/fixed Ip address
IPAddress local_IP(192, 168, 2, 100);
// Set your Gateway IP address
IPAddress gateway(192, 168, 2, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

HTTPClient http;

IPAddress p1MeterIP; // Homeward P1 device IP address is NOT fixed but dynamically assigned
String p1MeterURL = ""; // Looks like: http://###.###.###.###/api/v1/data
const String serverName = "esp32s3"; // ESP32 Hosted server name
// Android versions (<= 8.0.0) are known NOT to resolve (correctly) mDNS hostnames !!
// The following 100% correct serverURL will therefore NOT work, with Android !!!
// const String serverURL ="http://" + serverName + ".local/";
String serverURL = ""; // Set value later when IP is known
// Set web server port number to 80
WiFiServer webServer(80);

// HTTP Header JSON parser
#include <ArduinoJson.h>

#include "time.h"
// Global values for presentation
char timeHour[3]="00";
char timeMin[3]="00";
char timeSec[3];
char m[4];
char y[5];
char d[3];
// Global values for calculation 
int Hour=0;
int Min=0;

#define HWEPOLLINGTIMESPAN   5000
unsigned long HWEtimeInterval = 0;

// 4 Clock Switch moments since midnight, hrs, min and duration min
#define ROWS 4
#define COLS 3
uint8_t clockSwitchTime[ROWS][COLS] = {{15, 35, 60},{11, 00, 60},{05, 30, 60},{21, 30, 60}}; //sort later!
// Only for ease of reading/coding
#define HOUR1 clockSwitchTime[0][0]
#define MIN1  clockSwitchTime[0][1]
#define DUR1  clockSwitchTime[0][2]
#define HOUR2 clockSwitchTime[1][0]
#define MIN2  clockSwitchTime[1][1]
#define DUR2  clockSwitchTime[1][2]
#define HOUR3 clockSwitchTime[2][0]
#define MIN3  clockSwitchTime[2][1]
#define DUR3  clockSwitchTime[2][2]
#define HOUR4 clockSwitchTime[3][0]
#define MIN4  clockSwitchTime[3][1]
#define DUR4  clockSwitchTime[3][2]
// --------------------------------

#define MININADAY 1440  // 24 hrs --> 24*60 = 1440 minutes in a day
//bool clockSwitchArray[MININADAY] -->  1 byte (8 bits) used to store only ONE binary state: 
// True == 1 (ON) or False == 0 (OFF)
// Use a bit array instead of a byte array to store 24 hrs of switch ON/OFF timespans
// -> 1440/8 = 180 bytes instead of 1440*8 = 11.520 bytes of bool storage space!
std::bitset<MININADAY> clockSwitchBitArray;

// store switch time in "00:00" format
char SwitchTime1[6];
char SwitchTime2[6];
char SwitchTime3[6]; 
char SwitchTime4[6];

#define INVALIDPOS std::string::npos
// Variable to store the HTTP request header
std::string header;

// Time of request handling started
unsigned long requestTimeout = 0; 
// Define timeout time in milliseconds 2000ms = 2s
const long TIMEOUT = 2000;

// Set the GPIO for switching the relay
// Default setting on the relay breakout board is HIGH is ACTIVE
#define RELAYPIN 43   // -> Pin 43 connected to pin IN of 5V Relais 1-Channel

// Minimal Power SURPLUS to switch socket ON 
#define POWERSURPLUS -500 // kW --> Notice the negative value -> surplus power!
#define BANDWIDTH     400 // kW Only when 400 below POWERSURPLUS relay/socket is switched OFF
#define MAXWATTPEAK  2900 // kW Maximum value to be shown in the gauge presentation (full circle filled!!)
const int STEPSIZE = int(round(MAXWATTPEAK/360)); // calculate kW step size per circle degree

long NetPower = 0;
//float totImportPower = 0.0;
//float totExportPower = 0.0;
bool socketIsSwitchedON = false; // Current switch status
// Switch timespan presentation is limited to number of columns
#define MAXNUMOFRECORDS 170 // limited by horizontal display axis
//bool socketIsSwitchedArray[MAXNUMOFRECORDS+1] = {false};
// Use a bit array -> 170/8 = 22 bytes instead of 170*8 = 1.360 bytes
std::bitset<MAXNUMOFRECORDS+6> socketIsSwitchedBitArray;

bool queryService(const char* host, const char* service, const char* proto);
void fillClockSwitchBitArray(void);
void setLocalTime(void);
void sort2DArray(uint8_t arr[ROWS][COLS]);
String ip2String(IPAddress ip);
#ifdef DEBUG
void printClockSwithTime(void);
#endif
bool getSwitchTimePRSdata(void);
void setSwitchTimePRSdata(void);
bool getIsSwitchedBitArrayPRSdata(void);
void setIsSwitchedBitArrayPRSdata(void);
void showMessage(int cnt, String message);
void ShowLocalTime(void);
void ShowGaugePresentation(void);
//void ShowDataFields(void);
void ShowTimeSeries(void);
void ShowSocketSwitchStatus(void);
void getHWE_P1_Data(void);
bool setSocketStatus(bool switchOn);
bool checkPowerLevel(void);
void sendHTTP200OK(WiFiClient newClient);
String buildIsSwitchedPathSVG(void);
void sendDefaultResponse(WiFiClient newClient);
int getValueName(const std::string& name, std::size_t& pos);
void sendSchemeResponse(WiFiClient newClient);
void handleRequest(WiFiClient newClient);

void setup() {
#if defined(LILYGO_T_DISPLAY_ESP32_S3) 
    // Pin 15 needs to be set to HIGH in order to boot without USB connection
    pinMode(PIN_POWER_ON, OUTPUT); // to boot with battery...
    digitalWrite(PIN_POWER_ON, HIGH);  // and/or power from 5v rail instead of USB
#endif
/* Setup the LilyGo push buttons on the appropriate pins
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);
*/
    //Setup the relay pin default HIGH is ACTIVE
    pinMode(RELAYPIN, OUTPUT);    // Set for output
    digitalWrite(RELAYPIN, LOW);  // default start: Relay is NOT active
#ifdef DEBUG
    Serial.begin(115200);
    while (!Serial) 
      delay(10); 
    Serial.flush();
    delay(1000);
    DEBUG_PRINTLN("Start S3-Switch with LilyGo ESP32S3 T-display!");
    DEBUG_PRINTLN("--------------- Version 02.0 -----------------");
    delay(1000);
#endif
    // LittleFS start the Littlefilesystem lib and see if we have persistent data ----
    // This opens LittleFS with a root subdirectory /littlefs/
    //DEBUG_PRINTLN("Formatting LittleFS filesystem");
    //LittleFS.format();
    if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        DEBUG_PRINTLN("LittleFS Mount Failed!");
    }
    // Get or set the values of relevant and crucial variables
    // to persistence, with a Browser the user can set these remotely!
    // Get or set the values of clockSwitchTime[4][3] in PRSdata.
    if (!getSwitchTimePRSdata()) {
      setSwitchTimePRSdata();
    }
    // idem for switch history
    if (!getIsSwitchedBitArrayPRSdata()) {
      setIsSwitchedBitArrayPRSdata();
    }
    #ifdef RESETBITARRAYATBOOT
      socketIsSwitchedBitArray.reset();
    #endif

    // LittleFS------------------------------------------------------------------------

    // Setup T-Display 170x320
    TFT.init();
    TFT.fillScreen(TFT_BLACK);
    TFT.setRotation(0);  // 0 == USB downward 2 == USB Upward facing
    TFT.setSwapBytes(true);  
    //Set font for setup messages  
    TFT.loadFont(AA_FONT_SMALL); 
    TFT.setTextColor(TFT_YELLOW, TFT_BLACK, true);

#if defined(DISPLAY_BACKLIGHT) && (DISPLAY_BACKLIGHT >= 0)
    // Enable backlight pin, initially on
    pinMode(DISPLAY_BACKLIGHT, OUTPUT);
    analogWrite(DISPLAY_BACKLIGHT, TFT_BRIGHTNESS[NORMBRIGHTNESS]);
#endif

    configTime(3600, 3600, NTP_SERVER1);
// Setup ShowTime --------------------------------------------------------------------------------------
#define timeX    0 // 32
#define timeY    0
#define timeW  102
#define timeH   46
    aTime.createSprite(timeW, timeH);
    aTime.loadFont(AA_FONT_SMALL);
    aTime.setSwapBytes(true);

 // Setup Gauge graph ----------------------------------------------------
#define gaugeX    0
#define gaugeY   50
#define gaugeW  170
#define gaugeH  170
#define gaugeIR  67 // 47 57
#define gaugeOR  84 // 68 78
    gauge.createSprite(gaugeW, gaugeH);
    gauge.setSwapBytes(true);

/*
 // Setup Data graph ----------------------------------------------------
#define dataX    0
#define dataY  224
#define dataW  170
#define dataH   96
    data.createSprite(dataW, dataH);
    data.setSwapBytes(true);
*/

 // Setup Data graph ----------------------------------------------------
#define spanX    0
#define spanY  224
#define spanW  170
#define spanH   64
#define spanL   25  // central line
    span.createSprite(spanW, spanH);
    span.setSwapBytes(true);

 // Setup socket graph ----------------------------------------------------
#define socketX  108
#define socketY    0
#define socketW   62
#define socketH   46
#define socketOR  18 // Spot Outer radius
#define socketIR  14 // Spot Inner radius
    socket.createSprite(socketW, socketH);
    socket.setSwapBytes(true);
// -----------------------------------------------------------------------

// Setup for STAtion Mode only
    WiFi.mode(WIFI_STA);
    // Connect to Wi-Fi network like a STAtion with SSID and password  
    // Configure static/fixed Wifi Station IP address
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      DEBUG_PRINTLN("Config Static IP Address Failed to configure!");
    }
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);   
    String messageSTR = "Connect to WIFI "; // Hold messages within Setup phase
    DEBUG_PRINT(messageSTR);
    showMessage(1, messageSTR);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      messageSTR += ".";
      showMessage(1, messageSTR);
      DEBUG_PRINT(".");
    }
    DEBUG_PRINTLN();
    showMessage(2, "Done!");
    DEBUG_PRINT("Connected to ");
    DEBUG_PRINTLN(WIFI_SSID);
    DEBUG_PRINT("IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
    // Use local IP Address in URL to avoid any problems with Android phones
    serverURL = "http://" + ip2String(WiFi.localIP()) + "/";

    messageSTR = "Setting Local Time!";
    DEBUG_PRINTLN(messageSTR);
    showMessage(3, messageSTR);
    setLocalTime();
    showMessage(4, "Done!");

    sort2DArray(clockSwitchTime);
#ifdef DEBUG
      printClockSwithTime();
#endif
    fillClockSwitchBitArray();

    messageSTR = "Start ESP32 mDNS!";
    showMessage(5, messageSTR);
    DEBUG_PRINTLN(messageSTR);
    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp32.local"
    // - second argument is the IP address to advertise 
    //   we send our IP address on the WiFi network
    if (!MDNS.begin(serverName)) {
        DEBUG_PRINTLN("Fatal Error setting up mDNS responder!");
        showMessage(4, "Failed! ");
        while(1) {
            delay(1000);
        }
    }
    showMessage(6, "Done! ");
    DEBUG_PRINTLN("ESP32 mDNS started!");

    messageSTR = "Get P1 Meter Service!";
    DEBUG_PRINTLN(messageSTR);
    showMessage(7, messageSTR);
    if( !queryService("p1meter", "hwenergy", "tcp") ){
      DEBUG_PRINTLN("Fatal Error: NO P1-Meter device found!");
      showMessage(8, "Failed! ");
      while(1) {
          delay(1000);
      }
    }
    showMessage(8, "Done! ");
    p1MeterURL = "http://" + ip2String(p1MeterIP) + "/api/v1/data";

    // Start a simple WebServer for handling page requests
    // Set WiFi STAtion webserver with port number 80
    webServer = WiFiServer(WiFi.localIP(), 80);
    webServer.begin();
    messageSTR = "HTTP Server started!";
    DEBUG_PRINTLN(messageSTR);
    showMessage(9, messageSTR);
    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
    delay(2000);
    // Clear screen and start Sprites to be shown
    TFT.fillScreen(TFT_BLACK);
    TFT.fillSmoothRoundRect(0, 292, 170, 28, 4, TFT_DARKGREY);
    TFT.setTextColor(TFT_YELLOW, TFT_DARKGREY, true);
    TFT.drawString(serverURL, 6, 298);
    // Go !!
}

void showMessage(int cnt, String message) {
    cnt *= 30; // calculate pixel amount: 30 px per line
    TFT.drawString(message, 0, cnt); 
}

bool queryService(const char* host, const char* service, const char* proto){
    DEBUG_PRINTF("Browsing for service _%s._%s.local. ... ", service, proto);
    int n = MDNS.queryService(service, proto);
    if (n == 0) {
        DEBUG_PRINTLN("No services found");
    } else {
        DEBUG_PRINT(n);
        DEBUG_PRINTLN(" service(s) found");
        for (int i = 0; i < n; ++i) {
            // Print details for each service found
            DEBUG_PRINT("  ");
            DEBUG_PRINT(i + 1);
            DEBUG_PRINT(": ");
            DEBUG_PRINT(MDNS.hostname(i));
            DEBUG_PRINT(" (");
            DEBUG_PRINT(MDNS.IP(i));
            DEBUG_PRINT(":");
            DEBUG_PRINT(MDNS.port(i));
            DEBUG_PRINTLN(")");
            if(MDNS.hostname(i).startsWith(host)) {
              p1MeterIP = MDNS.IP(i);
              return true;
            }
        }
    }
    return false;
}


void sort2DArray(uint8_t arr[ROWS][COLS]) {
  uint8_t temp[1][COLS]= {0, 0, 0};
  int i, j;
  for(i = 0; i < ROWS; i++) {
    for(j = 0; j < (ROWS-1); j++) {
      if(arr[j][0] > arr[i][0]) {
          memcpy(temp[1], arr[i], sizeof(arr[i]) );
          memcpy(arr[i], arr[j], sizeof(arr[j]) );
          memcpy(arr[j], temp[1], sizeof(temp[1]) );	
      }
    }
  }
}

String ip2String(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

#ifdef DEBUG
void printClockSwithTime(void){
      DEBUG_PRINT("Clock switch moments and duration: ");
      for(int i = 0; i < ROWS; i++) {
        DEBUG_PRINTF("%02d:%02d [%d] ", clockSwitchTime[i][0], clockSwitchTime[i][1], clockSwitchTime[i][2]);
      }
      DEBUG_PRINTLN();
}
#endif

// LittleFS --------------------------------------------------
bool getSwitchTimePRSdata(void) { // Get clockSwitchTime[4][3] from persistent file
  if (LittleFS.exists(SWITCHDATAFILENAME)) {
    File file = LittleFS.open(SWITCHDATAFILENAME, FILE_READ);
    if (file) {    
      uint32_t readLen = file.read((byte *)&clockSwitchTime, sizeof(clockSwitchTime));
      DEBUG_PRINTLN(F("Got switch time data from persistent file storage"));
#ifdef DEBUG
      printClockSwithTime();
#endif
      file.close();
      return true;
    }    
  }
  return false;
}

void setSwitchTimePRSdata(void) { // Set clockSwitchTime[4][3] to persistent file
  File file = LittleFS.open(SWITCHDATAFILENAME, FILE_WRITE);
  if (file) {
      file.write((byte *)&clockSwitchTime, sizeof(clockSwitchTime));
      file.close();
      DEBUG_PRINTLN(F("Set new switch time data in persistent file storage"));
#ifdef DEBUG
      printClockSwithTime();
#endif
  }
}

// socketIsSwitchedBitArray
bool getIsSwitchedBitArrayPRSdata(void) { // Get socketIsSwitchedBitArray from persistent file
  if (LittleFS.exists(ISSWITCHEDFILENAME)) {
    File file = LittleFS.open(ISSWITCHEDFILENAME, FILE_READ);
    if (file) {    
      uint32_t readLen = file.read((byte *)&socketIsSwitchedBitArray, sizeof(socketIsSwitchedBitArray));
      DEBUG_PRINTLN(F("Got Is-Switched-Bit-Array data from persistent file storage"));
      file.close();
      return true;
    }    
  }
  return false;
}

void setIsSwitchedBitArrayPRSdata(void) { // Set socketIsSwitchedBitArray to persistent file
  File file = LittleFS.open(ISSWITCHEDFILENAME, FILE_WRITE);
  if (file) {
      file.write((byte *)&socketIsSwitchedBitArray, sizeof(socketIsSwitchedBitArray));
      file.close();
  }
}

// LittleFS --------------------------------------------------

void fillClockSwitchBitArray(void) {
  int TimeInMin = 0;
  clockSwitchBitArray.reset(); // Set all to zero == false
  Serial.println("Fill Switch Array!");
  for( int j=0; j<4; ++j) { // 0 - 3 (!)
      // calculate minutes since midnight
      Hour = clockSwitchTime[j][0];
      Min = clockSwitchTime[j][1];
      TimeInMin = (Hour*60 + Min);
      // fill array while ON time duration in minutes
      for(int i=TimeInMin; i<(TimeInMin+clockSwitchTime[j][2]); i++ ){
        clockSwitchBitArray.set(i, true);
      }
  }
}

void ShowLocalTime(void){
    setLocalTime();
    aTime.fillScreen(TFT_BLACK);
    aTime.setTextColor(TFT_WHITE, TFT_DARKGREY, true);
    aTime.fillSmoothRoundRect(0, 0, 30, 24, 4, TFT_DARKGREY);
    aTime.fillSmoothRoundRect(36,0, 30, 24, 4, TFT_DARKGREY);
    aTime.fillSmoothRoundRect(72,0, 30, 24, 4, TFT_DARKGREY);
    aTime.drawString(String(timeHour),  6, 6, 4);
    aTime.drawString(String(timeMin),  42, 6, 4);
    aTime.drawString(String(timeSec),  78, 6, 4);
    aTime.fillSmoothRoundRect(0, 28, timeW, 18, 4, TFT_DARKGREY);
    aTime.drawString(String(d)+" "+String(m)+" "+String(y), 8, 30);
    aTime.pushSprite(timeX, timeY);
}

void setLocalTime() {
  struct tm timeinfo;
  uint8_t cnt = 0;

  while(!getLocalTime(&timeinfo)) {
    DEBUG_PRINT(".");
    if(cnt++ == 5) break;
    delay(2000);
  }
  if(cnt>4) {
    DEBUG_PRINTLN(" -> Failed to obtain time!");
    memset(timeHour, 0, sizeof(timeHour));
    memset(timeMin, 0, sizeof(timeMin));
    memset(timeSec, 0, sizeof(timeSec));
    return; 
  }
  // Get presentation values
  strftime(timeHour,3, "%H", &timeinfo);
  strftime(timeMin,3, "%M", &timeinfo);
  strftime(timeSec,3, "%S", &timeinfo);
  strftime(y,5, "%Y", &timeinfo);
  strftime(m,4, "%b", &timeinfo);
  strftime(d,3, "%d", &timeinfo);
  // Set globals for calculation
  Hour = atoi(timeHour);
  Min = atoi(timeMin);
  // Start of a new day so clear the switch-ON-history of the day-before
  if( (Hour == 0) && (Min == 0) ) // During first 60 seconds clear bit array
    socketIsSwitchedBitArray.reset();
    setIsSwitchedBitArrayPRSdata(); // Store the new setting!
}

void ShowGaugePresentation(void) { 
  gauge.fillSprite(TFT_BLACK);
  gauge.fillSmoothRoundRect(0, 0, gaugeW, gaugeH, 4, TFT_DARKGREY);
  gauge.setFreeFont(&Orbitron_Bold_32); //&Orbitron_Medium_20);
  gauge.fillSmoothCircle(85, 85, gaugeIR-2, TFT_BLACK, TFT_DARKGREY); 
  gauge.setTextColor(TFT_CYAN, TFT_BLACK, true);
  char str[6] = {0};
  sprintf(str, "%d", NetPower); 
  int posX = 85 - gauge.textWidth(String(str))/2;
  gauge.drawNumber(NetPower, posX, 65);
#ifndef GAUGEMARKERS
  gauge.drawSmoothArc(85, 85, gaugeOR, gaugeIR, 0, 360, TFT_LIGHTGREY, TFT_DARKGREY); // NO  markers
#elif defined(MARKEVERY90)
 for(int j=0;j<4;j++){ // every 90 degrees a marker
    gauge.drawSmoothArc(85, 85, gaugeOR, gaugeIR, (j*90), ((j+1)*90)-2, TFT_LIGHTGREY, TFT_DARKGREY);
  }
 #elif defined(MARKEVERY10)
  for(int j=0;j<36;j++){ // every 10 degrees a marker
    gauge.drawSmoothArc(85, 85, gaugeOR, gaugeIR, (j*10), ((j+1)*10)-2, TFT_LIGHTGREY, TFT_DARKGREY);
    if(j%9 == 0) // extra tick every 90 degrees
      gauge.drawSmoothArc(85, 85, gaugeOR, gaugeIR, (j*10)-1, (j*10)+1, TFT_YELLOW, TFT_DARKGREY); 
  }
#endif
  // Draw a segment for showing Surplus Power Threshold Value
  int steps = 90 + POWERSURPLUS/STEPSIZE;
  if(steps <= 0) steps += 360; // We are beyond position 0/360 so correct value to move CCW
  gauge.drawSmoothArc(85, 85, gaugeOR-10, gaugeIR-5, steps, 90, TFT_GREEN, TFT_DARKGREY); // Write Surplus Power Value segment
  // ---------------------------------------------------
  // Maximise Power consumption (positive value) also to MAXWATTPEAK to avoid overflow!
  if(NetPower > MAXWATTPEAK) NetPower = MAXWATTPEAK;
  // Settle for rounding steps around zero between +5 and -5
  steps = (int)round(NetPower/STEPSIZE);
  if(steps == 0) { (NetPower>=0) ? steps = 5 : steps = -5; } 
  steps += 90; // --> Start at postion 90 of the gauge
  DEBUG_PRINTF("NetPower: [%d] steps: [%d] \n", NetPower, steps);
  if(NetPower >= 0) { // Power consumption !!!
      gauge.drawSmoothArc(85, 85, gaugeOR, gaugeIR, 90, steps, TFT_ORANGE, TFT_DARKGREY); //, true);
  } else { // Power surplus
      if(steps <= 0) steps += 360; // We are beyond position 0/360 so correct value to move CCW
      gauge.drawSmoothArc(85, 85, gaugeOR, gaugeIR, steps, 90, TFT_GREENYELLOW, TFT_DARKGREY); //, true); TFT_DARKGREEN
    }
  gauge.pushSprite(gaugeX, gaugeY);
}

/*
void ShowDataFields(void){
    data.fillScreen(TFT_BLACK);
    data.setFreeFont(&Orbitron_Medium_20);
    data.setTextColor(TFT_BLACK, TFT_DARKGREY, true);
    data.fillSmoothRoundRect(0, 0, dataW, 28, 4, TFT_DARKGREY);
    data.fillSmoothRoundRect(0,32, dataW, 28, 4, TFT_DARKGREY);
    data.fillSmoothRoundRect(0,64, dataW, 28, 4, TFT_DARKGREY);
    data.drawFloat(totImportPower, 3, 6, 2);
    data.drawFloat(totExportPower, 3, 6, 32+2);
    data.drawFloat(100.123, 3, 6, 64+2);
    data.pushSprite(dataX, dataY);
}
*/

void ShowTimeSeries(void) {
    span.fillScreen(TFT_BLACK);
    span.setFreeFont(&Orbitron_Medium_20);
    span.setTextFont(1);
    span.setTextColor(TFT_WHITE, TFT_DARKGREY, true);
    span.fillSmoothRoundRect(0, 0, spanW, spanH, 4, TFT_DARKGREY);
    // Draw time intervals the switch was ON with surplus power
    for(int i=0;i<MAXNUMOFRECORDS;i++){
//      if(socketIsSwitchedArray[i] == true)
      if(socketIsSwitchedBitArray.test(i) == true)
       span.drawFastVLine(spanX+i, spanL-20, 30, TFT_GREEN);
    }
    // Draw the time-intervals the switch is programmed to be ON, surplus or not !
    int cnt = 0;
    for(int i=5*60; i<(22*60); i+=6) { // Presentation only between 05:00 and 22:00
      if(clockSwitchBitArray.test(i)) {
        span.drawFastVLine(cnt, spanL, 20, TFT_RED); // 10
      }
      cnt++;
    }
    span.drawFastHLine(spanX, spanL+10, spanW, TFT_BLACK); // horizontal axis
    // Draw 16 vertical axis ticks every 10 pixels
    for(int i=1;i<18;i++) {
      if(i==3 || i==9 || i==15) { // draw 9, 15 and 21
        span.drawFastVLine((spanX+(i+1)*10), spanL+3, 15, TFT_BLACK); 
        span.drawString(String(i+6), (spanX+(i+1)*10-5), spanL+25);
      } if(i==1 || i==7 || i==13) { // draw 6, 12, 18
          span.drawFastVLine(spanX+(i*10), spanL, 20, TFT_BLACK); 
          span.drawString(String(i+5), spanX+(i*10)-5, spanL+25);
        } else
            span.drawFastVLine(spanX+(i*10), spanL+5, 10, TFT_BLACK);
    } 
    span.pushSprite(spanX, spanY);
}

void ShowSocketSwitchStatus(void){
    socket.fillScreen(TFT_BLACK);
    socket.fillSmoothRoundRect(0, 0, socketW, socketH, 4, TFT_DARKGREY);
    socket.drawSpot((socketW/2), (socketH/2), socketOR, TFT_LIGHTGREY, TFT_DARKGREY);
    if(socketIsSwitchedON)
      socket.drawSpot((socketW/2), (socketH/2), socketIR, TFT_RED, TFT_LIGHTGREY);
    else      
      socket.drawSpot((socketW/2), (socketH/2), socketIR, TFT_DARKGREY, TFT_LIGHTGREY);
    socket.pushSprite(socketX, socketY);
};

#ifdef TESTONOFF
#define SIMMAX 12
long simData[SIMMAX] = {-600, 0, -600, 0, -600, 0, -600, 0, -600, 0, -600, 0};

long generateNetPowerValue(void) {
  static int cnt = 0;
  if (cnt++ == SIMMAX-1 ) cnt= 0;
  return simData[cnt];
}
#endif

void getHWE_P1_Data(void) {
  // WARNING:   -----------------------------------------------------------------------------
  // HWE-P1 will refuse future http connections when a http session is not closed correctly !!
  // Remedy: Unmount physically device from P1 port and wait for 10 seconds -> remount
  // Device will reconnect to the active Homewizard Energy-App without human interference
  //-----------------------------------------------------------------------------------------
    // configure targeted server and p1MeterURL
    WiFiClient client;
    http.begin(client, p1MeterURL);
    // Allow for fetching streams
    http.useHTTP10(true);
  // ------------------------------------------------------------------------------------------
    // Ckeck WiFi connection
    if((WiFi.status() == WL_CONNECTED)) {
          // Send GET HTTP header
        int httpCode = http.GET();
        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            // Header found at server
            if(httpCode == HTTP_CODE_OK) {
                // Parse response
                DynamicJsonDocument doc(2048);
                deserializeJson(doc, http.getStream());
                // Read values
                //DEBUG_PRINT("Total gas m3: "); 
                //DEBUG_PRINT(doc["total_gas_m3"].as<float>());
                //DEBUG_PRINT(" Timestamp: ");               
                //DEBUG_PRINT(doc["gas_timestamp"].as<long long>());
                //totImportPower = doc["total_power_import_kwh"].as<float>();               
                //totExportPower = doc["total_power_export_kwh"].as<float>();         
#ifdef TESTONOFF
                NetPower = generateNetPowerValue();
#else
                NetPower = doc["active_power_w"].as<long>();
#endif
                //DEBUG_PRINT("  Netto Power: ");
                //DEBUG_PRINTLN(NetPower);
            }
        } else {
            DEBUG_PRINTF("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
    }
    http.end(); // close http session (see above !)
    client.stop(); // close the temporary client
}

bool setSocketStatus(bool switchOn) {
  if(socketIsSwitchedON != switchOn) {
    (socketIsSwitchedON = switchOn) ? digitalWrite(RELAYPIN, HIGH) : digitalWrite(RELAYPIN, LOW);
    delay(200);
  }
  return socketIsSwitchedON;
}

bool checkPowerLevel(void) {
    if( NetPower < POWERSURPLUS ) { // switch ON when netPower is above POWERSURPLUS value
      return setSocketStatus(true);
    }
    if( socketIsSwitchedON && (NetPower <= (POWERSURPLUS+BANDWIDTH)) ) // allow for a bandwidth
      return setSocketStatus(true);
    int indexVal = (Hour*60 + Min); // calculate index
    if(clockSwitchBitArray.test(indexVal)) { // check if a fixed clock interval is active
      return !setSocketStatus(true);
    }
    // all other cases switch is OFF
    return setSocketStatus(false);
}

void sendHTTP200OK(WiFiClient newClient) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            newClient.println("HTTP/1.1 200 OK");
            newClient.println("Content-type:text/html");
            newClient.println("Connection: close");
            newClient.println();
}

void sendSwitchResponse(WiFiClient newClient) {
            if(header.find("GET /switch/on") != INVALIDPOS) { 
              setSocketStatus(true); 
            } else if(header.find("GET /switch/off") != INVALIDPOS) {
                setSocketStatus(false); 
              } else if(header.find("GET /switch") != INVALIDPOS) { // Just keep present switch state: unchanged!
                } else {
                  newClient.print("HTTP/1.1 404 Not Found\r\n\r\n"); 
                  return;
                }
            String state = (socketIsSwitchedON) ? "ON" : "OFF";
            // Construct HTML response web page and send
            newClient.println("<!DOCTYPE html><html>");
            newClient.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            // CSS to style elements
            newClient.println("<style>html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            newClient.println("a.button {-webkit-appearance: button; -moz-appearance: button; appearance: button; text-decoration: none;}"); 
            newClient.print(".button { display: inline-block; background-color: grey; border-radius: 10px; padding: 5px; width: 200px;");
            newClient.println(" font-size: 30px; cursor: pointer; color: white; border: 4px double #cccccc; margin: 5px;}");
            newClient.println(".buttonON {background-color: grey; color: black}");
            newClient.println(".buttonOFF {background-color: black; color: white}");
            newClient.println("</style>");
            // CSS ends and Web Page Body starts
            newClient.println("</head><body>");
            // Insert <div>'s for content separation
            newClient.println("<div style=\"border: 1px solid black; background-color: #eee; width: 340px; border-radius: 10px; line-height: 2;\">");
            newClient.println("<h1>LILYGO ESP32S3</h1><p>Manually set switch ON or OFF!</p>");
            // Display current state, and ON/OFF button
            newClient.println("<p>Socket Switch State: <b>" + state  + "</b></p>");
            // If the socketIsSwitchedON is false, it displays the ON button       
            if (socketIsSwitchedON == false) {
              newClient.println("<p><a href=\"" + serverURL + "switch/on\"><button class=\"button buttonON\">ON</button></a></p>");
            } else {
              newClient.println("<p><a href=\"" + serverURL + "switch/off\"><button class=\"button buttonOFF\">OFF</button></a></p>");
            } 
            newClient.println("<br></div></body></html>");         
            // The HTTP response ends with another blank line
            newClient.println();
}

void sendDefaultResponse(WiFiClient newClient) {
            // Construct HTML response web page and send
            newClient.println("<!DOCTYPE html><html>");
            newClient.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            newClient.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style elements
            newClient.println("<style>html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            newClient.println("a.button {-webkit-appearance: button; -moz-appearance: button; appearance: button; text-decoration: none;}"); 
            newClient.print(".button { display: inline-block; background-color: grey; border-radius: 10px; padding: 5px; width: 200px;");
            newClient.println(" cursor: pointer; color: white; border: 4px double #cccccc; margin: 5px;}");
            newClient.println(".button:hover { background-color: silver; color: black;}");
            newClient.println("</style>");
            // CSS ends and Web Page Body starts
            newClient.println("</head><body>");
            // Insert <div>'s for content separation
            newClient.println("<div style=\"border: 1px solid black; background-color: #eee; width: 340px; border-radius: 10px; line-height: 2;\">");
            newClient.println("<h1>LILYGO ESP32S3</h1><p>Select one of the following options!</p>");
            newClient.println("<div><a href=\"" + serverURL + "status\" class=\"button\">Show status info</a></div>");
            newClient.println("<div><a href=\"" + serverURL + "scheme\" class=\"button\">Edit clock switching</a></div>");
            newClient.println("<div><a href=\"" + serverURL + "switch\" class=\"button\">Switch ON or OFF</a></div>");
            newClient.println("<br></div></body></html>");         
            // The HTTP response ends with another blank line
            newClient.println();
}

String buildIsSwitchedPathSVG(void){
  String temp = "<path d=\"";
    for(int i=0;i<MAXNUMOFRECORDS;i++){
        if(socketIsSwitchedBitArray.test(i) == true) { // first occurrence
          int M = i;
          //M 20 0 L 30 0 
          while(socketIsSwitchedBitArray.test(i) == true) { // loop until end of sequence
            i++;
          }
          int L = i;
          temp += "M " + String(M) + " 0 L " + String(L) + " 0 ";
        }
    }
    temp += "\" ";
    return temp;
}

void sendStatusResponse(WiFiClient newClient) {
            // Construct HTML response web page and send
            newClient.println("<!DOCTYPE html><html>");
            newClient.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            newClient.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style elements
            newClient.println("<style>html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            newClient.println(" .small {font: normal 6px sans-serif; } .left {text-align: left; padding-left: 5px;}");
            newClient.println("</style>");
            // CSS ends and Web Page Body starts
            newClient.println("</head><body>");
            // Insert <div>'s for content separation
            newClient.println("<div style=\"border: 1px solid black; background-color: #eee; width: 340px; border-radius: 10px; line-height: 1;\">");
            newClient.println("<h1>LILYGO ESP32S3</h1>");
            newClient.println("<p class=\"left\">Time: " + String(timeHour) + ":" + String(timeMin) + ":" + String(timeSec) + " &nbsp;&nbsp;&nbsp;Date: " + \
                              String(d) + " " + String(m) + " " + String(y) + "</p>");
            newClient.println("<p class=\"left\">Wifi network: " + String(WIFI_SSID) + "</p>");
            newClient.println("<p class=\"left\">Host: <b>" + serverName + ".local</b> &nbsp;IP: <b>" + ip2String(WiFi.localIP()) + "</b></p>");
            newClient.println("<p class=\"left\">Dev.: " + MDNS.hostname(0) + " IP: " + ip2String(MDNS.IP(0)) + "</p>");           
            char str1[6] = {0};
            sprintf(str1, "%04d", NetPower);
            char str2[6] = {0};
            sprintf(str2, "%04d", POWERSURPLUS);
            newClient.println("<p class=\"left\">Net Power: <b>" + String(str1) + "</b>&nbsp;&nbsp;(Surplus threshold: " + String(str2) + ")</p>");
            String state = (socketIsSwitchedON) ? "ON" : "OFF";
            newClient.println("<p class=\"left\">Socket Switch State: <b>" + state  + "</b></p>");
            newClient.println("<p>Fixed switch moments and duration:</p>");
            // Fill with latest data
            sprintf(SwitchTime1, "%02d:%02d", HOUR1, MIN1); // leading zero is mandatory in HTTP header
            sprintf(SwitchTime2, "%02d:%02d", HOUR2, MIN2); // leading zero is mandatory in HTTP header
            sprintf(SwitchTime3, "%02d:%02d", HOUR3, MIN3); // leading zero is mandatory in HTTP header           
            sprintf(SwitchTime4, "%02d:%02d", HOUR4, MIN4); // leading zero is mandatory in HTTP header
            char buffer[52];
            sprintf(buffer, "%s [%d] %s [%d] %s [%d] %s [%d]",SwitchTime1,DUR1,SwitchTime2,DUR2,SwitchTime3,DUR3,SwitchTime4,DUR4);
            newClient.println("<p><b> "+ String(buffer) + "</b></p>");
            newClient.println("<p>Surplus switching history:</p>");
            newClient.println("<svg viewBox=\"0 0 180 10\" xmlns=\"http://www.w3.org/2000/svg\">\n");
            newClient.println("<rect x=\"1\" width=\"178\" height=\"10\" fill-opacity=\"10%\"/>");
            newClient.println("<text x=\"7\" y=\"9\" class=\"small\">06</text>"); // Exact postion is in the center of double digits
            newClient.println("<text x=\"37\" y=\"9\" class=\"small\">09</text>");
            newClient.println("<text x=\"67\" y=\"9\" class=\"small\">12</text>");
            newClient.println("<text x=\"97\" y=\"9\" class=\"small\">15</text>");
            newClient.println("<text x=\"127\" y=\"9\" class=\"small\">18</text>");
            newClient.println("<text x=\"157\" y=\"9\" class=\"small\">21</text>");
            newClient.println(buildIsSwitchedPathSVG());
            newClient.println("style=\"fill:none;stroke:green;stroke-width:7px;stroke-opacity:1\"/>\n</svg>");
            newClient.println("</div></body></html>");         
            // The HTTP response ends with another blank line
            newClient.println();
}

int getValueName(const std::string& name, std::size_t& pos){
  std::size_t found = header.find(name, pos);
  if (found!=INVALIDPOS) {
    pos = found + name.length();
    std::string tmp = header.substr(pos, 2); // Extract only 2 digit integers
    return std::stoi(tmp);
  } else return 0;
}

void sendSchemeResponse(WiFiClient newClient) {
            // Parse NEW time and duration values, if we had a valid GET header
            if (header.find("GET /?Id=scheme") != INVALIDPOS) {
              DEBUG_PRINTLN("Parsed new time and duration values!");
              /* this caues a stack overflow... fatal crash...
              sscanf((const char*)header.c_str(),"GET /?Id=scheme&appt1=%d%%3A%d&dur1=%d&appt2=%d%%3A%d&dur2=%d&appt3=%d%%3A%d&dur3=%d&appt4=%d%%3A%d&dur4=%d HTTP/1.1", 
                      &HOUR1, &MIN1, &DUR1, &HOUR2, &MIN2, &DUR2, &HOUR3, &MIN3, &DUR3, &HOUR4, &MIN4, &DUR4);
              */
              std::size_t pos = 0; // Keep track of position in header string while parsing continues!
              HOUR1 = getValueName("&appt1=", pos);
              MIN1 = getValueName("%3A", pos);
              DUR1 = getValueName("&dur1=", pos);
              HOUR2 = getValueName("&appt2=", pos);
              MIN2 = getValueName("%3A", pos);
              DUR2 = getValueName("&dur2=", pos);
              HOUR3 = getValueName("&appt3=", pos);
              MIN3 = getValueName("%3A", pos);
              DUR3 = getValueName("&dur3=", pos);
              HOUR4 = getValueName("&appt4=", pos);
              MIN4 = getValueName("%3A", pos);
              DUR4 = getValueName("&dur4=", pos);
              DEBUG_PRINTLN("Updating values!");
              // Sort the clock switch values
              sort2DArray(clockSwitchTime);
#ifdef DEBUG
              printClockSwithTime();
#endif
              /* setPRSdata(); */
              setSwitchTimePRSdata();
              // Reset and refill the array with the new clock switch ON timespans
              fillClockSwitchBitArray();
            }
            // Taken the (new) values fill SwitchTime# variables to be inserted in the response header 
            sprintf(SwitchTime1, "%02d:%02d", HOUR1, MIN1); // leading zero is mandatory in HTTP header
            sprintf(SwitchTime2, "%02d:%02d", HOUR2, MIN2); // leading zero is mandatory in HTTP header
            sprintf(SwitchTime3, "%02d:%02d", HOUR3, MIN3); // leading zero is mandatory in HTTP header           
            sprintf(SwitchTime4, "%02d:%02d", HOUR4, MIN4); // leading zero is mandatory in HTTP header
            // Construct HTML response web page and send
            newClient.println("<!DOCTYPE html><html>");
            newClient.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            newClient.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style elements
            newClient.println("<style>html {font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}"); 
            newClient.println("input[type=number] {padding-left: 5px; text-align: right; width:3em; height:18px;}");
            newClient.print("input[type=submit] { background-color: grey; border-radius: 10px; padding: 5px; width: 200px;");
            newClient.println(" font-size: 1em; height:48px; cursor: pointer; color: white; border: 4px double #cccccc; margin: 5px;}");
            newClient.println("@media (hover: hover) { input[type=submit]:hover { background-color: silver; color: black; } }");
            newClient.println("input[type=time] {width: 6em;} label {padding-right: 10px;} ");
            newClient.println("</style>");
            // CSS ends and Web Page Body starts
            newClient.println("</head><body>");
            // Insert <div>'s for content separation
            newClient.println("<div style=\"border: 1px solid black; background-color: #eee; width: 340px; border-radius: 10px; line-height: 2;\">");
            // FORM to input (new) switch time and duration settings
            newClient.println("<form action = \"" + serverURL + "\" method=\"GET\">");
            newClient.println("<div>");
            newClient.println("<h1>LILYGO ESP32S3</h1><p>Select <b>Switch ON</b> moments and duration:</p>");
            newClient.println("</div><div>");
            newClient.println("<label for=\"appt1\">[1]</label>");
            newClient.println("<input type=\"hidden\" id=\"Id\" name=\"Id\" value=\"scheme\" />");
            newClient.println("<input type=\"time\" id=\"appt1\" name=\"appt1\" value=\"" + String(SwitchTime1) + "\" min=\"00:00\" max=\"23:00\" required>");
            newClient.println("<input type=\"number\" id=\"dur1\" name=\"dur1\" value=\"" + String(DUR1) + "\" min=\"0\" max=\"200\" required>");
            newClient.println("<small>Min.</small></div><div>");
            newClient.println("<label for=\"appt2\">[2]</label>");
            newClient.println("<input type=\"time\" id=\"appt2\" name=\"appt2\" value=\"" + String(SwitchTime2) + "\" min=\"00:00\" max=\"23:00\" required>");
            newClient.println("<input type=\"number\" id=\"dur2\" name=\"dur2\" value=\"" + String(DUR2) + "\" min=\"0\" max=\"200\" required>");
            newClient.println("<small>Min.</small></div><div>");
            newClient.println("<label for=\"appt3\">[3]</label>");
            newClient.println("<input type=\"time\" id=\"appt3\" name=\"appt3\" value=\"" + String(SwitchTime3) + "\" min=\"00:00\" max=\"23:00\" required>");
            newClient.println("<input type=\"number\" id=\"dur3\" name=\"dur3\" value=\"" + String(DUR3) + "\" min=\"0\" max=\"200\" required>");
            newClient.println("<small>Min.</small></div><div>");
            newClient.println("<label for=\"appt4\">[4]</label>");
            newClient.println("<input type=\"time\" id=\"appt4\" name=\"appt4\" value=\"" + String(SwitchTime4) + "\" min=\"00:00\" max=\"23:00\" required>");
            newClient.println("<input type=\"number\" id=\"dur4\" name=\"dur4\" value=\"" + String(DUR4) + "\" min=\"0\" max=\"200\" required>");
            newClient.println("<small>Min.</small></div><div>");
            // contruct submit button
            newClient.println("<input type=\"submit\" value=\"Update Values\" />");
            newClient.println("</div>");
            // close the form, <div>, body and the page
            newClient.println("</form><br></div>");
            newClient.println("</body></html>");         
            // The HTTP response ends with another blank line
            newClient.println();
}

void handleRequest(WiFiClient newClient){
    uint8_t cntNL = 0;                      // Reset count New Line characters
    DEBUG_PRINTLN("New Client!");          // print a message out in the serial port
    requestTimeout = millis() + TIMEOUT;
    // loop while the client's connected until done or after a timeout
    while (newClient.connected() && (millis() <= requestTimeout) ) { 
      if (newClient.available()) {          // if there's bytes to read from the client,
        char c = newClient.read();          // read a byte, then
#ifdef DEBUG
        Serial.write(c);                    // print header characters to serial monitor
#endif
        header += c;                        // fill the header string
        if (c == '\n' || c == '\r') {       // if the byte is a newline (NL or CR) character
          cntNL++;
          // if you got two newline characters --> end of the client HTTP request
          if(cntNL == 2) {
            // so send an OK-response:
            sendHTTP200OK(newClient);
            if (header.find("GET /?Id=scheme") != INVALIDPOS || header.find("GET /scheme") != INVALIDPOS ) {
              sendSchemeResponse(newClient);
            } else if(header.find("GET /switch") != INVALIDPOS) {
                sendSwitchResponse(newClient);
              } else if (header.find("GET /status") != INVALIDPOS) {
                  sendStatusResponse(newClient);
                } else {
                    sendDefaultResponse(newClient);
                  }
            // Break out of the while loop
            break;
          }
        } 
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    newClient.flush();
    newClient.stop();
    DEBUG_PRINTLN("Client disconnected!\n");
}

void loop() {
    ShowLocalTime();
    WiFiClient client = webServer.available();   // Listen for incoming clients
    if (client) {
        handleRequest(client);
    }
    if (millis() > HWEtimeInterval) { // Get new HWE P1 data
        getHWE_P1_Data();
        if( checkPowerLevel() ) { 
          // register in socketIsSwitchedBitArray index when switched ON -> Green
          int indexVal = ((Hour-5)*60 + Min)/6; // Start record only at AM 05:00 hr
          //DEBUG_PRINTF("Hour: [%d] Min: [%d] Calc index: [%d]\n", Hour, Min, indexVal);
          if((indexVal>=0) && (indexVal<MAXNUMOFRECORDS)) { 
            socketIsSwitchedBitArray.set(indexVal, true);
            setIsSwitchedBitArrayPRSdata();
          }
        }
        HWEtimeInterval = millis() + HWEPOLLINGTIMESPAN;
        ShowGaugePresentation();
        //ShowDataFields();
        ShowTimeSeries();
        ShowSocketSwitchStatus();
    }
}
