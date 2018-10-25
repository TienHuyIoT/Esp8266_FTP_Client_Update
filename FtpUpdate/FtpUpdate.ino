#include <ESP8266WiFi.h>
#include <FS.h>
#include "FtpClientUpdate.h"

/* Send 'f' from serial to download*/

// Set these to your desired softAP credentials. They are not configurable at runtime.
const char *ssid = "Wifi SSID";
const char *password = "Wifi pass";


const char *HwVersion = "0.1";
const char *FwVersion = "0.1.1";

String host = "192.168.31.199";
String user = "ftp user";
String pass = "ftp pass";
String path = "ftpupdateV1.bin";
String md5 = "84950FB41AC0515F0EADA7517A148D3A";

#define DEBUG(fmt, ...) Serial.printf_P(PSTR("\r\nP" fmt) ,##__VA_ARGS__)

boolean debug = false;  // true = more messages

// provide text for the WiFi status
const char *str_status[] = {
  "WL_IDLE_STATUS",
  "WL_NO_SSID_AVAIL",
  "WL_SCAN_COMPLETED",
  "WL_CONNECTED",
  "WL_CONNECT_FAILED",
  "WL_CONNECTION_LOST",
  "WL_DISCONNECTED"
};

// provide text for the WiFi mode
const char *str_mode[] = { "WIFI_OFF", "WIFI_STA", "WIFI_AP", "WIFI_AP_STA" };

// change to your server
IPAddress server( 192, 168, 31, 199 );

// change fileName to your file (8.3 format!)
String fileName = "BaseStation/ftpupdateV1.bin";

//----------------------- WiFi handling
void connectWifi() {
  Serial.print("Connecting as wifi client to SSID: ");
  Serial.println(ssid);

  // use in case of mode problem
  WiFi.disconnect();
  // switch to Station mode
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
  }

  WiFi.begin ( ssid, password );

  if (debug ) WiFi.printDiag(Serial);

  // ... Give ESP 10 seconds to connect to station.
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  // Check connection
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected; IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("WiFi connect failed to ssid: ");
    Serial.println(ssid);
    Serial.print("WiFi password <");
    Serial.print(password);
    Serial.println(">");
    Serial.println("Check for wrong typing!");
  }
}  // connectWiFi()

void setup() {
  delay(1000);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Sync,Sync,Sync,Sync,Sync");
  delay(500);
  Serial.println();

  Serial.printf_P(PSTR("\r\nHwVersion: %s"),HwVersion);
  DEBUG("FwVersion: %s",FwVersion);

  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);

  Serial.println ( "Connect to Router requested" );
  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi mode: ");
    Serial.println(str_mode[WiFi.getMode()]);
    Serial.print ( "Status: " );
    Serial.println (str_status[WiFi.status()]);
  } else {
    Serial.println("");
    Serial.println(F("WiFi connect failed, push RESET button."));
  }

  Serial.println(F("Ready. Press f or r"));
  Serial.println(F("Hello ftpUpdate2"));
}  // setup()


void loop() {
  byte inChar;

  if (Serial.available() > 0) {
    inChar = Serial.read();
  }

  if (inChar == 'f') {    
    if (FtpClientUpdate.update(host,user,pass,path,md5)) {
      Serial.println(F("FTP OK"));
      delay(1000);
      ESP.restart();
    }
    else Serial.println(F("FTP FAIL"));
  }
  delay(10);
}
