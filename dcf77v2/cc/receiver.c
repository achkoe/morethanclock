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

unsigned char rlist[60];

unsigned char state_0(void) {

}


unsigned char state_1(unsigned char count) {

}



void receive() {
    unsigned char state;
    while (1) {
        state = 0;
        while (1) {
            if (state == 0) {
                state = state_0();
            }
            else if (state == 1) {
                state = state_1(state);
            }
            else if (state > 1 && state < 60) {
                state = state_1(state);
            }
            else {
                printf("done\n");
                // print rlist and decode
            }
        }
    }
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
        if (state == STATE_0) {
            // syncing phase
            pin = getpin();
            tcurrent = ticks_ms();
            if (pin != ppin) {
                printf("[pin=%i|ppin=%i|t=%" PRId64 "\n" , pin, ppin, ticks_ms() - tstart);
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
                count = 1;
                state = STATE_1;
            }
        }
        else if (state == STATE_1) {
            // coming from STATE_0 or STATE_2, thus pin is always 1
            pin = getpin();
            tcurrent = ticks_ms();
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
            pin = getpin();
            tcurrent = ticks_ms();
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
                    count += 1;
                    printf("count=%i, dt=%" PRId64 "-> 0\n", count, tdiff);
                    t = tcurrent;
                    state = STATE_1;
                } else if (tdiff > 190 && tdiff < 220) {
                    // pin is 0 for 190ms ... 220ms -> 1
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

        if (count >= 60) {
            printf("received complete: STATE_%i -> STATE_0\n", state);
            count = 1;
            state = STATE_0;
        }
    }
}


