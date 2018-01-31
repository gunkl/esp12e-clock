#include <FS.h> // spiffs filesystem
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
// #include <EasyNTPClient.h> // https://github.com/aharshac/EasyNTPClient
// #include <WiFiUdp.h>
#include <TimeLib.h>
#include "TimeClient.h"
#include <MenuSystem.h>  // https://github.com/jonblack/arduino-menusystem
#include "CustomNumericMenuItem.h"  // part of menusystem
#include "MyRenderer.h"  // part of menusystem

#define PIN 14
long lastUpdate = millis();
long lastSecond = millis();

String hours, minutes, seconds;
int currentSecond, currentMinute, currentHour, secondMinute;
int thirdMinute[5];
bool greaterthan = false;
bool secondMinute_modifier = false;

char ssid[64];  //  your network SSID (name)
char password[64];       // your network password

// menu config
// forward declarations
const String format_float(const float value);
const String format_int(const float value);
const String format_color(const float value);
void on_component_selected(MenuComponent* p_menu_component);
void on_mainconfig(MenuComponent* p_menu_component);
void on_eraseconfig(MenuComponent* p_menu_component);
void on_exit(MenuComponent* p_menu_component);
void on_advancedconfig(MenuComponent* p_menu_component);
// Menu variables

MyRenderer my_renderer;
MenuSystem ms(my_renderer);

MenuItem mm_mi1("Restart (no changes)", &on_exit);
MenuItem mm_mi2("Basic Settings", &on_mainconfig);
MenuItem mm_mi4("RESET CONFIG", &on_eraseconfig);
Menu mu1("(Menu)");
// end menu config
//

float UTC_OFFSET = -8;
TimeClient timeClient(UTC_OFFSET);

// WiFiUDP Udp;
// EasyNTPClient ntpClient(Udp, "pool.ntp.org", (-8)); // 0 = GMT, use the timezone library to set the time zone.
Adafruit_NeoPixel strip = Adafruit_NeoPixel(12, PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  int reset_reason = checkResetReason();
  bool ismenumode = false;
  //
  startfs(); // start up SPIFFS.
  if (reset_reason == 0 || reset_reason == 6 ) {
    ismenumode = getmenumode();
  }
  if (ismenumode) {
    displaymenu();
    serialFlush();
    while (true) {
      serial_handler();
    }
  }
  //

  strip.begin();
  strip.setBrightness(128);
  strip.show();

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(0);
  WiFi.hostname("clock-01");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.updateTime(UTC_OFFSET);
  updateTime() ;
  // setSyncProvider(syncTime);
  // setSyncInterval(36000);
  lastUpdate = millis();
  lastSecond = millis();
}

