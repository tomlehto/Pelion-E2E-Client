# Changelog for Pelion Device Management Client reference example application

## Release 2.0.1.1 (16.10.2018)
* No changes.

## Release 2.0.1 (12.10.2018)
* Application uses `wait_ms(int)` instead of `wait(float)`, this saves a bit on application size.
* [Mbed OS] Application will print NetworkInterface status over console.
* Updated to Mbed OS 5.10.1.

## Release 2.0.0 (26.09.2018)
* Updated to Mbed OS 5.10.0.
* **Breaking changes** (Due to integration of storage and networking drivers to Mbed OS 5.10 and the introduction of new APIs, the application is not compatible with previous releases of Mbed OS).
    * Changed the application to use Mbed OS 5.10 network interfaces directly and removed the dependency to `easy-connect.lib`.
    * Changed the application to use Mbed OS 5.10 storage interfaces directly and removed the dependency to `storage-selector.lib`.
    * The example uses Mbed OS bootloader binaries and the new Mbed OS feature `FEATURE_BOOTLOADER`. This feature makes the `combine_bootloader_with_app.py` script obsolete. We have removed the obsolete script and old bootloader binaries from the application repository.
* The bootloader is now automatically combined with the application binary at compile time:
        * `mbed-cloud-client-example.bin` is the binary you need to flash on the development board.
        * You can use `mbed-cloud-client-example_update.bin` for the firmware update as long as prequisites for firmware update has been fullfilled. (See the application tutorial).
* Removed the legacy configuration file `configs/eth_v4_legacy.json` that was used for Mbed Cloud Client 1.2.6.
* Removed the `.autostart` configurations used by Online Compiler.
* Enabled serial buffer in all `.json` files.
    * This improves the reliability of the connection with tracing enabled, for example Nucleo F429ZI had connectivity problems with UDP & traces enabled.
* Increased LWIP buffers for all STM-based targets to improve the stability of DTLS connection.
* Disabled Hardware acceleration for Ublox EVK Odin W2 and Nucleo F411RE. See Mbed OS [issue](https://github.com/ARMmbed/mbed-os/issues/6545).
* [LINUX] Updated Mbed TLS to 2.13.1.

## Release 1.5.0 (11.9.2018)
* Added a hardcoded RoT injection when application is configured to use developer mode. This preserves the Device Management Client credentials even when SOTP is erased (for example due to reflashing of the application binary).
* Updated to Mbed OS 5.9.6.
* Updated easy-connect to v1.2.16.
* Updated the storage selector with compiler warning fixes in the internal libraries.
* Replaced the notification delivery status functionality with a more generic message delivery status callback.
* Added an example on using the delayed response for execute operations.
* Added configurations for the K66F target board.


## Release 1.4.0 (13.07.2018)
* Increased application main stack-size to 5120 to fix Stack overflow with ARMCC compiled binaries when tracing is enabled.
* Linux: Updated Mbedtls to 2.10.0 in pal-platform.
* Updated to Mbed OS 5.9.2.
* Updated easy-connect to v1.2.12.
* Updated storage-selector with support for SPI flash frequency, and Nucleo F411RE board.
* Moved the initialization of `mbed_trace()` as the first step in the application initialization. This allows the printing of any early debug information from the PAL layer when debug tracing is enabled.
* Added support for Nucleo F411RE board with Wifi-X-Nucleo shield.
* Increased the event loop thread stack size to 8192 bytes from a default value of 6144. This fixes stack overflows in some cases where crypto operations result in deep callstacks.

## Release 1.3.3 (08.06.2018)
* Updated to Mbed OS 5.8.5.
* The example application prints information about the validity of the stored Cloud credentials (including Connect and Update certificates).
* Added a start-up delay of two seconds as a workaround for the SD driver initialization [issue](https://github.com/ARMmbed/sd-driver/issues/93).
* Fixed the heap corruption that took place with `print_m2mobject_stats()` when building the code with the MBED_HEAP_STATS_ENABLED flag.
* Added the `-DMBED_STACK_STATS_ENABLED` flag. It enables printing information on the application thread stack usage.

## Release 1.3.2 (22.05.2018)
* Updated easy-connect to v1.2.9.
* Updated to Mbed OS 5.8.4.
* Added the `partition_mode` configuration. It is enabled by default and supposed to be used with a data storage, such as an SD card.
* Linux: Updated Mbedtls to 2.7.1 in pal-platform.
* Linux: Fixed CMake generation and performed generic cleanup in pal-platform scripts.

#### Platform Adaptation Layer (PAL)
* Linux: Converted all timers to use signal-based timer (`SIGEV_SIGNAL`) instead of (`SIGEV_THREAD`).
  * This fixes the Valgrind warnings for possible memory leaks caused by LIBC's internal timer helper thread.
    <span class="notes">**Note**: If the client application is creating a pthread before instantiating MbedCloudClient,
    it needs to block the PAL_TIMER_SIGNAL from it. Otherwise the thread may get an exception caused
    by the default signal handler with a message such as "Process terminating with default action
    of signal 34 (`SIGRT2`)". For a suggested way to handle this please see `mcc_platform_init()` in [here](https://github.com/ARMmbed/mbed-cloud-client-example/blob/master/source/platform/Linux/common_setup.c).</span>

## Release 1.3.1.1 (27.04.2018)
* No changes.

## Release 1.3.1 (19.04.2018)
* Converted LED blinking callback from a blocking loop to event based tasklet.
* Updated to Mbed OS 5.8.1.
* Rewrote platform-specific code to have common implementation which can be shared between other Cloud applications (source/platform/).
  * enabled multipartition support for application.
  * enabled LittleFS support.
  * enabled autoformat/autopartition for storage (controllable via compile-time flags).

## Release 1.3.0 (27.3.2018)

Initial public release.
