#include <gpiod.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>


struct gpiod_chip *chip;
struct gpiod_line_request_config config;
struct gpiod_line_bulk lines;

#define PIN_IN 14
#define TFORMAT "%" PRId64 "\n"
unsigned char recvbuffer[61];


typedef struct {
    unsigned int mesz;
    unsigned int mez_mesz_anounce;
    unsigned int leapsecond_anounce;
    unsigned int minute;
    unsigned int minute_valid;
    unsigned int hour;
    unsigned int hour_valid;
    unsigned int day_of_month;
    unsigned int day_of_week;
    unsigned int month;
    unsigned int year;
    unsigned int date_valid;
    unsigned int frame_valid;
} dcf77_t;

dcf77_t dcf77;

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

    if ((recvbuffer[0] != 0) || (recvbuffer[20] != 1)) {
        dcf77.frame_valid = 0;
        return;
    }
    dcf77.mesz = (recvbuffer[17] == 1) && (recvbuffer[18] == 0);
    dcf77.mez_mesz_anounce = recvbuffer[16] == 1;
    dcf77.leapsecond_anounce = recvbuffer[19] == 1;

    count = 0;
    for (index = 36; index < 58; index++) {
        count += recvbuffer[index];
    }
    dcf77.date_valid = count % 2 == recvbuffer[58];

    count = 0;
    for (index = 21; index < 28; index++) {
        count += recvbuffer[index];
    }
    dcf77.minute_valid = count % 2 == recvbuffer[28];

    count = 0;
    for (index = 30; index < 35; index++) {
        count += recvbuffer[index];
    }
    dcf77.hour_valid = count % 2 == recvbuffer[35];

    dcf77.minute = recvbuffer[21] * 1 + recvbuffer[22] * 2 + recvbuffer[23] * 4 + recvbuffer[24] * 8 + recvbuffer[25] * 10 + recvbuffer[26] * 20 + recvbuffer[27] * 40;
    dcf77.hour = recvbuffer[29] * 1 + recvbuffer[30] * 2 + recvbuffer[31] * 4 + recvbuffer[32] * 8 + recvbuffer[33] * 10 + recvbuffer[34] * 20;
    dcf77.day_of_month = recvbuffer[36] * 1 + recvbuffer[37] * 2 + recvbuffer[38] * 4 + recvbuffer[39] * 8 + recvbuffer[40] * 10 + recvbuffer[41] * 20;
    dcf77.day_of_week = recvbuffer[42] * 1 + recvbuffer[43] * 2 + recvbuffer[44] * 4;
    dcf77.month = recvbuffer[45] * 1 + recvbuffer[46] * 2 + recvbuffer[47] * 4 + recvbuffer[48] * 8 + recvbuffer[49] * 10;
    dcf77.year = 2000 + recvbuffer[50] * 1 + recvbuffer[51] * 2 + recvbuffer[52] * 4 + recvbuffer[53] * 8 + recvbuffer[54] * 10 + recvbuffer[55] * 20 + recvbuffer[56] * 40 + recvbuffer[57] * 80;
}

void _main() {
    int s;

    s = getpin();
    printf("%i\n", s);
}

#define STATE_0 0
#define STATE_1 1
#define STATE_2 2


void main() {
    int64_t t;
    int64_t tcurrent;
    int64_t tstart;
    int64_t tdiff;
    unsigned char state, pstate;
    int pin;
    int ppin;
    int count = 0;
    recvbuffer[60] = 0;

    if (initialise() != 0) {
        return;
    }

    printf("START\n");
    state = STATE_0;
    pstate = STATE_0;
    tstart = ticks_ms();
    t = ticks_ms();
    ppin = getpin();
    printf("ppin=%i\n", ppin);

    while (1) {
        pin = getpin();
        tcurrent = ticks_ms();
        if (state == STATE_0) {
            // syncing phase
            if (pin != ppin) {
                printf("[%i -> %i|t=%" PRId64 "\n" , ppin, pin, ticks_ms() - tstart);
                fflush(stdout);
                tstart = tcurrent;
                ppin = pin;
            }
            if (pin != 1) {
                t = tcurrent;
            }
            if ((tcurrent - t) > 1000) {
                // breaking the loop if pin is 1 for at least 0.9s
                printf("[pin=%i|ppin=%i|dt=%" PRId64 ": STATE_0 -> STATE_1\n" , pin, ppin, tcurrent - t);
                t = tcurrent;
                count = 0;
                state = STATE_1;
            }
        }
        else if (state == STATE_1) {
            // coming from STATE_0 or STATE_2, thus pin is always 1
            if (pin == 1) {
                if (tcurrent - t > 1000) {
                    printf("1: STATE_1 -> STATE_0\n");
                    t = tcurrent;
                    state = STATE_0;
                }
            } else {
                printf("2: STATE_1 -> STATE_2\n");
                t = tcurrent;
                state = STATE_2;
            }
        }
        else if (state == STATE_2) {
            // coming from STATE_1, thus pin is 0
            if (pin == 0) {
                if (tcurrent - t > 300) {
                    // pin is 0 for more than 0.3s, thus going back to state_0
                    printf("3: STATE_1 -> STATE_0\n");
                    t = tcurrent;
                    state = STATE_0;
                }
            } else {
                // pin is 1 again
                tdiff = tcurrent - t;
                if (tdiff > 90 && tdiff < 120) {
                    // pin is 0 for 90ms ... 120ms -> 0
                    recvbuffer[count] = 0;
                    count += 1;
                    printf("count=%i, dt=%" PRId64 "-> 0\n", count, tdiff);
                    t = tcurrent;
                    state = STATE_1;
                } else if (tdiff > 190 && tdiff < 220) {
                    // pin is 0 for 190ms ... 220ms -> 1
                    recvbuffer[count] = 1;
                    count += 1;
                    printf("count=%i, dt=%" PRId64 "-> 1\n", count, tdiff);
                    t = tcurrent;
                    state = STATE_1;
                } else {
                    // pin is 0 for a longer time, thus go to state_0
                    count = 0;
                    printf("4: dt=%" PRId64 ": STATE_1 -> STATE_0\n", tdiff);
                    t = tcurrent;
                    state = STATE_0;
                }
            }
        }

        if (count >= 59) {
            printf("received complete, count=%i: STATE_%i -> STATE_0\n", count, state);
            for (count = 0; count < 59; count++) {
                printf("%c", recvbuffer[count] ? '1': '0');
            }
            printf("\n");
            decode();
            printf("hour=%d\n", dcf77.hour);
            printf("minute=%d\n", dcf77.minute);
            printf("day_of_month=%d\n", dcf77.day_of_month);
            printf("month=%d\n", dcf77.month);
            printf("year=%d\n", dcf77.year);
            printf("day_of_week=%d\n", dcf77.day_of_week);
            printf("hour_valid=%d\n", dcf77.hour_valid);
            printf("minute_valid=%d\n", dcf77.minute_valid);
            printf("date_valid=%d\n", dcf77.date_valid);
            printf("mesz=%d\n", dcf77.mesz);
            printf("mez_mesz_anounce=%d\n", dcf77.mez_mesz_anounce);
            printf("leapsecond_anounce=%d\n", dcf77.leapsecond_anounce);
            count = 0;
            state = STATE_0;
        }
    }
}


