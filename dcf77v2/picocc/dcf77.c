/*
export PICO_SDK_PATH=~/pico/pico-sdk/
cd build
make
cp dcf77.uf2 /media/achimk/RPI-RP2

optional:
cmake ..
*/

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <time.h>


void display_dp(bool dp);


// the states of DCF77 receiver
#define STATE_0 0
#define STATE_1 1
#define STATE_2 2

// Log levels
#define DEBUG 10
#define INFO 20
#define LOGLEVEL DEBUG


const uint LED_PIN = 25;
const uint PIN_IN = 28;
const uint PIN_BUTTON = 15;

struct {
    unsigned char state;
    char ppin;
    int64_t t;
    int64_t tstart;
    int count;
    unsigned char recvbuffer[61];
} receive_s;


struct {
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
} dcf77_s;


struct {
    int64_t t;
    time_t localtime;
    unsigned char update;
} localtime_s;


struct tm tm;

/** time of last valid DCF time reception */
time_t last_synchronization_time;

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
    1 * 1 + 1 * 2 + 1 * 4 + 0 * 8 + 1 * 16 + 1 * 32 + 1 * 64 + 1 * 128, // 0
    0 * 1 + 1 * 2 + 0 * 4 + 0 * 8 + 0 * 16 + 1 * 32 + 1 * 64 + 0 * 128, // 1
    1 * 1 + 1 * 2 + 1 * 4 + 1 * 8 + 0 * 16 + 0 * 32 + 1 * 64 + 1 * 128, // 2
    1 * 1 + 1 * 2 + 0 * 4 + 1 * 8 + 0 * 16 + 1 * 32 + 1 * 64 + 1 * 128, // 3
    0 * 1 + 1 * 2 + 0 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 1 * 64 + 0 * 128, // 4
    1 * 1 + 0 * 2 + 0 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 1 * 64 + 1 * 128, // 5
    1 * 1 + 0 * 2 + 1 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 1 * 64 + 1 * 128, // 6
    1 * 1 + 1 * 2 + 0 * 4 + 0 * 8 + 0 * 16 + 1 * 32 + 1 * 64 + 0 * 128, // 7
    1 * 1 + 1 * 2 + 1 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 1 * 64 + 1 * 128, // 8
    1 * 1 + 1 * 2 + 0 * 4 + 1 * 8 + 1 * 16 + 1 * 32 + 1 * 64 + 1 * 128, // 9
    0 * 1 + 0 * 2 + 0 * 4 + 0 * 8 + 0 * 16 + 0 * 32 + 1 * 64 + 0 * 128, // DP
};

// GPIO pin driving digit        0   1   2   3  4   5   DP
unsigned char const digits[7] = {14, 13, 11, 9, 12, 10, 8};


struct {
    unsigned char digits[6];
    unsigned char position;
    bool dp;
} display_s;


struct {
    unsigned char currentButtonState;
    unsigned char lastButtonState;
    unsigned char readingButton;
    bool buttonChanged;
    int64_t lastDebounceTime;
    int64_t debounceDelay;

    int64_t tbutton;
    unsigned char level;
} button_s;


void initialize() {
    stdio_init_all();

    // initialize all uses pins
    // 33222222222211111111110000000000
    // 10987654321098765432109876543210
    // 00010010000000001111111111111111 = 1200FFFF
    // 0001 0010 0000 0000 1111 1111 1111 1111 = 1200FFFF
    gpio_init_mask(0x1200FFFF);

    // set all output and input pins
    // 33222222222211111111110000000000
    // 10987654321098765432109876543210
    // 00000010000000000111111111111111
    // 0000 0010 0000 0000 0111 1111 1111 1111 = 02007FFF
    gpio_set_dir_masked(0x1200FFFF, 0x02007FFF);
    // use pullup for button pin
    gpio_pull_up(PIN_BUTTON);
}


