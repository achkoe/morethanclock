# Setting arduino-cli


## Create init file

    arduino-cli config init


## Install core

    arduino-cli core update-index


## Install libraries

    arduino-cli lib install <libname>

    Required libraries are:
    - BME280: https://github.com/adafruit/Adafruit_BME280_Library
    - DCF77: https://playground.arduino.cc/Code/DCF77/
    - Time: https://playground.arduino.cc/Code/Time/
    - u8g2: https://github.com/olikraus/u8g2/wiki

    For Time Library and DCF77 library wotk together you have to
    copy are link ``TimeLib.h`` to ``Time.h``.


## Search the correct board

    arduino-cli board listall esp
    ...
    arduino:avr:nano

## Arduino-IDE
- select Tools/Board -> Arduino Nano
- select Tools/Processor -> ATMega328P (Old Bootloader)
- select Tools/Programmer -> AVRISP mkII


## Compile

    arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328old morethanclock


## Before upload

  sudo chmod a+rw /dev/ttyUSB0

## Upload

    arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano:cpu=atmega328old morethanclock



# Very interesting

https://www.mikrocontroller.net/articles/Umstieg_von_Arduino_auf_AVR


# Time decoding

--+  +--------+    +------+-----------+
  |  |        |    |                  |
  +--+        +----+                  +--

  |t1|        |-t2-|
  |-----tg----|-----tg----|-----tg----|

tg = 1s
t1 = 0.1s -> LOW
t2 = 0.2s -> HIGH

If no falling edge for longer than tg -> start of new cycle

