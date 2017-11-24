# AssetBLOCK

Low power consumption Arduino asset tracker with OTA configuration capabilities.
Idea is to get a board without FTDI chip and other energy sucking components.

Uses libraries:
* https://github.com/appelflap/IridiumSBD
* http://arduiniana.org/libraries/tinygpsplus/
* http://arduiniana.org/libraries/pstring/
* https://www.pjrc.com/teensy/td_libs_Time.html

Built using hints/code from:
* https://makezine.com/projects/make-37/iridiumsatellite/
* http://www.kevindarrah.com/download/arduino_code/LowPowerVideo.ino

Hardware components:
* Arduino Nano Pro 3.3v
* RockBLOCK Iridium modem
* Adafruit Ultimate GPS Breakout
* 2x or 4x Long Life 3.7v batteries parallel for 12Ah life

Steps:
* Enable GPS
* Try to get FIX for n seconds
* Disable GPS
* If no fix -> sleep
* Else enable RockBLOCK
* Try to get Iridium network
* Receive message (if available in network)
* Send GPS Location in less than 50 bytes (1 credit)
* Apply configuration to EEPROM if found in received message
* Disable as much hardware
* Sleep for b seconds (1 hour by default at the moment, will be changed to 24 hours)
