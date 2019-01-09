# Pelion-E2E-Client

## Hardware:
* FRDM K64F or similar development board
* ST X-NUCLEO-IDB05A1 or similar bluetooth low energy extension board
* ESP8266-based Grove UART-WiFi adapter

This project should work out of the box with this hardware setup. Minor changes may be needed if other hardware is used.
## Prerequisites:
* mbed-cli installed
* GCC_ARM toolchain installed
## Initial setup:
```bash
git clone https://github.com/tomlehto/Pelion-E2E-Client.git
cd Pelion-E2E-Client
mbed deploy
mbed new .
mbed config -G CLOUD_SDK_API_KEY <API_KEY>
mbed target "K64F"
mbed toolchain GCC_ARM
mbed device-management init -d arm.com --model-name example-app --force -q
mbed compile
```
## Compile for ethernet
```bash
mbed compile
```
## Compile for WiFi
Modify configs/wifi_esp8266_v4.json:
```json
"nsapi.default-wifi-ssid"           : "\"SSID\"", #<-- SSID here
"nsapi.default-wifi-password"       : "\"Password\"" #<-- Password here
```
Alternatively, create WiFi hotspot with SSID: "SSID" and password: "Password" :)
```bash
mbed compile --app-config configs/wifi_esp8266_v4.json
```
## Add BLE feature
Modify main.cpp:
```c
#define FEA_BLE 1
```
Modify shields/TARGET_CORDIO_BLUENRG/bluenrg_targets.h
```c
#define BLUENRG_PIN_SPI_SCK (D13) // Pin D3 has to be changed to D13 for Arduino shield pinout compatibility
```
