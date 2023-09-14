/**
 *          S3-Switch (Smart-Solar-Surplus-Switch) with Sonoff Basic R2 (ESP8266)
 *
 *          Created in the summer of 2023
 *          Published at: https://github.com/Berg0162/S3-Switch
 *
 * The Sonoff was selected for its very low cost, ESP8266 processor, ready for-the-purpose-package, 
 * it is meant to be hacked and to upload/flash custom firmware.
 *
 * Functionality:
 * Connects to your local WiFi network (you have to supply SSID and its Passphrase)
 * Will autodetect the Homewizard P1 Meter on the same network and connects to it. Every 5 seconds it will poll for new info.
 * Led will indicate switch status: blue blinking -> connecting; blue continuous -> AP mode; red continuous S3-Switch is ON
 * Switches the relay ON when Net Power reaches more than or equal -500 kW surplus (value of your choice)
 * Allows for 4 fixed clock switch moments (and duration) independent of Net Power level
 * Button allows for toggling the switch on or off
 * Has a builtin simple webserver for local access to show status and edit remotely fixed clock switch moments and duration
 * Point a browser to the local fixed IP address: 192.168.2.200 or to: esp8266.local
 *
**/

#include <Arduino.h>
#include <bitset>
#include <string>

// COMPILER DIRECTIVE to allow/suppress DEBUG messages that help debugging...
// Uncomment general "#define DEBUG" to activate
#define DEBUG
// Include these debug utility macros in all cases!
#include "DebugUtils.h"

// LittleFS for internal storage of persistent data on the esp8266
#include <FS.h>
#define FILE_READ       "r"
#define FILE_WRITE      "w"
#include <LittleFS.h>
// Managing persistence of some data with LittleFS system
// clockSwitchTime array Data is written to a file 
#define SWITCHDATAFILENAME "/switchprsdata.txt"
// network properties Data is written to a file
#define WIFIDATAFILENAME "/wifiprsdata.txt"
#define FORMAT_LITTLEFS_IF_FAILED true
// LittleFS--------------------------------------------------------------

// Set the esp8266 to start with Access Point when WiFi credentials are invalid
#define ENABLE_ACCESS_POINT

#ifdef ENABLE_ACCESS_POINT
// Default Network properties: Max length each of 16 !!
#define MAXSIZE 16
std::string ssid(MAXSIZE,'x');
std::string password(MAXSIZE,'x');
#else
// NO Access Point ? -> insert fixed VALID(!) SSID and Password of the WiFi network
std::string ssid = "SSID_WIFI"; 
std::string password = "PASSWORD_WIFI";
#endif

// Set the Access Point network properties
const char* ssid_AP     = "ESP8266-AP";
const char* password_AP = ""; // No PW --> open access

// Time server
#define NTP_SERVER1     "pool.ntp.org"

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>

// Assign a R"rawliteral()";
const std::string form_html = R"(
<!DOCTYPE html><html>
<head><meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;} </style>
</head><body>
<div style="border: 1px solid black; background-color: #eee; width: 340px; border-radius: 10px; line-height: 2;">
<h1>SONOFF ESP8266 AP</h1>
<form action = "" method="GET">
<label for="ssid">WiFi SSID: </label>
<input type="hidden" id="id" name="id" value="data" />
<input type="text" id="ssid" name="ssid" required>
<label for="password">Password: </label>
<input type="password" id="password" name="password" required>
<input type="submit" value="Submit Values" />
</form><br></div>                    
</body></html>)";

// HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
// and end with a Blank line
// Assign a R"rawliteral()";
const std::string http_200_OK = R"(
HTTP/1.1 200 OK
Connection: close
Content-type:text/html)";

// HTML page to confirm receipt of network properties
// Assign a R"rawliteral()";
const std::string confirm_html = R"(
<!DOCTYPE html><html>
<head><meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="icon" href="data:,">
<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;} </style>
</head><body>
<div style="border: 1px solid black; background-color: #eee; width: 340px; border-radius: 10px; line-height: 2;">
<h1>SONOFF ESP8266 AP</h1>
<p>Device has successfully received WiFi network name and passphrase! It will now attempt to connect the WiFi network...</p>
<br></div>                    
</body></html>)";