int getpin() {
    return !gpio_get(PIN_IN);
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
    display_dp(pin);
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
        dcf77_s.frame_valid = 0;
        return;
    }
    dcf77_s.mesz = (receive_s.recvbuffer[17] == 1) && (receive_s.recvbuffer[18] == 0);
    dcf77_s.mez_mesz_anounce = receive_s.recvbuffer[16] == 1;
    dcf77_s.leapsecond_anounce = receive_s.recvbuffer[19] == 1;

    count = 0;
    for (index = 36; index < 58; index++) {
        count += receive_s.recvbuffer[index];
    }
    dcf77_s.date_valid = count % 2 == receive_s.recvbuffer[58];

    count = 0;
    for (index = 21; index < 28; index++) {
        count += receive_s.recvbuffer[index];
    }
    dcf77_s.minute_valid = count % 2 == receive_s.recvbuffer[28];

    count = 0;
    for (index = 29; index < 35; index++) {
        count += receive_s.recvbuffer[index];
    }
    dcf77_s.hour_valid = count % 2 == receive_s.recvbuffer[35];

    dcf77_s.minute = receive_s.recvbuffer[21] * 1 + receive_s.recvbuffer[22] * 2 + receive_s.recvbuffer[23] * 4 + receive_s.recvbuffer[24] * 8 + receive_s.recvbuffer[25] * 10 + receive_s.recvbuffer[26] * 20 + receive_s.recvbuffer[27] * 40;
    dcf77_s.hour = receive_s.recvbuffer[29] * 1 + receive_s.recvbuffer[30] * 2 + receive_s.recvbuffer[31] * 4 + receive_s.recvbuffer[32] * 8 + receive_s.recvbuffer[33] * 10 + receive_s.recvbuffer[34] * 20;
    dcf77_s.day_of_month = receive_s.recvbuffer[36] * 1 + receive_s.recvbuffer[37] * 2 + receive_s.recvbuffer[38] * 4 + receive_s.recvbuffer[39] * 8 + receive_s.recvbuffer[40] * 10 + receive_s.recvbuffer[41] * 20;
    dcf77_s.day_of_week = receive_s.recvbuffer[42] * 1 + receive_s.recvbuffer[43] * 2 + receive_s.recvbuffer[44] * 4;
    dcf77_s.month = receive_s.recvbuffer[45] * 1 + receive_s.recvbuffer[46] * 2 + receive_s.recvbuffer[47] * 4 + receive_s.recvbuffer[48] * 8 + receive_s.recvbuffer[49] * 10;
    dcf77_s.year = 2000 + receive_s.recvbuffer[50] * 1 + receive_s.recvbuffer[51] * 2 + receive_s.recvbuffer[52] * 4 + receive_s.recvbuffer[53] * 8 + receive_s.recvbuffer[54] * 10 + receive_s.recvbuffer[55] * 20 + receive_s.recvbuffer[56] * 40 + receive_s.recvbuffer[57] * 80;
}


void dcf77_show_frame() {
    logfn(INFO, "received complete, count=%i: STATE_%i -> STATE_0\n", receive_s.count, receive_s.state);
    for (receive_s.count = 0; receive_s.count < 59; receive_s.count++) {
        logfn(INFO, "%c", receive_s.recvbuffer[receive_s.count] ? '1': '0');
    }
    logfn(INFO, "\n");
    logfn(INFO, "hour=%d\n", dcf77_s.hour);
    logfn(INFO, "minute=%d\n", dcf77_s.minute);
    logfn(INFO, "day_of_month=%d\n", dcf77_s.day_of_month);
    logfn(INFO, "month=%d\n", dcf77_s.month);
    logfn(INFO, "year=%d\n", dcf77_s.year);
    logfn(INFO, "day_of_week=%d\n", dcf77_s.day_of_week);
    logfn(INFO, "hour_valid=%d\n", dcf77_s.hour_valid);
    logfn(INFO, "minute_valid=%d\n", dcf77_s.minute_valid);
    logfn(INFO, "date_valid=%d\n", dcf77_s.date_valid);
    logfn(INFO, "mesz=%d\n", dcf77_s.mesz);
    logfn(INFO, "mez_mesz_anounce=%d\n", dcf77_s.mez_mesz_anounce);
    logfn(INFO, "leapsecond_anounce=%d\n", dcf77_s.leapsecond_anounce);
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
    if (dcf77_s.hour_valid == 1) {
        tm.tm_hour = dcf77_s.hour;
    }
    if (dcf77_s.minute_valid == 1) {
        // tm.tm_min = dcf77_s.minute == 59 ? 0 : dcf77_s.minute + 1;  // this
        tm.tm_min = dcf77_s.minute;                                    // this or that
    }
    tm.tm_sec = 0;
    tm.tm_isdst = dcf77_s.mesz;
    if (dcf77_s.date_valid == 1) {
        tm.tm_mon = dcf77_s.month - 1;
        tm.tm_year = dcf77_s.year - 1900;
        tm.tm_mday = dcf77_s.day_of_month;
    }
    localtime_s.localtime = mktime(&tm);
    localtime_s.t = ticks_ms();
    localtime_s.update = 1;

    last_synchronization_time = localtime_s.localtime;
}


