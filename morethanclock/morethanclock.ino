#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include "BME280I2C.h"

#include "DCF77.h"
#include "Time.h"

#define DCF_PIN 2                // Connection pin to DCF 77 device
#define DCF_INTERRUPT 0          // Interrupt number associated with pin

time_t time;
char time_str[16];
char date_str[16];
char bme_str[16];
int8_t last_second = -1;
int8_t current_second;
int8_t xpos_1 = 0;
int8_t xpos_2 = -66;
int8_t dir_1 = +1;
int8_t dir_2 = -1;
int8_t cnt = 0;
char* day_of_week[8] = {"", "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};

// Non-inverted input on pin DCF_PIN
DCF77 DCF = DCF77(DCF_PIN,DCF_INTERRUPT, true);

// SH1106 ADC5/PC5/SCL, PC4/ADC4/SDA
// U8G2_SH1106_128X64_NONAME_F_SW_I2C(rotation, clock, data [, reset]) [full framebuffer, size = 1024 bytes]
//U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 6, 7);
//U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, A5, A4, U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R2, A5, A4, U8X8_PIN_NONE);

BME280I2C bme;

  //If you close the jumper it is 0x76 //The I2C address must be set before .begin() otherwise the cal values will fail to load.
  // if(bmeA.beginI2C() == false) Serial.println("Sensor A connect failed"); bmeB.setI2CAddress(0x76); //Connect to a second sensor

void setup() {

    Serial.begin(115200);
    Wire.begin();
    u8g2.begin();

    while (!bme.begin()) {
        Serial.println("!");
        delay(1000);
    }

    DCF.Start();
}

void loop() {
    delay(1000);

    printBME280Data(&Serial);

    time_t DCFtime = DCF.getTime(); // Check if new DCF77 time is available
    if (DCFtime != 0) {
        Serial.println(F("TU"));
        setTime(DCFtime);
    }

    current_second = second();

    if (current_second != last_second) {
        last_second = current_second;
        sprintf(time_str, "%d:%02d:%02d", hour(), minute(), current_second);
        sprintf(date_str, "%s, %d.%d.%d", day_of_week[weekday()], day(), month(), year());

        if (xpos_1 + u8g2.getStrWidth(time_str) > 126) {
            dir_1 = -1;
        } else if (xpos_1 < 1) {
            dir_1 = +1;
        }
        xpos_1 += dir_1;

        if (xpos_2 <= -6) {
            xpos_2 = 126 - u8g2.getStrWidth(date_str);
        }
        if (xpos_2 + u8g2.getStrWidth(date_str) >= 126) {
            dir_2 = -1;
        } else if (xpos_2 < 0) {
            dir_2 = +1;
        }
        xpos_2 += dir_2;


        //u8g2.updateDisplay();
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_crox4hb_tn);
            u8g2.drawStr(xpos_1, 24, time_str);
            u8g2.setFont(u8g2_font_crox1cb_tf);
            u8g2.drawStr(xpos_2, 24 + 16, date_str);
            u8g2.drawStr(0, 24 + 2 * 16, bme_str);
        } while ( u8g2.nextPage() );
    }
}


void printBME280Data(Stream* client) {
   float temp(NAN), hum(NAN), pres(NAN);

   BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
   BME280::PresUnit presUnit(BME280::PresUnit_hPa);

   bme.read(pres, temp, hum, tempUnit, presUnit);

   if (cnt == 0) {
        dtostrf(temp, 4, 1, bme_str);
        strcat(bme_str, "\xb0\x43");
   } else if (cnt == 3) {
        dtostrf(hum, 6, 1, bme_str);
        strcat(bme_str, "% RH");
   } else if (cnt == 6) {
        dtostrf(pres, 8, 1, bme_str);
        strcat(bme_str, "hPa");
   } else if (cnt == 8) {
        cnt = -1;
   }
   cnt += 1;

   client->println(bme_str);
   client->print(F("\t\tHumidity: "));
   client->print(hum);
   client->print(F("% RH"));
   client->print(F("\t\tPressure: "));
   client->print(pres);
   client->println(F("Pa"));
}
