/*
export PICO_SDK_PATH=~/pico/pico-sdk/
cd build
make
cp dcf77.uf2 /media/achimk/RPI-RP2

optional:
cmake ..
*/

#include <stdarg.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <time.h>


// the states of DCF77 receiver
#define STATE_0 0
#define STATE_1 1
#define STATE_2 2

// Log levels
#define DEBUG 10
#define INFO 20
#define LOGLEVEL INFO


const uint LED_PIN = 25;
const uint PIN_IN = 28;
const uint PIN_BUTTON = 15;

typedef struct {
    unsigned char state;
    char ppin;
    int64_t t;
    int64_t tstart;
    int count;
    unsigned char recvbuffer[61];
} receive_t;

receive_t receive_s;

typedef struct {
    unsigned char mesz;
    unsigned char mez_mesz_anounce;
    unsigned char leapsecond_anounce;
    unsigned char minute;
    unsigned char minute_valid;
    unsigned char hour;
    unsigned char hour_valid;
    unsigned char day_of_month;
    unsigned char day_of_week;
    unsigned char month;
    unsigned int year;
    unsigned char date_valid;
    unsigned char frame_valid;
} dcf77_t;

dcf77_t dcf77;

typedef struct {
    int64_t t;
    time_t localtime;
    unsigned char update;
} localtime_t;

localtime_t localtime_s;

struct tm tm;


void initialize() {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_init(PIN_IN);
    gpio_init(PIN_BUTTON);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_dir(PIN_IN, GPIO_IN);
    gpio_set_dir(PIN_BUTTON, GPIO_IN);
    gpio_pull_down(PIN_BUTTON);
}


int getpin() {
    return gpio_get(PIN_IN);
}


int64_t ticks_ms() {
    return  time_us_64() / 1000;
}


void logfn(int level, const char* format, ...) {
    va_list args;
    if (level < LOGLEVEL) return;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}


void dcf77_receive() {
    int64_t tcurrent;
    int64_t tdiff;
    int pin;

    pin = getpin();
    gpio_put(LED_PIN, pin);
    tcurrent = ticks_ms();
    if (receive_s.state == STATE_0) {
        // syncing phase
        if (pin != receive_s.ppin) {
            logfn(DEBUG, "%i -> %i|t=%lld\n" , receive_s.ppin, pin, ticks_ms() - receive_s.tstart);
            fflush(stdout);
            receive_s.tstart = tcurrent;
            receive_s.ppin = pin;
        }
        if (pin != 1) {
            receive_s.t = tcurrent;
        }
        if ((tcurrent - receive_s.t) > 1000) {
            // breaking the loop if pin is 1 for at least 0.9s
            logfn(INFO, "[pin=%i|ppin=%i|dt=%lld: STATE_0 -> STATE_1\n" , pin, receive_s.ppin, tcurrent - receive_s.t);
            receive_s.t = tcurrent;
            receive_s.count = 0;
            receive_s.state = STATE_1;
        }
    }
    else if (receive_s.state == STATE_1) {
        // coming from STATE_0 or STATE_2, thus pin is always 1
        if (pin == 1) {
            if (tcurrent - receive_s.t > 1000) {
                logfn(DEBUG, "1: STATE_1 -> STATE_0\n");
                receive_s.t = tcurrent;
                receive_s.state = STATE_0;
            }
        } else {
            logfn(DEBUG, "2: STATE_1 -> STATE_2\n");
            receive_s.t = tcurrent;
            receive_s.state = STATE_2;
        }
    }
    else if (receive_s.state == STATE_2) {
        // coming from STATE_1, thus pin is 0
        if (pin == 0) {
            if (tcurrent - receive_s.t > 300) {
                // pin is 0 for more than 0.3s, thus going back to state_0
                logfn(DEBUG, "3: STATE_1 -> STATE_0\n");
                receive_s.t = tcurrent;
                receive_s.state = STATE_0;
            }
        } else {
            // pin is 1 again
            tdiff = tcurrent - receive_s.t;
            if (tdiff > 90 && tdiff < 120) {
                // pin is 0 for 90ms ... 120ms -> 0
                receive_s.recvbuffer[receive_s.count] = 0;
                receive_s.count += 1;
                logfn(DEBUG, "count=%i, dt=%lld-> 0\n", receive_s.count, tdiff);
                receive_s.t = tcurrent;
                receive_s.state = STATE_1;
            } else if (tdiff > 190 && tdiff < 220) {
                // pin is 0 for 190ms ... 220ms -> 1
                receive_s.recvbuffer[receive_s.count] = 1;
                receive_s.count += 1;
                logfn(DEBUG, "count=%i, dt=%lld-> 1\n", receive_s.count, tdiff);
                receive_s.t = tcurrent;
                receive_s.state = STATE_1;
            } else {
                // pin is 0 for a longer time, thus go to state_0
                receive_s.count = 0;
                logfn(DEBUG, "4: dt=%lld: STATE_1 -> STATE_0\n", tdiff);
                receive_s.t = tcurrent;
                receive_s.state = STATE_0;
            }
        }
    }
}


