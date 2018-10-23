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
