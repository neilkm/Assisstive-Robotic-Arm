Useful commands:

#monitor serial port
pio device monitor -p /dev/ttyUSB0 -b 115200 --filter time

#build clean upload
pio run 
pio run -t clean
pio run -t upload

#build and upload from the repository root
Software/scripts/build_and_flash_esp32_app.sh