/** Decode data in recvbuffer to dcf77 */
void decode() {
    int index, count;

    if ((receive_s.recvbuffer[0] != 0) || (receive_s.recvbuffer[20] != 1)) {
        dcf77.frame_valid = 0;
        return;
    }
    dcf77.mesz = (receive_s.recvbuffer[17] == 1) && (receive_s.recvbuffer[18] == 0);
    dcf77.mez_mesz_anounce = receive_s.recvbuffer[16] == 1;
    dcf77.leapsecond_anounce = receive_s.recvbuffer[19] == 1;

    count = 0;
    for (index = 36; index < 58; index++) {
        count += receive_s.recvbuffer[index];
    }
    dcf77.date_valid = count % 2 == receive_s.recvbuffer[58];

    count = 0;
    for (index = 21; index < 28; index++) {
        count += receive_s.recvbuffer[index];
    }
    dcf77.minute_valid = count % 2 == receive_s.recvbuffer[28];

    count = 0;
    for (index = 29; index < 35; index++) {
        count += receive_s.recvbuffer[index];
    }
    dcf77.hour_valid = count % 2 == receive_s.recvbuffer[35];

    dcf77.minute = receive_s.recvbuffer[21] * 1 + receive_s.recvbuffer[22] * 2 + receive_s.recvbuffer[23] * 4 + receive_s.recvbuffer[24] * 8 + receive_s.recvbuffer[25] * 10 + receive_s.recvbuffer[26] * 20 + receive_s.recvbuffer[27] * 40;
    dcf77.hour = receive_s.recvbuffer[29] * 1 + receive_s.recvbuffer[30] * 2 + receive_s.recvbuffer[31] * 4 + receive_s.recvbuffer[32] * 8 + receive_s.recvbuffer[33] * 10 + receive_s.recvbuffer[34] * 20;
    dcf77.day_of_month = receive_s.recvbuffer[36] * 1 + receive_s.recvbuffer[37] * 2 + receive_s.recvbuffer[38] * 4 + receive_s.recvbuffer[39] * 8 + receive_s.recvbuffer[40] * 10 + receive_s.recvbuffer[41] * 20;
    dcf77.day_of_week = receive_s.recvbuffer[42] * 1 + receive_s.recvbuffer[43] * 2 + receive_s.recvbuffer[44] * 4;
    dcf77.month = receive_s.recvbuffer[45] * 1 + receive_s.recvbuffer[46] * 2 + receive_s.recvbuffer[47] * 4 + receive_s.recvbuffer[48] * 8 + receive_s.recvbuffer[49] * 10;
    dcf77.year = 2000 + receive_s.recvbuffer[50] * 1 + receive_s.recvbuffer[51] * 2 + receive_s.recvbuffer[52] * 4 + receive_s.recvbuffer[53] * 8 + receive_s.recvbuffer[54] * 10 + receive_s.recvbuffer[55] * 20 + receive_s.recvbuffer[56] * 40 + receive_s.recvbuffer[57] * 80;
}


