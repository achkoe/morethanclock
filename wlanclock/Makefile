# Makefile for wlanclock

src = wlanclock

all: compile upload

compile:
	cd .. && arduino-cli compile --warnings all --fqbn esp8266:esp8266:d1_mini $(src)

upload:
	cd .. && arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:d1_mini $(src)