// STAtion Mode: Use a static/fixed Ip address
IPAddress local_IP(192, 168, 2, 200);
// Set your Gateway IP address
IPAddress gateway(192, 168, 2, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

HTTPClient http;
MDNSResponder MDNS;

IPAddress p1MeterIP; // Homeward P1 device IP address is NOT fixed but dynamically assigned
String p1MeterURL = ""; // Looks like: http://###.###.###.###/api/v1/data
const String serverName = "esp8266"; // esp8266 Host Name
// Android versions (<= 8.0.0) are known NOT to resolve (correctly) mDNS hostnames !!
// The following 100% correct serverURL will therefore NOT work, with Android !!!
// const String serverURL ="http://" + serverName + ".local/";
String serverURL = ""; // Set value later when IP is known

// Set webserver port number to 80
WiFiServer webServer(80); // To be used with AP OR STA later...

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
//bool clockSwitchArray[MININADAY]; // 1 byte (8 bits) used to store only ONE binary state: 
// True == 1 (ON) or False == 0 (OFF)
// Use a bit array instead of a byte array to store 24 hrs of switch ON/OFF timespans
// -> 1440/8 = 180 bytes instead of 1440*8 = 11.520 bytes of bool storage space!
std::bitset<MININADAY> clockSwitchBitArray;

#define INVALIDPOS std::string::npos
// Variable to store the HTTP request header
std::string header;

// store switch time in "00:00" format
char SwitchTime1[6];
char SwitchTime2[6];
char SwitchTime3[6]; 
char SwitchTime4[6];
// Time of request handling started
unsigned long requestTimeout = 0; 
// Define timeout time in milliseconds 2000ms = 2s
const long TIMEOUT = 2000;

// Set the GPIO 12 for switching the relay
// Default setting on the relay is LOW -> NOT ACTIVE
const int PIN_RELAY = 12;   // -> Pin 12 connected to Relais Channel AND builtin Led
const int PIN_LED = 13;     // -> Pin 13 only connected to Sonoff builtin Led
/* ------------------------------------------------------------------
Notice the Sonof Basic R2 board is wired in such a way that pin 12 AND pin 13 
both control the (3 wire) LED color status!
Only at startup when pin 12 (!) is initialized to LOW, these rules apply:
12  13
L   H   Blue
L   L   Off
After pin 12 is set High for the first time the rules change !
H   H   Red
L   H   Off
H   L   Red/Blue combined
L   L   Off
-------------------------------------------------------------------*/
const int PIN_BUTTON = 0; // Used during operation to toggle relais On/Off
unsigned long buttonDownTime = 0;
byte lastButtonState = 1;
byte buttonPressHandled = 0;

// Minimal Power SURPLUS to switch socket ON 
#define POWERSURPLUS -500 // --> Notice the negative value -> surplus power!
#define BANDWIDTH     400 // Only when 400 below POWERSURPLUS relay/socket is switched OFF

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
// AP
#ifdef DEBUG
void printWiFiMode(int mode);
#endif
bool getWiFiPRSdata(void);
void setWiFiPRSdata(void);
void setupWiFiAccessPointConnect(void);
bool testWiFiStationConnect(void);
bool handleAP_Request(WiFiClient client);
bool getWiFiPropertiesFromUser(void);
// AP
bool getSwitchTimePRSdata(void);
void setSwitchTimePRSdata(void);
void getHWE_P1_Data(void);
bool setSocketStatus(bool switchOn);
bool checkPowerLevel(void);
void sendHTTP200OK(WiFiClient newClient);
void sendDefaultResponse(WiFiClient newClient);
int getValueName(const std::string& name, std::size_t& pos);
std::string getStrValueName(const std::string& name, std::size_t& pos, const char& separator);
void sendSchemeResponse(WiFiClient newClient);
void handleSTA_Request(WiFiClient newClient);
void toggleRelay(void);
void buttonHandler(void);
void toggleLed(unsigned long delayT);

void setup() {
    //Setup the relay pin default HIGH is ACTIVE
    pinMode(PIN_RELAY, OUTPUT);    // Set for output
    digitalWrite(PIN_RELAY, LOW);  // default start: Relay is NOT active
    delay(250);
    // Setup Builtin Led
    pinMode(PIN_LED, OUTPUT);   // Set for output
    digitalWrite(PIN_LED, HIGH); // default start: PIN_LED is Active BLUE
    // Setup Button
    pinMode(PIN_BUTTON, INPUT);
#ifdef DEBUG
    Serial.begin(115200);
    while (!Serial) 
      delay(10); 
    Serial.flush();
    delay(1000);
    DEBUG_PRINTLN("Start S3-Switch with SONOFF ESP8266 and AP-support!");
    DEBUG_PRINTLN("------------------ Version 02.0 -------------------");
    delay(1000);
#endif
    // LittleFS start the Littlefilesystem lib and see if we have persistent data ----
    // This opens LittleFS with a root subdirectory /littlefs/
    int tryCnt = 0;
    DEBUG_PRINTLN("LittleFS begin...");
    while(!LittleFS.begin()){
      DEBUG_PRINTLN("LittleFS Mount Failed!");
      if(tryCnt++ == 3){ 
        DEBUG_PRINTLN("3 Tries -> LittleFS Mount Failed: FATAL!");
        break; 
      }
      if(FORMAT_LITTLEFS_IF_FAILED) {
        DEBUG_PRINTLN("Formatting LittleFS filesystem");
        LittleFS.format();
      }
    }
    while(tryCnt == 3) { // Hang while blinking
      toggleLed(200);
    }
    // Get or set the values of relevant and crucial variables
    // to persistence, with a Browser the user can set these remotely!
    // Get or set the values of clockSwitchTime[4][3] in PRSdata.
    if (!getSwitchTimePRSdata()) {
      setSwitchTimePRSdata();
    }
    // Get or set the values of STA network properties in PeRSistent file.
    if (!getWiFiPRSdata()) {
      setWiFiPRSdata();
    }
    // LittleFS------------------------------------------------------------------------

    configTime(3600, 3600, NTP_SERVER1);

    // Start with a setup in Dual Mode --> AP and STA
    WiFi.mode(WIFI_AP_STA);
    delay(250); // <-------Included

    while(!testWiFiStationConnect()) {
        // Start a Temporary Access Point (AP)
        setupWiFiAccessPointConnect();
        if(getWiFiPropertiesFromUser()) {
          DEBUG_PRINTLN("Test if WiFi Station can connect now!");
          setWiFiPRSdata(); // Persistently store new network properties 
        }
        DEBUG_PRINTLN("Close Temporary AP Web Server!");
        webServer.close();
#ifdef DEBUG
        printWiFiMode(WiFi.getMode());
#endif
    }
    // Close the temporary Access Point if it still exists..
    if(WiFi.enableAP(true)) {
#ifdef DEBUG
      printWiFiMode(WiFi.getMode());
#endif 
      WiFi.softAPdisconnect(true);
      DEBUG_PRINTLN("Disable WiFi AP!");
    }
    DEBUG_PRINTLN("Connected successfully to local WIFI!");
#ifdef DEBUG
    printWiFiMode(WiFi.getMode());
#endif
    // Set LED again to: HIGH = BLUE
    digitalWrite(PIN_LED, HIGH);

    DEBUG_PRINT("Device is connected to ");
    DEBUG_PRINT(ssid.c_str());
    DEBUG_PRINT(" IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
    // Use local IP Address in URL to avoid any problems with Android phones
    serverURL = "http://" + ip2String(WiFi.localIP()) + "/";

    DEBUG_PRINTLN("Setting Local Time!");
    setLocalTime();

    sort2DArray(clockSwitchTime);
#ifdef DEBUG
      printClockSwithTime();
#endif
    fillClockSwitchBitArray();

    DEBUG_PRINTLN("Start ESP8266 mDNS!");
    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp8266.local"
    // - second argument is the IP address to advertise 
    //   we send our IP address on the WiFi network
    if (!MDNS.begin(serverName, WiFi.localIP())) {
        DEBUG_PRINTLN("Fatal Error setting up mDNS responder!");
        while(1) {
          toggleLed(200);
        }
    }
    DEBUG_PRINTLN("ESP8266 mDNS started!");
    DEBUG_PRINTLN("Get P1 Meter Service!");
    if( !queryService("p1meter", "hwenergy", "tcp") ){
      DEBUG_PRINTLN("Fatal Error: NO P1-Meter device found!");
      while(1) {
        toggleLed(200);
      }
    }
    p1MeterURL = "http://" + ip2String(p1MeterIP) + "/api/v1/data";

    // Start a simple WebServer for handling page requests
    // Set WiFi STAtion webserver with port number 80
    webServer = WiFiServer(WiFi.localIP(), 80);
    webServer.begin();
    DEBUG_PRINTLN("HTTP Web Server started!");

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
}


std::string getStrValueName(const std::string& name, std::size_t& pos, const char& separator) {
  std::size_t found = header.find(name, pos);
  if (found != INVALIDPOS) {
    pos = found + name.length();
    std::size_t len = header.find(separator, pos);
    std::string tmp = header.substr(pos, (len>pos ? len-pos : pos) ); // Extract 
    return tmp;
  } else return "";
}

// LittleFS --------------------------------------------------
bool getWiFiPRSdata(void) { // Get network properties from persistent file
  if (LittleFS.exists(WIFIDATAFILENAME)) {
    File file = LittleFS.open(WIFIDATAFILENAME, FILE_READ);
    if (file) {  
      String buffer = file.readStringUntil('\n'); // Get first field
      ssid = buffer.c_str();
      buffer = file.readStringUntil('\n'); // Get second field
      password = buffer.c_str();
      file.close();
      DEBUG_PRINTLN(F("Got WiFi properties from persistent file storage"));
      DEBUG_PRINT("Read SSID: "); 
      DEBUG_PRINT(ssid.c_str());
      DEBUG_PRINT(" Read Password: "); 
      DEBUG_PRINTLN(password.c_str());
      return true;
    }    
  }
  return false;
}

void setWiFiPRSdata(void) { // Set network properties to persistent file
  File file = LittleFS.open(WIFIDATAFILENAME, FILE_WRITE);
  if (file) {
      file.print(ssid.c_str());
      file.print('\n'); // write field separator
      file.print(password.c_str());
      file.print('\n'); // write field separator
      file.close();
      DEBUG_PRINTLN(F("Set new WiFi properties in persistent file storage"));
      DEBUG_PRINT("Store SSID: "); 
      DEBUG_PRINT(ssid.c_str());
      DEBUG_PRINT(" Store Password: "); 
      DEBUG_PRINTLN(password.c_str());
  }
}
// LittleFS --------------------------------------------------
#ifdef DEBUG
void printWiFiMode(int mode) {
 const char* modes[] = { "NULL", "STA", "AP", "STA+AP" };
    DEBUG_PRINT("Present WiFi Mode: ");
    DEBUG_PRINTLN(modes[mode]);
}
#endif

void setupWiFiAccessPointConnect(void) {
  // Setup AP Wi-Fi network with SSID and password
  DEBUG_PRINT("Setup Access Point (AP) ->");
  WiFi.mode(WIFI_AP);
  delay(250); // <-------Included
  WiFi.softAP(ssid_AP, password_AP);
  delay(250); // <-------Included
  DEBUG_PRINT(" IP address: ");
  DEBUG_PRINT(WiFi.softAPIP());
  DEBUG_PRINT(" SSID name: ");
  DEBUG_PRINT(WiFi.softAPSSID());
  DEBUG_PRINT(" Mac Address: ");
  DEBUG_PRINTLN(WiFi.softAPmacAddress());
  webServer = WiFiServer(WiFi.softAPIP(), 80); // setup AP web server
  webServer.begin();
  DEBUG_PRINTLN("Select this WiFi Access Point and point browser to its IP address...");
}

bool testWiFiStationConnect(void) {
    unsigned long timeout = millis() + 20*1000; // 10 seconds
    bool success = true;
    DEBUG_PRINTLN("Setting STA (Station)");
    WiFi.mode(WIFI_STA);
    delay(250); // <-------Included
    // Connect to Wi-Fi network like a STAtion with SSID and password  
    // Configure static/fixed Wifi Station IP address
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      DEBUG_PRINTLN("Config Static IP Address Failed to configure!");
    }
    delay(250); // <-------Included
    WiFi.begin(ssid.c_str(), password.c_str());
    delay(250); // <-------Included
    DEBUG_PRINT("Connect to local WIFI ");   
    while (WiFi.status() != WL_CONNECTED) {
      toggleLed(500);
      DEBUG_PRINT(".");
      if(millis() >= timeout) {
        success = false;
        DEBUG_PRINTLN("\nWiFi Station connect timeout!"); 
        break;
      }
    }
    DEBUG_PRINTLN();  
    return success;
}

bool handleAP_Request(WiFiClient client) {
    uint8_t cntNL = 0;                      // Reset count New Line characters
    bool validResponse = false;
    DEBUG_PRINTLN("New Client!");          // print a message out in the serial port
    requestTimeout = millis() + TIMEOUT;
    // loop while the client's connected until done or after a timeout
    while (client.connected() && (millis() <= requestTimeout) ) { 
      if (client.available()) {          // if there's bytes to read from the client,
        char c = client.read();          // read a byte, then
#ifdef DEBUG
        Serial.write(c);                    // print header characters to serial monitor
#endif
        header += c;                        // fill the header string
        if (c == '\n' || c == '\r') {       // if the byte is a newline (NL or CR) character
          cntNL++;
          // if you got two newline characters --> end of the client HTTP request
          if(cntNL == 2) {
            sendHTTP200OK(client);
            if (header.find("GET / ") != INVALIDPOS) {
              DEBUG_PRINTLN("We have made contact...");
              client.println(form_html.c_str());
            } else if (header.find("GET /?id=data") != INVALIDPOS) {
                DEBUG_PRINTLN("We have network properties received, confirm and parse...");
                client.println(confirm_html.c_str());
                std::size_t pos = 0; // Keep track of position in header string while parsing continues!
                ssid = getStrValueName("&ssid=", pos, '&');
                DEBUG_PRINT("SSID: "); 
                DEBUG_PRINTLN(ssid.c_str());
                password = getStrValueName("&password=", pos, ' ');
                DEBUG_PRINT("Password: "); 
                DEBUG_PRINTLN(password.c_str());
                validResponse = true;
                // Break out of the while connected loop
                break;
                }
          }
        } 
      }
    }
    // Clear the header variable
    header = "";
    // Close the client connection
    client.flush();
    client.stop();
    DEBUG_PRINTLN("Client disconnected!\n");
    return validResponse;
}

bool getWiFiPropertiesFromUser(void) {
  bool validResponse = false;
  do { // loop() replacement
    WiFiClient client = webServer.available();   // Listen for incoming clients
    if (client) { // client
      validResponse = handleAP_Request(client);
    }
  } while(!validResponse);
  return validResponse;
}

void toggleRelay(void) {
  bool on = digitalRead(PIN_RELAY) == HIGH;
  digitalWrite(PIN_RELAY, (on ? LOW : HIGH) );
  socketIsSwitchedON = (on ? false : true);
}

void buttonHandler(void) {
  byte buttonState = digitalRead(PIN_BUTTON);
  if ( buttonState != lastButtonState ) {
    if (buttonState == LOW) {
      buttonDownTime     = millis();
      buttonPressHandled = 0;
    }
    else {
      unsigned long dt = millis() - buttonDownTime;
      if ( dt >= 90 && dt <= 900 && buttonPressHandled == 0 ) {
        toggleRelay();
        buttonPressHandled = 1;
      }
    }
    lastButtonState = buttonState;
  }
}

void toggleLed(unsigned long delayT) {
  bool on = digitalRead(PIN_LED) == HIGH;
  digitalWrite(PIN_LED, (on ? LOW : HIGH) );
  delay(delayT);
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
  uint8_t temp[COLS]= {0, 0, 0};
  int i, j;
  for(i = 0; i < ROWS; i++) {
    for(j = 0; j < (ROWS-1); j++) {
      if(arr[j][0] > arr[i][0]) {
          memcpy(&temp, arr[i], sizeof(arr[i]));
          memcpy(arr[i], arr[j], sizeof(arr[j]));
          memcpy(arr[j], &temp, sizeof(temp));	
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
// LittleFS --------------------------------------------------

void fillClockSwitchBitArray(void) {
  int TimeInMin = 0;
  clockSwitchBitArray.reset(); // Set all to zero == false
  DEBUG_PRINTLN("Fill Switch Array!");
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
}

void getHWE_P1_Data(void) {
  // WARNING:   -----------------------------------------------------------------------------
  // HWE-P1 will refuse future http connections when a http session is not closed correctly !!
  // Remedy: Unmount physically device from P1 port and wait for 10 seconds -> remount
  // Device will reconnect to the active Homewizard Energy-App without human interference
  //-----------------------------------------------------------------------------------------
    //DEBUG_PRINTLN("[HTTP] begin...");
    // configure targeted server and p1MeterURL
    WiFiClient client;
    http.begin(client, p1MeterURL);
    // Allow for fetching streams
    http.useHTTP10(true);
  // ------------------------------------------------------------------------------------------
    // Ckeck WiFi connection
    if((WiFi.status() == WL_CONNECTED)) {
        //DEBUG_PRINT("[HTTP] GET...\n");
        // Send GET HTTP header
        int httpCode = http.GET();
        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            //DEBUG_PRINTF("[HTTP] GET... code: %d\n", httpCode);
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
                NetPower = doc["active_power_w"].as<long>();
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
    (socketIsSwitchedON = switchOn) ? digitalWrite(PIN_RELAY, HIGH) : digitalWrite(PIN_RELAY, LOW);
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
    if(clockSwitchBitArray.test(indexVal)) {
      return !setSocketStatus(true);
    }
    // all other cases switch is OFF
    return setSocketStatus(false);
}

void sendHTTP200OK(WiFiClient newClient) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            newClient.println(http_200_OK.c_str());
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
            newClient.println("<h1>SONOFF ESP8266</h1><p>Manually set switch ON or OFF!</p>");
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
            newClient.println("<h1>SONOFF ESP8266</h1><p>Select one of the following options!</p>");
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
            newClient.println("<h1>SONOFF ESP8266</h1>");
            newClient.println("<p class=\"left\">Time: " + String(timeHour) + ":" + String(timeMin) + ":" + String(timeSec) + " &nbsp;&nbsp;&nbsp;Date: " + \
                              String(d) + " " + String(m) + " " + String(y) + "</p>");
            newClient.println("<p class=\"left\">Wifi network: " + String(ssid.c_str()) + "</p>");
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
              DEBUG_PRINTLN("Parsing new time and duration values!");
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
            newClient.println("<h1>SONOFF ESP8266</h1><p>Select <b>Switch ON</b> moments and duration:</p>");
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

void handleSTA_Request(WiFiClient newClient){
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
            // so send 200 OK-response:
            sendHTTP200OK(newClient);
            if (header.find("GET /?Id=scheme") != INVALIDPOS || header.find("GET /scheme") != INVALIDPOS ) {
              sendSchemeResponse(newClient);
            } else if( header.find("GET /switch") != INVALIDPOS ) {
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
    MDNS.update();
    setLocalTime();
    buttonHandler();

    WiFiClient client = webServer.available();   // Listen for incoming clients
    if (client) {
        handleSTA_Request(client);
    }

    if (millis() > HWEtimeInterval) { // Get new HWE P1 data
        getHWE_P1_Data();
        if( checkPowerLevel() ) { 
          // register in socketIsSwitchedBitArray index when switched ON -> Green
          int indexVal = ((Hour-5)*60 + Min)/6; // Start record only at AM 05:00 hr
          //DEBUG_PRINTF("Hour: [%d] Min: [%d] Calc index: [%d]\n", Hour, Min, indexVal);
          if((indexVal>=0) && (indexVal<MAXNUMOFRECORDS)) { 
            socketIsSwitchedBitArray.set(indexVal, true);
          }
        }
        HWEtimeInterval = millis() + HWEPOLLINGTIMESPAN;
    }
}