void dcf77_show_frame() {
    logfn(INFO, "received complete, count=%i: STATE_%i -> STATE_0\n", receive_s.count, receive_s.state);
    for (receive_s.count = 0; receive_s.count < 59; receive_s.count++) {
        logfn(INFO, "%c", receive_s.recvbuffer[receive_s.count] ? '1': '0');
    }
    logfn(INFO, "\n");
    logfn(INFO, "hour=%d\n", dcf77.hour);
    logfn(INFO, "minute=%d\n", dcf77.minute);
    logfn(INFO, "day_of_month=%d\n", dcf77.day_of_month);
    logfn(INFO, "month=%d\n", dcf77.month);
    logfn(INFO, "year=%d\n", dcf77.year);
    logfn(INFO, "day_of_week=%d\n", dcf77.day_of_week);
    logfn(INFO, "hour_valid=%d\n", dcf77.hour_valid);
    logfn(INFO, "minute_valid=%d\n", dcf77.minute_valid);
    logfn(INFO, "date_valid=%d\n", dcf77.date_valid);
    logfn(INFO, "mesz=%d\n", dcf77.mesz);
    logfn(INFO, "mez_mesz_anounce=%d\n", dcf77.mez_mesz_anounce);
    logfn(INFO, "leapsecond_anounce=%d\n", dcf77.leapsecond_anounce);
}


/** Advance localtime py 1 if 1s has elapsed.
 */
void localtime_update() {
    int64_t tcurrent;

    tcurrent = ticks_ms();
    if (tcurrent - localtime_s.t >= 1000) {
        localtime_s.update = 1;
        localtime_s.t = tcurrent;
        localtime_s.localtime += 1;
    }
}


/** Synchronize local time with time received from DCF77.
*/
void synchronize() {
    tm = *localtime(&localtime_s.localtime);
    if (dcf77.hour_valid == 1) {
        tm.tm_hour = dcf77.hour;
    }
    if (dcf77.minute_valid == 1) {
        tm.tm_min = dcf77.minute == 59 ? 0 : dcf77.minute + 1;
    }
    tm.tm_sec = 0;
    tm.tm_isdst = dcf77.mesz;
    if (dcf77.date_valid == 1) {
        tm.tm_mon = dcf77.month - 1;
        tm.tm_year = dcf77.year - 1900;
        tm.tm_mday = dcf77.day_of_month;
    }
    localtime_s.localtime = mktime(&tm);
    localtime_s.t = ticks_ms();
    localtime_s.update = 1;
}


/*
 *    number  0  1  2  3  4  5  6  7  8  9
 * segment a  1  0  1  1  0  1  1  1  1  1  GPIO0
 * segment b  1  1  1  1  1  0  0  1  1  1  GPIO1
 * segment c  1  1  0  1  1  1  1  1  1  1  GPIO5
 * segment d  1  0  1  1  0  1  1  0  1  1  GPIO7
 * segment e  1  0  1  0  0  0  1  0  1  0  GPIO2
 * segment f  1  0  0  0  1  1  1  0  1  1  GPIO4
 * segment g  0  0  1  1  1  1  1  0  1  1  GPIO3
 * dp                                       GPIO6
*/

unsigned char const segments[11] = {
// Each value in this array represents the segments for displaying a number
// in a 7 segment display. The number corresponds to the index in the array.
// The segments are driven by GPIO as shown in comment.
// Used by function  gpio_set_mask(uint32_t mask)
//   -------------------------GPIO------------------------------------
//  0       1       2       3       4        5        6        7
    1 * 1 + 1 * 2 + 1 * 4 + 0 * 8 + 1 * 16 + 1 * 32 + 0 * 64 + 1 * 128, // 0
    0 * 1 + 1 * 2 + 0 * 4 + 0 * 8 + 0 * 16 + 1 * 32 + 0 * 64 + 0 * 128, // 1
    1 * 1 + 1 * 2 + 1 * 4 + 1 * 8 + 0 * 16 + 0 * 32 + 0 * 64 + 1 * 128, // 2
    1 * 1 + 1 * 2 + 0 * 4 + 1 * 8 + 0 * 16 + 1 * 32 + 0 * 64 + 1 * 128, // 3
    0 * 1 + 1 * 2 + 0 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 0 * 64 + 0 * 128, // 4
    1 * 1 + 0 * 2 + 0 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 0 * 64 + 1 * 128, // 5
    1 * 1 + 0 * 2 + 1 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 0 * 64 + 1 * 128, // 6
    1 * 1 + 1 * 2 + 0 * 4 + 0 * 8 + 0 * 16 + 1 * 32 + 0 * 64 + 0 * 128, // 7
    1 * 1 + 1 * 2 + 1 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 0 * 64 + 0 * 128, // 8
    1 * 1 + 1 * 2 + 0 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 0 * 64 + 1 * 128, // 9
    0 * 1 + 0 * 2 + 0 * 4 + 0 * 8 + 0 * 16 + 0 * 32 + 1 * 64 + 0 * 128, // DP
};

