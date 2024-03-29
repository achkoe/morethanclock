#include <gpiod.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>


struct gpiod_chip *chip;
struct gpiod_line_request_config config;
struct gpiod_line_bulk lines;

// DCF77 input pin
#define PIN_IN 14

// the states of DCF77 receiver
#define STATE_0 0
#define STATE_1 1
#define STATE_2 2

// Log levels
#define DEBUG 10
#define INFO 20
#define LOGLEVEL INFO


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
    unsigned char state;
    char ppin;
    int64_t t;
    int64_t tstart;
    int count;
    unsigned char recvbuffer[61];
} receive_t;

receive_t receive_s;

typedef struct {
    int64_t t;
    time_t localtime;
    unsigned char update;
} localtime_t;

localtime_t localtime_s;

struct tm tm;


void logfn(int level, const char* format, ...) {
    va_list args;
    if (level < LOGLEVEL) return;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}


int initialise(void) {
    return 0;
}


int getpin() {
    unsigned int offsets[1];

    int values[1];
    int err;

    chip = gpiod_chip_open("/dev/gpiochip0");
    if(!chip) {
        perror("gpiod_chip_open");
        goto cleanup;
    }

    // use pin 14 as input
    offsets[0] = 14;
    values[0] = -1;

    err = gpiod_chip_get_lines(chip, offsets, 1, &lines);
    if(err) {
        perror("gpiod_chip_get_lines");
        goto cleanup;
    }

    memset(&config, 0, sizeof(config));
    config.consumer = "input example";
    config.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
    config.flags = 0;

    err = gpiod_line_request_bulk(&lines, &config, values);
    if(err) {
        perror("gpiod_line_request_bulk");
        goto cleanup;
    }

    err = gpiod_line_get_value_bulk(&lines, values);
    if(err) {
        perror("gpiod_line_get_value_bulk");
        goto cleanup;
    }

    // printf("value of gpio line %d=%d\n", offsets[0], values[0]);

    cleanup:
        gpiod_line_release_bulk(&lines);
        gpiod_chip_close(chip);

    return values[0];
    //return values[0] ^ 1;
}


int64_t ticks_ms() {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return ((int64_t) now.tv_sec) * 1000 + ((int64_t) now.tv_nsec) / 1000000;

    return 1000.0 * (long double)clock() / CLOCKS_PER_SEC;
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


void dcf77_receive() {
    int64_t tcurrent;
    int64_t tdiff;
    int pin;

    pin = getpin();
    tcurrent = ticks_ms();
    if (receive_s.state == STATE_0) {
        // syncing phase
        if (pin != receive_s.ppin) {
            logfn(DEBUG, "%i -> %i|t=%" PRId64 "\n" , receive_s.ppin, pin, ticks_ms() - receive_s.tstart);
            fflush(stdout);
            receive_s.tstart = tcurrent;
            receive_s.ppin = pin;
        }
        if (pin != 1) {
            receive_s.t = tcurrent;
        }
        if ((tcurrent - receive_s.t) > 1000) {
            // breaking the loop if pin is 1 for at least 0.9s
            logfn(INFO, "[pin=%i|ppin=%i|dt=%" PRId64 ": STATE_0 -> STATE_1\n" , pin, receive_s.ppin, tcurrent - receive_s.t);
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
                logfn(DEBUG, "count=%i, dt=%" PRId64 "-> 0\n", receive_s.count, tdiff);
                receive_s.t = tcurrent;
                receive_s.state = STATE_1;
            } else if (tdiff > 190 && tdiff < 220) {
                // pin is 0 for 190ms ... 220ms -> 1
                receive_s.recvbuffer[receive_s.count] = 1;
                receive_s.count += 1;
                logfn(DEBUG, "count=%i, dt=%" PRId64 "-> 1\n", receive_s.count, tdiff);
                receive_s.t = tcurrent;
                receive_s.state = STATE_1;
            } else {
                // pin is 0 for a longer time, thus go to state_0
                receive_s.count = 0;
                logfn(DEBUG, "4: dt=%" PRId64 ": STATE_1 -> STATE_0\n", tdiff);
                receive_s.t = tcurrent;
                receive_s.state = STATE_0;
            }
        }
    }
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


void main() {
    if (initialise() != 0) {
        return;
    }

    logfn(INFO, "START\n");

    receive_s.count = 0;
    receive_s.recvbuffer[60] = 0;
    receive_s.state = STATE_0;
    receive_s.ppin = getpin();
    receive_s.tstart = ticks_ms();
    receive_s.t = ticks_ms();

    localtime_s.localtime = 0;

    logfn(INFO, "ppin=%i\n", receive_s.ppin);

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


