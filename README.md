# 5counter

Tiny counter from 0 to 4 build on top of flashlight with 2 leds.
Uses ATtiny13A as MCU.

Project 3 IO pins:

* for red _signal_ led
* for white _flashlight_ led
* for button

Both leds are connected through PNP transistors, so their outputs are inverse (0 = on, 1 = off).