void loop()
{
  int testHour = 0;
  if ((millis() - lastUpdate) > 36000000) updateTime(); // 36000000 is every 10 hours

  if ((millis() - lastSecond) > 1000)
  {
    // 12-led ring

    for (int i=0; i<13; i=i+1){
      strip.setPixelColor(i, 0, 0, 0);
    }
    
    /* 24-led ring
    strip.setPixelColor(currentSecond / 2.5, 0, 0, 0);
    strip.setPixelColor(currentMinute / 2.5, 0, 0, 0);
    strip.setPixelColor(currentHour * 2, 0, 0, 0);
    */

    strip.show();
    lastSecond = millis();
    currentSecond++;
    if (currentSecond > 59)
    { currentSecond = 0;
      currentMinute++;
      if (currentMinute > 59) {
        currentMinute = 0;
        currentHour++;
        testHour = currentHour; // we need to know the hour below here but with 0 being 12 not 0.
        if (currentHour > 12){
          currentHour = 0;
        }
      }
    }
    String currentTime = String(currentHour) + ':' + String(currentMinute) + ':' + String(currentSecond);
    Serial.println(currentTime);

    strip.setPixelColor(currentSecond / 5, 0, 0, 255);
    strip.setPixelColor(currentMinute / 5, 0, 255, 0);
    int roughMinute = (currentMinute / 5);
    int remainMinute = (currentMinute % 5);
    if (remainMinute == 0) {
            secondMinute_modifier = false; // stop adding to the second minute hand
    }
    if (roughMinute <= 6 && (roughMinute + (currentMinute % 5)) != roughMinute){
      secondMinute = roughMinute + (currentMinute % 5);
      greaterthan = false;
    }
    if (roughMinute > 6 && (roughMinute - (currentMinute % 5)) != roughMinute){
      secondMinute = roughMinute - (currentMinute % 5);
      greaterthan = true;
    }
    if (secondMinute == currentHour) {
      secondMinute_modifier = true; // add to the second minute hand because the hour hand is in the way
    }
    if (secondMinute_modifier) {
      if (!greaterthan){
        secondMinute++;
      }
      else {
        secondMinute--;
      }
    }
    strip.setPixelColor(secondMinute, 50, 0, 200);
    // Handle scenarios where there are x6,x7,x8,x9 minutes (since clock has 12 LEDs, how do we display
    // finer than 5 minutes? we create purple dots.
    // do something different with the leds between the minute marker and the second minute marker.
    // second minute marker is the indicator by distance, ie if its minute 7, you'll get the 5 pixel lit
    // plus a dim one close, and a bright one on the second (remainder) led away
    int hour_intercept = 0; // if the minute modifier hits the hour led, then add another
    //
    // if <= 34 minutes, then put the purple dots to the left of the 6 hand
    if (!greaterthan && remainMinute != 0){ 
      for ( int i=(roughMinute + 1); i < (roughMinute + remainMinute); i = i + 1){
        if (i == testHour){
          hour_intercept++;
        }
        strip.setPixelColor((i + hour_intercept), 10, 0, 35);
        // Serial.println("LT: " + String(i) + " - HRI: " + String(hour_intercept));
      }
    }
    // if > 34 minutes, then put the purple dots to the right of the 6 hand
    else if (remainMinute != 0) {
      for ( int i=(roughMinute - remainMinute + 1); i < (roughMinute); i = i + 1){
        if (i <= testHour){
          hour_intercept++;
        }
        else {
          hour_intercept = 0;
        }
        strip.setPixelColor((i - hour_intercept), 10, 0, 35);
        // Serial.println("GT: " + String(i) + " - HRI: " + String(hour_intercept));

      }
    }
    strip.setPixelColor(currentHour, 255, 0, 0);
    /* 24-led ring
   strip.setPixelColor(currentSecond / 2.5, 0, 0, 255);
    strip.setPixelColor(currentMinute / 2.5, 0, 255, 0);
    strip.setPixelColor(currentHour * 2, 255, 0, 0);
    */
    strip.show();
  }
}

time_t syncTime(){
    timeClient.updateTime(UTC_OFFSET);
}

void updateTime() 
{
  timeClient.updateTime(UTC_OFFSET);
  hours = timeClient.getHours();
  minutes = timeClient.getMinutes();
  seconds = timeClient.getSeconds();
  currentHour = hours.toInt();
  if (currentHour > 12) currentHour = currentHour - 12;
  currentMinute = minutes.toInt();
  currentSecond = seconds.toInt();
  lastUpdate = millis();
  //
  // tests
  // currentHour = 11;
  // currentMinute = 58;
  // currentHour = 1;
  // currentMinute = 58;
  // currentHour = 5;
  // currentMinute = 32;
  // currentHour = 5;
  // currentMinute = 37;
  
}

