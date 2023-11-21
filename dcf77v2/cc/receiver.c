#include <stdio.h>
#include <time.h>
#include <pigpio.h>
#include <unistd.h>

#define PIN_IN 14



int initialise(void) {
    if (gpioInitialise() < 0) {
        printf("pigpio initialisation failed.\n");
        return -1;
    }
    gpioSetMode(PIN_IN, PI_INPUT);
    return 0;
}

unsigned char getpin() {
    return gpioRead(PIN_IN) ^ 1;
}

long double ticks_ms() {
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



void main() {
    int s;
    clock_t t, tl;
    long double tms, tlms;

    if (initialise() != 0) {
        return;
    }
    tl = clock();
    tlms = ticks_ms();
    while (1) {
        s = getpin();
        t = clock();
        tms = ticks_ms();
        printf("%5.3f  %6.3Lf -> %i\n", ((float)t - (float)tl) / CLOCKS_PER_SEC, tms - tlms, s);
        tl = t;
        tlms = tms;
        while (getpin() == s) {}
    }
}