/**
 * This function is called periodically.
 * In every call the digit indicated by display_s.position is driven with
 * the pattern given by display_s.digit[position]
 */
void display_update(void) {
    display_s.position += 1;
    if (display_s.position > 5) {
        display_s.position = 0;
    }
    // printf("%d : %d -> %02X\n", display_s.position, digits[display_s.position], display_s.digits[display_s.position]);
    // 33222222222211111111110000000000
    // 10987654321098765432109876543210
    // 11111101111111111111111011111111
    // 1111 1101 1111 1111 1111 1110 1111 1111
    // clear all bits except dp
    gpio_clr_mask(0xFDFFFEFF);
    // set all bits required to show digit
    gpio_set_mask(display_s.digits[display_s.position] | (1 << digits[display_s.position]));
}


/*
    Set display contents.

    Args:
        content: array with 3 unsigned chars, [left, middle, right] content
        dp: bool, stands for 'decimal point', true means on, false means off
*/
void display_set(unsigned char *content) {
    unsigned char index;
    char buffer[4];
    for (index = 0; index < 3; index++) {
        sprintf(buffer, "%02d", content[index]);
        display_s.digits[index * 2 + 0] = segments[buffer[0] - '0'];
        display_s.digits[index * 2 + 1] = segments[buffer[1] - '0'];
    }
    logfn(DEBUG,"digits=");
    for (index = 0; index < 6; index++) {
        logfn(DEBUG,"%02X ", display_s.digits[index]);
    }
    logfn(DEBUG,"\n");
}


void display_dp(bool on) {
    gpio_put(8, on ? 1 : 0);
}


void getButton() {
    button_s.readingButton = gpio_get(PIN_BUTTON);
    if (button_s.readingButton != button_s.lastButtonState) {
        // reset the debouncing timer
        button_s.lastDebounceTime = ticks_ms();
    }
    if ((ticks_ms() - button_s.lastDebounceTime) > button_s.debounceDelay) {
        // whatever the button_s.readingButton is at, it's been there for longer than the debounce
        // delay, so take it as the actual current state:

        // if the button state has changed:
        if (button_s.readingButton != button_s.currentButtonState) {
            button_s.currentButtonState = button_s.readingButton;
            button_s.buttonChanged = true;
            // only toggle the LED if the new button state is LOW
        }
    }
    button_s.lastButtonState = button_s.readingButton;
}


void wait_for_key() {
    int c;
    puts("press '.' to continue");
    do {
        c = getchar();
    } while (c != '.');
}


void test_display() {
    int index;
    int64_t t, tu, tdp;
    bool dp;
    unsigned char content[3] = {65, 43, 21};

    initialize();
    //while (true) {
    while (ticks_ms() < 10000) {
        gpio_put(LED_PIN, 1);
        busy_wait_ms(500);
        gpio_put(LED_PIN, 0);
        busy_wait_ms(500);
        printf("%lld\n", ticks_ms());
    }
    for (index = 0; index < 11; index++) {
        printf("%02d -> %04X\n", index, segments[index]);
    }


    int c, segment, digit;
    if (false) {
        for (index = 0; index < 11; index++) {
            printf("%d: %02X\n", index, segments[index]);
        }

        for (digit=0; digit<6; digit++) {
            for (segment=0; segment < 8; segment++) {
                gpio_put(segment, 1);
                gpio_put(digits[digit], 1);
                printf("press key\n");

                printf("segment %2d: press '.' to continue\n", segment);
                do {
                    c=getchar();
                    putchar (c);
                } while (c != '.');

                gpio_put(segment, 0);
                gpio_put(digits[digit], 0);
            }
        }
        printf("done\n");
    }


    display_set(content);

    tdp = ticks_ms();
    tu = tdp;
    while (true) {
        t = ticks_ms();
        if (t - tu > 2) {
            tu = t;
            display_update();
        }
        if (t - tdp > 1000) {
            tdp = t;
            display_dp(dp);
            dp = !dp;
        }
    };
}


