TARGET=ESP32S3
PORT=/dev/ttyACM1
#ESP32_FILESYSTEM=littlefs
ESP32_PSRAM=opi
ESP32_FILESYSTEM=spiffs
ESP32_FILESYSTEM_PART=spiffs
ESP32_PARTSCHEME=min_spiffs
ESP32_FLASHSIZE=16MB
ESP32S3_CDCONBOOT=cdc
ESP32_DEBUGLEVEL=verbose
GITHUB_REPOS= \
reeltwo/Reeltwo \

ifeq ("$(TOUCH)","1")
ARDUINO_OPTS+='-prefs="compiler.cpp.extra_flags=-DST7789V_DRIVER=1"'
endif

include ../Arduino.mk
