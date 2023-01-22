/*
Clock using NTP client, a BME280 sensor for humidity, air pressure and temperature measurement,
another BS18B20 sensor for temperature measurement and a 128x64 OLED display to display all this.
*/

#include <Wire.h>
#include <U8g2lib.h>
#include <BME280I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include "src/wifi.h"
#include "src/httpServer.h"


// for detecting when a second has elapsed
int last_second = -1;
int current_second;
// for detecting when a minute has elapsed
int last_minute = -1;
int current_minute;

// for converting UTC time to local time
int time_offset = 0;

char time_str[9];             // '10:20:33'        -> 8 characters
char date_str[11];            // '16.12.2022'      -> maximum 10 characters
char bme_temperature_str[8];  // '-13.1°C'         -> maximum 7 characters
char bs_temperature_str[8];   // '-13.1°C'         -> maximum 7 characters
char bme_humidity_str[10];    // '100.0% RH'       -> maximum 9 characters
char bme_pressure_str[10];    // '1234.5hPa'       -> maximum 9 characters
char *sensor_str;

// for selecting what is shown in the display
int8_t cnt = 0;

// variables for determining display positions
int8_t time_str_pos = 0;   // x position of time string
int8_t time_str_dir = +1;  // moving direction of time string
int8_t date_str_pos = 0;   // x position of date string
int8_t sensor_str_pos = 0; // x position of sensor string

// name of weekday
char const *day_of_week[8] = {"", "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};


WiFiUDP udp;
NTPClient ntpClient = NTPClient(udp);

// U8G2_SH1106_128X64_NONAME_F_SW_I2C(rotation, clock, data [, reset]) [full framebuffer, size = 1024 bytes]
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R2);

BME280I2C bme;

// Data wire of BS18B20 is plugged into port 14 (D5)
#define ONE_WIRE_BUS 14

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// prototypes
void updateLocalizedUtcOffset();
time_t getNtpTime();


void setup() {
  // initialize serial communications at 9600 bps
  Serial.begin(9600);
  Wire.begin();
  u8g2.begin();
  sensors.begin();
  while (!bme.begin()) {
      delay(1000);
  }
  Wifi::setup();
  HttpServer::setup();
  // set time_offset
  updateLocalizedUtcOffset();
  // setuo nto client
  ntpClient.begin();
  ntpClient.update();
  // setup time library
  setSyncInterval(60);
  setSyncProvider(getNtpTime);
}


void loop() {
  ntpClient.update();
  HttpServer::loop();

  current_second = second();
  if (current_second != last_second) {
    // execute every second
    last_second = current_second;

    current_minute = minute();
    if (current_minute != last_minute) {
      // execute every minute
      last_minute = current_minute;
      updateLocalizedUtcOffset();
      Serial.println(time_offset);
    }

    // fill bme_humidity_str, bme_temperature_str, bme_pressure_str with measured data
    getBME280Data();
    // fill bs_temperature_str with measured data
    getBS18B20Data();

/*
    Serial.println(bme_temperature_str);
    Serial.println(bs_temperature_str);
    Serial.println(bme_humidity_str);
    Serial.println(bme_pressure_str);
*/
    // render current time and determine x position in display
    sprintf(time_str, "%02d:%02d:%02d", hour(), current_minute, current_second);
    u8g2.setFont(u8g2_font_crox5hb_tn); // font height is 25 pixels
    if (time_str_pos + u8g2.getStrWidth(time_str) > 126) {
        time_str_dir = -1;
    } else if (time_str_pos < 1) {
        time_str_dir = +1;
    }
    time_str_pos += time_str_dir;

    // render name of weekday or current date and determine position to display it centered
    if (current_second % 10 == 0) {
      sprintf(date_str, "%s", day_of_week[weekday()]);
    } else {
      sprintf(date_str, "%d.%d.%d", day(), month(), year());
    }
    u8g2.setFont(u8g2_font_crox2cb_tf); // font height is 16 pixels
    date_str_pos = (128 - u8g2.getStrWidth(date_str)) / 2;

    // render sensor strng and determine position to display it centered
    if (cnt == 0) {
      sensor_str = bme_temperature_str;
    } else if (cnt == 1) {
      sensor_str = bs_temperature_str;
    } else if (cnt == 2) {
      sensor_str = bme_humidity_str;
    } else if (cnt == 3) {
      sensor_str = bme_pressure_str;
    }
    sensor_str_pos = (128 - u8g2.getStrWidth(sensor_str)) / 2;

    // put all to display
    u8g2.firstPage();
    do {
        u8g2.drawRFrame(0, 0, 128, 64, 3);
        u8g2.setFont(u8g2_font_crox5hb_tn); // font height is 25 pixels
        u8g2.drawStr(time_str_pos, 25, time_str);
        u8g2.setFont(u8g2_font_crox2cb_tf); // font height is 16 pixels
        u8g2.drawStr(date_str_pos, 25 + 16 + 1, date_str);
        u8g2.drawStr(sensor_str_pos, 25 + 2 * 16 + 1, sensor_str);
    } while ( u8g2.nextPage() );

    // update cnt
    if (current_second % 3 == 0) {
      cnt = cnt + 1 > 3 ? 0 : cnt + 1;
    }
  }
}


/*!
 * Render BME280 temperature, air pressure and humidity to the corresponding strings
 */
void getBME280Data() {
  float temp(NAN), hum(NAN), pres(NAN);

  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);

  bme.read(pres, temp, hum, tempUnit, presUnit);

  dtostrf(temp, 0, 1, bme_temperature_str);
  strcat(bme_temperature_str, "\xb0\x43");

  dtostrf(hum, 0, 1, bme_humidity_str);
  strcat(bme_humidity_str, "% RH");

  dtostrf(pres, 0, 1, bme_pressure_str);
  strcat(bme_pressure_str, "hPa");
}


/*!
 * Render BS18B20 temperature the corresponding string
 */
void getBS18B20Data() {
  sensors.requestTemperatures(); // Send the command to get temperatures
  dtostrf(sensors.getTempCByIndex(0), 4, 1, bs_temperature_str);
  strcat(bs_temperature_str, "\xb0\x43");
}


/*!
 * Return the time from NTP corrected by time offset.
 */
time_t getNtpTime() {
  return time_offset + ntpClient.getEpochTime();
}


/*!
 * Get time offset, leave current time offset unchanged if not possible. */
void updateLocalizedUtcOffset() {
/* remove the depricated ESP8266 API call:
 *HTTPClient http;
 *http.begin("http://worldtimeapi.org/api/ip");
*/

//  use the new API like below:
  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://worldtimeapi.org/api/ip");
  int responseCode = http.GET();
  int utcOffset = 0;
  int dstOffset = 0;

  if (responseCode == 200) {
    String payload = http.getString();

    StaticJsonDocument<1024> doc;
    deserializeJson(doc, payload);

    utcOffset = doc["raw_offset"].as<int>();
    dstOffset = doc["dst_offset"].as<int>();
    time_offset = utcOffset + dstOffset;
  }
  http.end();
}
