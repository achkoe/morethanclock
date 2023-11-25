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
from time import sleep
from datetime import datetime
import RPi.GPIO as GPIO


PIN = 17
GPIO.setmode(GPIO.BCM)
GPIO.setup(PIN, GPIO.OUT)
GPIO.output(PIN, GPIO.HIGH)


def int_to_bcd(number, digits):
    s = "{:02}".format(number)
    s = "{:04b}".format(int(s[0])) + "{:04b}".format(int(s[1]))
    s = s[::-1]
    return s[:digits]


def parity(s):
    return '0' if s.count('1') % 2 == 0 else '1'

# TODO: consider SZ, WZ

def encode_to_dcf77(dt):
    rs = "000000000000000" + "001001"
    s = int_to_bcd(dt.minute, 7)
    p = parity(s)
    rs = rs + s + p
    s = int_to_bcd(dt.hour, 6)
    p = parity(s)
    rs = rs + s + p

    day = int_to_bcd(dt.day, 6)

    # 1 Montag und 7 Sonntag
    dow = int_to_bcd(dt.isoweekday(), 3)

    month = int_to_bcd(dt.month, 5)

    year = ("{:04}".format(dt.year))[-2:]
    year = int(("{:04}".format(dt.year))[-2:])
    year = int_to_bcd(year, 8)

    p = parity(day + dow + month + year)
    rs = rs + day + dow + month + year + p
    return rs


def emit(s):
    print(len(s))
    t = time.time()
    for index, c in enumerate(s):
        print(f"{index:02} {c} {(time.time() - t):4.1f}")
        t = time.time()
        if c == "0":
            GPIO.output(PIN, GPIO.LOW)
            sleep(0.1)
            GPIO.output(PIN, GPIO.HIGH)
            sleep(0.9)
        else:
            GPIO.output(PIN, GPIO.LOW)
            sleep(0.2)
            GPIO.output(PIN, GPIO.HIGH)
            sleep(0.8)
    sleep(1)


def test_single():
    ts = "0 11111111111111 001001 10011100 0001001 100100 001 10010 100110011"
    #     0 11111111111111 001001 10011100 0001001 100100 001 10010 100110011
    #     0 11111111111111 001001 10011100 0001001 100100 001 10010 100110011
    # ts = "00001111"

    ts = ts.replace(" ", "")

    while 1:
        emit(ts)


def test_contiuous():
    while datetime.now().second != 0:
        pass
    while 1:
        t = datetime.now()
        print(t)
        r = encode_to_dcf77(t)
        emit(r)


if __name__ == "__main__":
    test_contiuous()