boolean fileWrite(String name, String filemode, String content) {
  char stringBuffer[3];
  //open the file for writing.
  //Modes:
  //"r"  Opens a file for reading. The file must exist.
  //"w" Creates an empty file for writing. If a file with the same name already exists, its content is erased and the file is considered as a new empty file.
  //"a" Appends to a file. Writing operations, append data at the end of the file. The file is created if it does not exist.
  //"r+"  Opens a file to update both reading and writing. The file must exist.
  //"w+"  Creates an empty file for both reading and writing.
  //"a+"  Opens a file for reading and appending.:

  //choosing w because we'll both write to the file and then read from it at the end of this function.
  sprintf(stringBuffer, "%s", (filemode).c_str());
  File file = SPIFFS.open(name.c_str(), stringBuffer);

  //verify the file opened:
  if (!file) {
    //if the file can't open, we'll display an error message;
    String errorMessage = "Can't open '" + name + "' !\r\n";
    Serial.println(errorMessage);
    return false;
  } else {
    file.write((uint8_t *)content.c_str(), content.length());
    file.close();
    return true;
  }
}


void fileRead(String name) {
  char new_int[5];
  //read file from SPIFFS and store it as a String variable
  String contents;
  File file = SPIFFS.open(name.c_str(), "r");
  if (!file) {
    String errorMessage = "Can't open '" + name + "' !\r\n";
    Serial.println(errorMessage);
  }
  else {
    if (name.equals("/datalogger.conf")) {
      Serial.println("Reading config...");
      while (file.available()) {
        //Lets read line by line from the file
        file.readStringUntil('\n').toCharArray(ssid, 64);
        Serial.println("SSID: " + String(ssid));
        file.readStringUntil('\n').toCharArray(password, 64);
        // Serial.println("password: " + String(password));
        file.readStringUntil('\n').toCharArray(new_int, 5);
        sscanf(new_int, "%f", &UTC_OFFSET);
        Serial.println("UTC offset: " + String(UTC_OFFSET));
        //
      }
    }
  }
}


boolean fileRemove(String name) {
  //read file from SPIFFS and store it as a String variable
  SPIFFS.remove(name.c_str());
  return true;
}

void displaymenu() {
  ms.get_root_menu().add_item(&mm_mi1);
  ms.get_root_menu().add_item(&mm_mi2);
  // ms.get_root_menu().add_item(&mm_mi3);
  ms.get_root_menu().add_item(&mm_mi4);
  ms.get_root_menu().add_menu(&mu1);
  // mu1.add_item(&mu1_mi0);
  // mu1.add_item(&mu1_mi1);
  // mu1.add_item(&mu1_mi2);
  // mu1.add_item(&mu1_mi3);
  // ms.get_root_menu().add_item(&mm_mi4);
  // ms.get_root_menu().add_item(&mm_mi5);

  display_help();
  ms.display();
}

void serialFlush() {
  while (Serial.available() > 0) {
    char t = Serial.read();
  }
}

bool getmenumode() {
  char minput[5];
  Serial.setTimeout(5000);
  serialFlush();
  Serial.println("");
  Serial.println("Type mm + <enter> within 5 seconds for menu...");
  Serial.readBytesUntil('\r', minput, 5);
  if (String(minput).endsWith("mm")) {
    Serial.println("Loading menu.");
    return true;
  }
  else {
    Serial.println("Booting...");
    return false;
  }
}

void startfs() {
  SPIFFS.begin();
  // FSInfo fs_info;
  if (SPIFFS.exists("/datalogger.conf")) {
    // Serial.println("SPIFFS: " + String(SPIFFS.info(fs_info)));
    fileRead("/datalogger.conf");
  }
  else {
    Serial.println("No SPIFFS found, or no config found, formatting...");
    SPIFFS.format();
    displaymenu();
    serialFlush();
    while (true) {
      serial_handler();
    }
  }
}

void on_eraseconfig(MenuComponent* p_menu_component) {
  Serial.println("Formatting SPIFFS.");
  SPIFFS.format();
  config_reset();
}

void on_exit(MenuComponent* p_menu_component) {
  config_reset();
}

String menuinput(char *minput, String defaultval, int fieldsize) {
  // fieldsize should always be +1 the allowed size of the destination field
  int numchar = Serial.readBytesUntil('\r', minput, fieldsize);
  if (numchar > 0) {
    minput[numchar] = '\0'; // terminate string with NULL
    return minput;
  }
  else {
    Serial.println("No input provided, using prior value.");
    return defaultval;
  }
}