void test_button() {
    initialize();

    button_s.currentButtonState = 1;
    button_s.lastButtonState = 1;
    button_s.lastDebounceTime = 0;
    button_s.debounceDelay = 50;
    button_s.buttonChanged = false;
    button_s.level = 0;

    while (true) {
        getButton();
        if (button_s.buttonChanged == true) {
            logfn(DEBUG, "buttonState %d\n", button_s.currentButtonState);
            button_s.buttonChanged = false;

            if (button_s.currentButtonState == 0) {
                button_s.level = button_s.level > 1 ? 0 : button_s.level + 1;
                logfn(INFO, "level=%i\n", button_s.level);
                button_s.tbutton = ticks_ms();
            }
        }

        if (button_s.level > 0 && ticks_ms() - button_s.tbutton > 10000) {
            logfn(INFO, "level <- 0\n");
            button_s.level = 0;
        }
    }
}


void _main() {
    unsigned char content[3];
    int64_t t_update;

    initialize();

    receive_s.count = 0;
    receive_s.recvbuffer[60] = 0;
    receive_s.state = STATE_0;
    receive_s.ppin = getpin();
    receive_s.tstart = ticks_ms();
    receive_s.t = ticks_ms();

    localtime_s.localtime = 0;

    t_update = ticks_ms();

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

            if (button_s.level == 0){
                // normal display -> display hours, minutes, seconds
                content[0] = tm.tm_hour;
                content[1] = tm.tm_min;
                content[2] = tm.tm_sec;
            } else if (button_s.level == 1) {
                // button one time pressed -> display day of month, month, year
                content[0] = tm.tm_mday;
                content[1] = tm.tm_mon + 1;
                content[2] = tm.tm_year >= 100 ? tm.tm_year - 100 : tm.tm_year;
            } else {
                // button two time pressed -> display day, hour, minute of time difference between now and last DCF77 reception
                time_t tdelta = (time_t)difftime(localtime_s.localtime, last_synchronization_time);
                lldiv_t quot_rem = lldiv(tdelta, 24 * 60 * 60);
                content[0] = (unsigned char)quot_rem.quot < 100 ? (unsigned char)quot_rem.quot : 99;
                quot_rem = lldiv(quot_rem.rem, 60 * 60);
                content[1] = (unsigned char)quot_rem.quot < 100 ? (unsigned char)quot_rem.quot : 99;
                quot_rem = lldiv(quot_rem.rem, 60);
                content[2] = (unsigned char)quot_rem.quot < 100 ? (unsigned char)quot_rem.quot : 99;
                logfn(INFO, "tdelta=%lld: content -> %d, %d, %d\n", tdelta, content[0], content[1], content[2]);
            }
            display_set(content);
            logfn(INFO, "%s", asctime(&tm));
        }
        localtime_update();

        getButton();
        if (button_s.buttonChanged == true) {
            button_s.buttonChanged = false;

            if (button_s.currentButtonState == 0) {
                button_s.level = button_s.level > 1 ? 0 : button_s.level + 1;
                logfn(INFO, "level=%i\n", button_s.level);
                button_s.tbutton = ticks_ms();
            }
        }

        if (button_s.level > 0 && ticks_ms() - button_s.tbutton > 10000) {
            logfn(INFO, "level <- 0\n");
            button_s.level = 0;
        }

        if (ticks_ms() - t_update > 2) {
            t_update = ticks_ms();
            display_update();
        }
    }
}


void main() {
    //test_display();
    _main();
}