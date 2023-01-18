#include "LedControl.h"
#include <LiquidCrystal.h>
#include <dcf77.h>

char* day_of_week[8] = {"", "Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"};

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 2, en = 4, rw = 3, d4 = 5, d5 = 6, d6 = 7, d7 = 8;
LiquidCrystal lcd(rs, rw, en, d4, d5, d6, d7);

/*
LedControl(DIN, CLK, LOAD, NUMBER_OF_DEVICES)
*/
// 9 - PB1 - Pin 15, 10 - PB2 - Pin 16, A0 - ADC0 - Pin 23 (Pins for DIP28)
const int lc_din = 9, lc_cs = A0, lc_clk = 10;
LedControl lc=LedControl(lc_din, lc_clk, lc_cs, 1);


// --- begin dcf77 stuff ---
const uint8_t dcf77_analog_sample_pin = A3;     // A3 - Pin 26 on DIP28
const uint8_t dcf77_sample_pin = A3;
const uint8_t dcf77_inverted_samples = 0;
const uint8_t dcf77_analog_samples = 1;
const uint8_t dcf77_pin_mode = INPUT;  // disable internal pull up
const uint8_t dcf77_monitor_led = A2;    // A2 - ADC2 - Pin 25 on DIP28

uint8_t ledpin(const uint8_t led) {
    return led;
}

uint8_t sample_input_pin() {
    const uint8_t sampled_data =
        dcf77_inverted_samples ^ (dcf77_analog_samples? (analogRead(dcf77_analog_sample_pin) > 200)
                                                      : digitalRead(dcf77_sample_pin));
    digitalWrite(ledpin(dcf77_monitor_led), sampled_data);
    return sampled_data;
}
// --- end dcf77 stuff ---

/*
Print a number in range 0 - 99 at LED display.
The number is always printes with 2 digits.
The lower digit is printed at position, the igher digit is printes at position + 1
*/
void printNumberOnLED(BCD::bcd_t number, uint8_t position) {
    lc.setDigit(0, position, number.digit.lo, false);
    lc.setDigit(0, position + 1, number.digit.hi, false);
}

void setup() {
    using namespace Clock;

    /* The MAX72XX of LED is in power-saving mode on startup, we have to do a wakeup call */
    lc.shutdown(0,false);
    /* Set the brightness to a medium values */
    lc.setIntensity(0,8);
    /* and clear the display */
    lc.clearDisplay(0);

    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);

    /*
    lcd.setCursor(0, 0);
    lcd.print("12:00:00+01");
    lcd.setCursor(15, 0);
    lcd.write(0x5C);
    lcd.setCursor(0, 1);
    lcd.print(F("So., 21-01-2001"));
    */

    lcd.print(F("Initializing..."));

    pinMode(ledpin(dcf77_monitor_led), OUTPUT);
    pinMode(dcf77_sample_pin, dcf77_pin_mode);

    DCF77_Clock::setup();
    DCF77_Clock::set_input_provider(sample_input_pin);


    // Wait till clock is synced, depending on the signal quality this may take
    // rather long. About 5 minutes with a good signal, 30 minutes or longer
    // with a bad signal
    for (uint8_t state = Clock::useless;
        state == Clock::useless || state == Clock::dirty;
        state = DCF77_Clock::get_clock_state()) {

        // wait for next sec
        Clock::time_t now;
        DCF77_Clock::get_current_time(now);

        // render one dot per second while initializing
        static uint16_t count = 0;
        lcd.setCursor(0, 1);
        lcd.print(count);

        BCD::bcd_t bcd_count = BCD::int_to_bcd(count % 60);
        printNumberOnLED(bcd_count, 0);

        ++count;
    }
}

void paddedPrint(BCD::bcd_t n) {
    lcd.print(n.digit.hi);
    lcd.print(n.digit.lo);
}

void loop() {
    Clock::time_t now;

    DCF77_Clock::get_current_time(now);
    if (now.month.val > 0) {
        lcd.clear();
        lcd.setCursor(0, 1);

        lcd.print((char*)day_of_week[now.weekday.digit.lo]);
        lcd.print("., ");
        paddedPrint(now.day);
        lcd.print('-');
        paddedPrint(now.month);
        lcd.print('-');
        lcd.print(F("20"));
        paddedPrint(now.year);

        lcd.setCursor(15, 0);

        switch (DCF77_Clock::get_clock_state()) {
            case Clock::useless: lcd.print(F("u")); break;
            case Clock::dirty:   lcd.print(F("d")); break;
            case Clock::synced:  lcd.print(F("\x5C")); break;
            case Clock::locked:  lcd.print(F("l")); break;
        }

        lcd.setCursor(0, 0);
        paddedPrint(now.hour);
        lcd.print(':');
        paddedPrint(now.minute);
        lcd.print(':');
        paddedPrint(now.second);
        lcd.print("+0");
        lcd.print(now.uses_summertime? '2': '1');

        printNumberOnLED(now.second, 0);
        lc.setChar(0, 2, '-', false);
        printNumberOnLED(now.minute, 3);
        lc.setChar(0, 5, '-', false);
        printNumberOnLED(now.hour, 6);
    }
}

