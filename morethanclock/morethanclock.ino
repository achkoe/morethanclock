#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "DCF77.h"
#include "Time.h"

#define DCF_PIN 2                // Connection pin to DCF 77 device
#define DCF_INTERRUPT 0          // Interrupt number associated with pin

time_t time;
char tstring[16];
char dstring[16];
int8_t last_second = -1;
int8_t current_second;
int8_t xpos = 0;
int8_t dir = +1;


// Non-inverted input on pin DCF_PIN
DCF77 DCF = DCF77(DCF_PIN,DCF_INTERRUPT, true);

// SH1106 ADC5/PC5/SCL, PC4/ADC4/SDA
U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, A5, A4);

void setup() {
    u8g2.begin();
    Serial.begin(115200);
    DCF.Start();
    Serial.println("Wait DCF77 ... ");
    // Serial.println("It will take at least 2 minutes before a first time update.");
}

void loop() {
    delay(1000);
    time_t DCFtime = DCF.getTime(); // Check if new DCF77 time is available
    if (DCFtime != 0) {
        Serial.println("Time updated");
        setTime(DCFtime);
    }
    current_second = second();
    if (current_second != last_second) {
        last_second = current_second;
        sprintf(tstring, "%d:%02d:%02d", hour(), minute(), current_second);
        sprintf(dstring, "%d.%d.%d", day(), month(), year());

        //digitalClockDisplay();
        //Serial.println(tstring);

        if (xpos + u8g2.getStrWidth(tstring) > 126) {
            dir = -1;
        } else if (xpos < 0) {
            dir = +1;
        }
        xpos += dir;

        u8g2.firstPage();
        do {
            //u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.setFont(u8g2_font_crox4hb_tn);
            u8g2.drawStr(xpos, 24, tstring);
            u8g2.setFont(u8g2_font_crox1cb_tn);
            u8g2.drawStr(0, 24 + 16, dstring);
        } while ( u8g2.nextPage() );
    }
}

/*
void digitalClockDisplay(){
    // digital clock display of the time
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.print(" ");
    Serial.print(day());
    Serial.print(" ");
    Serial.print(month());
    Serial.print(" ");
    Serial.print(year());
    Serial.println();
}

void printDigits(int digits){
    // utility function for digital clock display: prints preceding colon and leading 0
    Serial.print(":");
    if(digits < 10)
       Serial.print('0');
    Serial.print(digits);
}
*/