void on_mainconfig(MenuComponent* p_menu_component) {
  char minput[64];
  Serial.println(p_menu_component->get_name());
  Serial.setTimeout(120000);  // 120 second input timeout
  serialFlush();
  Serial.println("");
  Serial.println("Enter SSID: [" + String(ssid) + "] ");
  String newssid1 = menuinput(minput, String(ssid), 64);
  Serial.println("Enter password: [" + String(password) + "] ");
  String newpassword1 = menuinput(minput, String(password), 64);
  //
  Serial.println("UTC Offset: [" + String(UTC_OFFSET) + "] ");
  String new_UTC_OFFSET = menuinput(minput, String(UTC_OFFSET), 5);
  //
  Serial.println("Writing config...");
  fileWrite("/datalogger.conf", "w", newssid1 + "\n" + newpassword1 + "\n");
  fileWrite("/datalogger.conf", "a", new_UTC_OFFSET + "\n");
  config_reset();
}


void config_reset() {
  Serial.println("Restarting to complete changes..");
  ESP.restart();
}

// make the reset info reason code work
extern "C" {
#include <user_interface.h> // https://github.com/esp8266/Arduino actually tools/sdk/include
}
//
/*
  enum rst_reason {
  REASON_DEFAULT_RST = 0,  normal startup by power on
  REASON_WDT_RST = 1,  hardware watch dog reset
  REASON_EXCEPTION_RST = 2,  exception reset, GPIO status won't change
  REASON_SOFT_WDT_RST   = 3,  software watch dog reset, GPIO status won't change
  REASON_SOFT_RESTART = 4,  software restart ,system_restart , GPIO status won't change
  REASON_DEEP_SLEEP_AWAKE = 5,  wake up from deep-sleep
  REASON_EXT_SYS_RST      = 6  external system reset
  };
*/
int checkResetReason() {
  rst_info *myResetInfo;
  delay(5000); // slow down so we really can see the reason!!
  myResetInfo = ESP.getResetInfoPtr();
  Serial.printf("myResetInfo->reason %x \n", myResetInfo->reason); // reason is uint32
  Serial.flush();
  return int(myResetInfo->reason);
}
void serial_handler() {
  char inChar[2];
  Serial.setTimeout(120000);
  Serial.readBytesUntil('\r', inChar, 2);
  // Serial.println("\033c");
  switch (inChar[0]) {
    case 'w': // Previous item
      ms.prev();
      ms.display();
      Serial.println("");
      break;
    case 's': // Next item
      ms.next();
      ms.display();
      Serial.println("");
      break;
    case 'a': // Back presed
      ms.back();
      ms.display();
      Serial.println("");
      break;
    case 'd': // Select presed
      ms.select();
      ms.display();
      Serial.println("");
      break;
    case '?':
    case 'h': // Display help
      ms.display();
      Serial.println("");
      break;
    default:
      break;
  }
}

// Menu callback function

// writes the (int) value of a float into a char buffer.
const String format_int(const float value) {
  return String((int) value);
}

// writes the value of a float into a char buffer.
const String format_float(const float value) {
  return String(value);
}

// writes the value of a float into a char buffer as predefined colors.
const String format_color(const float value) {
  String buffer;

  switch ((int) value)
  {
    case 0:
      buffer += "Red";
      break;
    case 1:
      buffer += "Green";
      break;
    case 2:
      buffer += "Blue";
      break;
    default:
      buffer += "undef";
  }

  return buffer;
}

// In this example all menu items use the same callback.

void on_component_selected(MenuComponent* p_menu_component) {
  Serial.println(p_menu_component->get_name());
}


void display_help() {
  Serial.println("");
  Serial.println("***************");
  Serial.println("w: go to previus item (up)");
  Serial.println("s: go to next item (down)");
  Serial.println("a: go back (right)");
  Serial.println("d: select \"selected\" item");
  Serial.println("?: print this help");
  Serial.println("h: print this help");
  Serial.println("***************");
}


