"""
DCF77 Simulator

From https://www.heret.de/funkuhr/log_19990909.htm

Sekunde 0-14: sind nicht belegt und werden hier auch nicht angezeigt!
Sekunde   15: Reserveantenne                Sekunde 21-28: Minute mit Pruefbit
Sekunde   16: Wechsel von MEZ <> MESZ       Sekunde 29-35: Stunde mit Pruefbit
Sekunde   17: Sommerzeit                    Sekunde 36-41: Tag
Sekunde   18: Winterzeit                    Sekunde 42-44: Wochentag
Sekunde   19: Schaltsekunde                 Sekunde 45-49: Monat
Sekunde   20: Beginn des Zeitprotokolls     Sekunde 50-58: Jahr mit Pruefbit

1    2 2         3      3   4  4   4     5
567890 12345678 9012345 678901 234 56789 012345678
Info   Minute P StundeP Tag    WoT Monat Jahr    P  Datum:         Zeit:        Sonderinformationen:
====================================================================================================
001001 10011100 0001001 100100 001 10010 100110011  Do, 09.09.1999 08:39:00, SZ


LOW  is 0.1s
HIGH is 0.2s
"""
import time
import RPi.GPIO as GPIO

# Pin 36: 3V3
# Pin 38: GND
# Pin 34: GP28

PIN = 14
GPIO.setmode(GPIO.BCM)
GPIO.setup(PIN, GPIO.IN)


rlist = []


c_pin = GPIO.input(PIN)
p_pin = c_pin


def _getpin():
    global c_pin, p_pin
    c_pin = PIN.value()
    if c_pin != p_pin:
        time.sleep(tfilter)
        if PIN.value() == c_pin:
            p_pin = c_pin
    LED.value(p_pin)
    return p_pin ^ 1


def getpin():
    return GPIO.input(PIN)


def decode(rlist):
    """
    Sekunde     0: Minutenmarke (kennzeichnet den Beginn)
    Sekunde  1-14: Bereitgestellte Daten von BBK und Meteo Time
    Sekunde    15: Rufbit
    Sekunde    16: Wechsel von MEZ <> MESZ
    Sekunde    17: Sommerzeit
    Sekunde    18: Normalzeit
    Sekunde    19: Schaltsekunde
    Sekunde    20: Beginn des Zeitprotokolls
    Sekunde 21-28: Minute mit Parität
    Sekunde 29-35: Stunde mit Parität
    Sekunde 36-41: Tag
    Sekunde 42-44: Wochentag
    Sekunde 45-49: Monat
    Sekunde 50-58: Jahr mit Parität für Datum
    Sekunde    59: Kein Impuls oder Schaltsekunde

    Returns None if any error in rlist detectd, else a dict with keys
    minute: int 0 ... 59
    minute_valid: bool
    hour:   int 0 ... 23
    hour_valid: bool
    month:  int 1 ... 12
    year:   int 2000 ... 2165
    date_valid: bool
    day_of_month: int 1 ... 31
    day_of_week: int 1 ... 1
    mez_mesz_anounce: bool
    mesz: bool
    leapsecond_anounce: bool
    """
    if rlist[0] != 0 or rlist[20] != 1:
        # indicate error if either bit 0 is not 0 or bit 20 is not 1
        return None
    rdict = dict()
    rdict["mesz"] = True if rlist[17] == 1 and rlist[18] == 0 else False
    rdict["mez_mesz_anounce"] = True if rlist[16] == 1 else False
    rdict["leapsecond_anounce"] = True if rlist[19] == 1 else False
    rdict["minute"] = rlist[21] * 1 + rlist[22] * 2 + rlist[23] * 4 + rlist[24] * 8 + rlist[25] * 10 + rlist[26] * 20 + rlist[27] * 40
    rdict["minute_valid"] = rlist[21:28].count(1) % 2 == rlist[28]
    rdict["hour"] = rlist[29] * 1 + rlist[30] * 2 + rlist[31] * 4 + rlist[32] * 8 + rlist[33] * 10 + rlist[34] * 20
    rdict["hour_valid"] = rlist[30:35].count(1) % 2 == rlist[35]
    rdict["day_of_month"] = rlist[36] * 1 + rlist[37] * 2 + rlist[38] * 4 + rlist[39] * 8 + rlist[40] * 10 + rlist[41] * 20
    rdict["day_of_week"] = rlist[42] * 1 + rlist[43] * 2 + rlist[44] * 4
    rdict["month"] = rlist[45] * 1 + rlist[46] * 2 + rlist[47] * 4 + rlist[48] * 8 + rlist[49] * 10
    rdict["year"] = 2000 + rlist[50] * 1 + rlist[51] * 2 + rlist[52] * 4 + rlist[53] * 8 + rlist[54] * 10 + rlist[55] * 20 + rlist[56] * 40 + rlist[57] * 80
    rdict["date_valid"] = rlist[36:58].count(1) % 2 == rlist[58]
    return rdict


def state_0():
    global rlist
    ts = time.time()
    t = time.time()
    while True:
        if getpin() != 1:
            t = time.time()
        if time.time() - t > 1.0:
            print(time.time() - t)
            # breaking the loop if pin is 1 for at least 0.9s
            break
    print("sync", time.time() - ts)
    rlist = []
    return 1


def state_1(count):
    global rlist
    t = time.time()
    # coming from state_0 or state_1, thus pin is always 1
    while getpin() != 0:
        if time.time() - t > 1.000:
            # no transition to 0 received for more than 1s
            print("1: go to state_0")
            return 0
    # pin is 0
    t = time.time()
    while getpin() != 1:
        if time.time() - t > 0.300:
            # pin is 0 for more than 0.3s, thus going back to state_0
            print("2: go to state_0")
            return 0
    # pin is 1 again
    t = time.time() - t
    if t > 0.090 and t < 0.120:
        # pin was 0 for roundabout 0.1s, thus it is a 0
        print(f"3: {count:02} - {t:6.1} -> 0")
        rlist.append(0)
    elif t > 0.190 and t < 0.220:
        # pin was 0 for roundabout 0.2s, thus it is a 1
        print(f"3: {count:02} - {t:6.1} -> 1")
        rlist.append(1)
    else:
        # pin was 0 for a longer time, thus go to state_0
        print(f"4: {t:6.1} -> go to state_0")
        return 0
    return count + 1


def receive():
    global rlist
    while True:
        state = 0
        while True:
            if state == 0:
                state = state_0()
            elif state == 1:
                state = state_1(state)
            elif state > 1 and state < 60:
                state = state_1(state)
            else:
                print("done")
                print("".join(str(r) for r in rlist))
                print(decode(rlist))
                break


if __name__ == "__main__":
    print("RECEIVER", 1 ^ 1, 0 ^ 1)
    receive()
