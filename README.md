# Pelion-E2E-Client

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
Alternatively, create WiFi hotspot on your phone with SSID: "SSID" and password: "Password" :)
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
