#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#include "wifi.h"

void Wifi::setup() {
  WiFi.hostname("WlanClock");
  WiFi.mode(WIFI_STA);

  WiFiManager wifiManager;
  wifiManager.autoConnect("WlanClock");
}

void Wifi::reset() {
  WiFi.disconnect(true);
}