// GPIO pin driving digit        0   1   2   3  4   5   DP
unsigned char const digits[7] = {14, 13, 11, 9, 12, 10, 8};

struct {
    unsigned char digits[7];
    unsigned char position;
} display_s;

/**
 * This function is called periodically.
 * In every call the digit indicated by display_s.position is driven with
 * the pattern given by display_s.digit[position]
 */
void display_update(void) {
    display_s.position += 1;
    if (display_s.position > 6) {
        display_s.position = 0;
    }
    printf("%d : %d -> %02X\n", display_s.position, digits[display_s.position], display_s.digits[display_s.position]);
}


/*
    Set display contents.

    Args:
        content: array with 3 unsigned chars, [left, middle, right] content
        dp: bool, stands for 'decimal point', true means on, false means off
*/
void display_set(unsigned char *content, bool dp) {
    unsigned char index;
    char buffer[4];
    for (index = 0; index < 3; index++) {
        sprintf(buffer, "%02d", content[index]);
        display_s.digits[index * 2 + 0] = segments[buffer[0] - '0'];
        display_s.digits[index * 2 + 1] = segments[buffer[1] - '0'];
        display_s.digits[6] = dp == true ? segments[10] : 0;
    }
}


void _main() {
    initialize();

    receive_s.count = 0;
    receive_s.recvbuffer[60] = 0;
    receive_s.state = STATE_0;
    receive_s.ppin = getpin();
    receive_s.tstart = ticks_ms();
    receive_s.t = ticks_ms();

    localtime_s.localtime = 0;

    while (1) {
        dcf77_receive();
        if (receive_s.count >= 59) {
            decode();
            dcf77_show_frame();
            synchronize();
            receive_s.count = 0;
            receive_s.state = STATE_0;
        }
        if (localtime_s.update == 1) {
            localtime_s.update = 0;
            tm = *localtime(&localtime_s.localtime);
            printf("%s", asctime(&tm));
        }
        localtime_update();
    }
}

void __main() {
    int index;
    unsigned char content[3] = {12, 34, 56};

    initialize();
    //while (true) {
    while (ticks_ms() < 10000) {
        gpio_put(LED_PIN, 1);
        busy_wait_ms(500);
        gpio_put(LED_PIN, 0);
        busy_wait_ms(500);
        printf("%d\n", ticks_ms());
    }

    for (index = 0; index < 11; index++) {
        printf("%d: %02X\n", index, segments[index]);
    }
    display_set(content, true);
    for (index = 0; index < 7; index++) {
        display_update();
    }
    while (true) {};
}



void main() {
    unsigned char buttonState = 0;
    unsigned char lastButtonState = 0;
    unsigned char reading;
    bool ledState = true;
    int64_t lastDebounceTime = 0;
    int64_t debounceDelay = 50;

    initialize();
    gpio_put(LED_PIN, ledState);

    while (true) {
        reading = gpio_get(PIN_BUTTON);
        if (reading != lastButtonState) {
            // reset the debouncing timer
            lastDebounceTime = ticks_ms();
        }
        if ((ticks_ms() - lastDebounceTime) > debounceDelay) {
            // whatever the reading is at, it's been there for longer than the debounce
            // delay, so take it as the actual current state:

            // if the button state has changed:
            if (reading != buttonState) {
                buttonState = reading;

                // only toggle the LED if the new button state is HIGH
                if (buttonState == 1) {
                    ledState = !ledState;
                }
            }
        }
        gpio_put(LED_PIN, ledState);
        lastButtonState = reading;
    }
}