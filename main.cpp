// ----------------------------------------------------------------------------
// Copyright 2016-2018 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#include "simplem2mclient.h"
#ifdef TARGET_LIKE_MBED
#include "mbed.h"
#endif
#include "application_init.h"
#include "mcc_common_button_and_led.h"
#include "blinky.h"
#ifndef MBED_CONF_MBED_CLOUD_CLIENT_DISABLE_CERTIFICATE_ENROLLMENT
#include "certificate_enrollment_user_cb.h"
#endif

extern "C"
{
#include "ble_beacon.h"
}

// value range 0-MAX_CONNECTED_BEACONS
static uint8_t connected_beacons = 0;

// bitmap indicating valid beacons
// bit 0 corresponds to beacon_data_res_tbl[0] and so on
// note: must be under <= 63 bits in length
static uint16_t data_valid_bmp = 0x0u;


static void main_application(void);

int main(void)
{
    mcc_platform_run_program(main_application);
}

// Pointers to the resources that will be created in main_application().
static M2MResource* beacon_data_res_tbl[MAX_CONNECTED_BEACONS];
static M2MResource* pelion_data_valid_bmp;


// Pointer to mbedClient, used for calling close function.
static SimpleM2MClient *client;

// This function is called when a POST request is received for resource 5000/0/1.
void unregister(void *)
{
    printf("Unregister resource executed\n");
    client->close();
}

// This function is called when a POST request is received for resource 5000/0/2.
void factory_reset(void *)
{
    printf("Factory reset resource executed\n");
    client->close();
    kcm_status_e kcm_status = kcm_factory_reset();
    if (kcm_status != KCM_STATUS_SUCCESS) {
        printf("Failed to do factory reset - %d\n", kcm_status);
    } else {
        printf("Factory reset completed. Now restart the device\n");
    }
}

// sets new values for resources in Pelion based on client-side data
void update_beacon_cloud_data()
{
    uint32_t i = 0;
    uint32_t updated_count = 0;

    BEACON_DATA_T* data_tbl = get_beacon_tbl();
    BEACON_DATA_T* beacon;

    for (i = 0; i < MAX_CONNECTED_BEACONS; i++)
    {
        beacon = &(data_tbl[i]);

        if (beacon->element_used && beacon->updated)
        {
            beacon_data_res_tbl[i]->set_value_float(beacon->temp);
            beacon->updated = 0;
            printf("Beacon %lu temperature updated: %f C\n", i, beacon->temp);
            updated_count++;
        }
    }

    pelion_data_valid_bmp->set_value((int64_t)data_valid_bmp); // safe cast: bitmap shorter than 63 bits

    printf("Updated data from %lu devices sent to Pelion.\n", updated_count);
}

void main_application(void)
{
#if defined(__linux__) && (MBED_CONF_MBED_TRACE_ENABLE == 0)
        // make sure the line buffering is on as non-trace builds do
        // not produce enough output to fill the buffer
        setlinebuf(stdout);
#endif 

    // Initialize trace-library first
    if (application_init_mbed_trace() != 0) {
        printf("Failed initializing mbed trace\n" );
        return;
    }

    // Initialize storage
    if (mcc_platform_storage_init() != 0) {
        printf("Failed to initialize storage\n" );
        return;
    }

    // Initialize platform-specific components
    if(mcc_platform_init() != 0) {
        printf("ERROR - platform_init() failed!\n");
        return;
    }

    // Print platform information
    mcc_platform_sw_build_info();

    // Print some statistics of the object sizes and their heap memory consumption.
    // NOTE: This *must* be done before creating MbedCloudClient, as the statistic calculation
    // creates and deletes M2MSecurity and M2MDevice singleton objects, which are also used by
    // the MbedCloudClient.
#ifdef MBED_HEAP_STATS_ENABLED
    print_m2mobject_stats();
#endif

    // SimpleClient is used for registering and unregistering resources to a server.
    SimpleM2MClient mbedClient;

    // application_init() runs the following initializations:
    //  1. platform initialization
    //  2. print memory statistics if MBED_HEAP_STATS_ENABLED is defined
    //  3. FCC initialization.
    if (!application_init()) {
        printf("Initialization failed, exiting application!\n");
        return;
    }

    // Save pointer to mbedClient so that other functions can access it.
    client = &mbedClient;

#ifdef MBED_HEAP_STATS_ENABLED
    printf("Client initialized\r\n");
    print_heap_stats();
#endif
#ifdef MBED_STACK_STATS_ENABLED
    print_stack_statistics();
#endif


    // Create resource for unregistering the device. Path of this resource will be: 5000/0/1.
    mbedClient.add_cloud_resource(5000, 0, 1, "unregister", M2MResourceInstance::STRING,
                 M2MBase::POST_ALLOWED, NULL, false, (void*)unregister, NULL);

    // Create resource for running factory reset for the device. Path of this resource will be: 5000/0/2.
    mbedClient.add_cloud_resource(5000, 0, 2, "factory_reset", M2MResourceInstance::STRING,
                 M2MBase::POST_ALLOWED, NULL, false, (void*)factory_reset, NULL);

    init_beacon_tbl();

    uint16_t i;
    for (i = 0; i < MAX_CONNECTED_BEACONS; i++)
    {
        char res_name[32] = {0};
        M2MResource* res;

        sprintf(res_name, "beacon_%02x_temperature", i);
        res = mbedClient.add_cloud_resource(3303u, i, 5700u, res_name, 
                                        M2MResourceInstance::FLOAT, M2MBase::GET_PUT_ALLOWED, "", true, NULL, NULL);
        beacon_data_res_tbl[i] = res;
    }

    // TODO: check path, this was copied from blinking pattern resource
    pelion_data_valid_bmp = mbedClient.add_cloud_resource(3201, 0, 5853, "beacon_validity_bitmap", 
                                M2MResourceInstance::INTEGER, M2MBase::GET_PUT_ALLOWED, 0, true, NULL, NULL);

    mbedClient.register_and_connect();

#ifndef MBED_CONF_MBED_CLOUD_CLIENT_DISABLE_CERTIFICATE_ENROLLMENT
    // Add certificate renewal callback
    mbedClient.get_cloud_client().on_certificate_renewal(certificate_renewal_cb);
#endif // MBED_CONF_MBED_CLOUD_CLIENT_DISABLE_CERTIFICATE_ENROLLMENT


    uint8_t dummy_update_idx = 0;

    // Check if client is registering or registered, if true sleep and repeat.
    while (mbedClient.is_register_called())
    {
        if (1 /* BLE device connected */ && (connected_beacons < MAX_CONNECTED_BEACONS))
        {
            uint32_t tbl_idx = add_beacon();

            if (tbl_idx != INVALID_U32)
            {
                data_valid_bmp |= (0x1u << tbl_idx);
                connected_beacons++;
            }
        }
 
        dummy_update_beacon_data(dummy_update_idx);
        update_beacon_cloud_data();

        dummy_update_idx = (dummy_update_idx < (MAX_CONNECTED_BEACONS -1)) ? (dummy_update_idx + 1) : 0;
        mcc_platform_do_wait(5000);
    }

    // Client unregistered, exit program.
}
