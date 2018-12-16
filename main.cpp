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

/* Used to turn BLE feature on/off */
#define FEA_BLE 0
/* Used to turn some debug prints on/off */
#define DEBUG_PRINTS 0 
#define BLE_BEACON_TAG_OFFSET 10
#define BLE_BEACON_IDX_OFFSET 11
#define BLE_BEACON_TEMP_OFFSET 12

#if FEA_BLE
#include <events/mbed_events.h>
#include "ble/BLE.h"
#endif

extern "C"
{
#include "ble_beacon.h"
}

void update_beacon_cloud_data();

// value range 0-MAX_CONNECTED_BEACONS
static uint8_t connected_beacons = 0;

// bitmap indicating valid beacons
// bit 0 corresponds to beacon_data_res_tbl[0] and so on
// note: must be under <= 63 bits in length
static uint16_t data_valid_bmp = 0x0u;

#if FEA_BLE

static const uint8_t DEVICE_NAME[]        = "GAP_device";

/* Duration of each mode in milliseconds */
static const size_t MODE_DURATION_MS      = 6000;

/* Time between each mode in milliseconds */
static const size_t TIME_BETWEEN_MODES_MS = 5000;

static const size_t CONNECTION_DURATION = 3000;



typedef struct {
    GapAdvertisingParams::AdvertisingType_t adv_type;
    uint16_t interval;
    uint16_t timeout;
} AdvModeParam_t;

typedef struct {
    uint16_t interval;
    uint16_t window;
    uint16_t timeout;
    bool active;
} ScanModeParam_t;

static const ScanModeParam_t scanning_params = 
/* interval      window    timeout       active */
    { 500,       100,         5,         false };

/* parameters to use when attempting to connect to maximise speed of connection */
static const GapScanningParams connection_scan_params(
    GapScanningParams::SCAN_INTERVAL_MAX,
    GapScanningParams::SCAN_WINDOW_MAX,
    3,
    false
);

/* get number of items in our arrays */
static const size_t SCAN_PARAM_SET_MAX =
    sizeof(scanning_params) / sizeof(GapScanningParams);

static const char* to_string(Gap::Phy_t phy) {
    switch(phy.value()) {
        case Gap::Phy_t::LE_1M:
            return "LE 1M";
        case Gap::Phy_t::LE_2M:
            return "LE 2M";
        case Gap::Phy_t::LE_CODED:
            return "LE coded";
        default:
            return "invalid PHY";
    }
}

class GAPDevice : private mbed::NonCopyable<GAPDevice>, public Gap::EventHandler
{
public:
    GAPDevice() :
        _ble(BLE::Instance()),
        _led1(LED1, 0),
        _set_index(0),
        _is_in_scanning_mode(false),
        _on_duration_end_id(0),
        _scan_count(0) { };

    ~GAPDevice()
    {
        if (_ble.hasInitialized()) {
            _ble.shutdown();
        }
    };

    /** Start BLE interface initialisation */
    void run()
    {
        ble_error_t error;

        if (_ble.hasInitialized()) {
            printf("Ble instance already initialised.\r\n");
            return;
        }

        /* this will inform us off all events so we can schedule their handling
         * using our event queue */
        _ble.onEventsToProcess(
            makeFunctionPointer(this, &GAPDevice::schedule_ble_events)
        );

        /* handle timeouts, for example when connection attempts fail */
        _ble.gap().onTimeout(
            makeFunctionPointer(this, &GAPDevice::on_timeout)
        );

        /* handle gap events */
        _ble.gap().setEventHandler(this);

        error = _ble.init(this, &GAPDevice::on_init_complete);

        if (error) {
            printf("Error returned by BLE::init.\r\n");
            return;
        }

        /* to show we're running we'll blink every 500ms */
        _event_queue.call_every(500, this, &GAPDevice::blink);

        /* this will not return until shutdown */
        _event_queue.dispatch_forever();
    };
 

private:
    /** This is called when BLE interface is initialised and starts the first mode */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *event)
    {
        if (event->error) {
            printf("Error during the initialisation\r\n");
            return;
        }

        /* print device address */
        Gap::AddressType_t addr_type;
        Gap::Address_t addr;
        _ble.gap().getAddress(&addr_type, addr);
        printf("Device address: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
               addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

        /* setup the default phy used in connection to 2M to reduce power consumption */
        Gap::PhySet_t tx_phys(/* 1M */ false, /* 2M */ true, /* coded */ false);
        Gap::PhySet_t rx_phys(/* 1M */ false, /* 2M */ true, /* coded */ false);
        ble_error_t err = _ble.gap().setPreferredPhys(&tx_phys, &rx_phys);
        if (err) {
            printf("INFO: GAP::setPreferedPhys failed with error code %s", BLE::errorToString(err));
        }

        /* all calls are serialised on the user thread through the event queue */
        _event_queue.call(this, &GAPDevice::demo_mode_start);
    };

    /** queue up start of the current demo mode */
    void demo_mode_start()
    {
        _event_queue.call(this, &GAPDevice::scan);

        /* for performance measurement keep track of duration of the demo mode */
        _demo_duration.start();
        /* keep track of our state */

        /* queue up next demo mode */
        _on_duration_end_id = _event_queue.call_in(
            MODE_DURATION_MS, this, &GAPDevice::on_duration_end
        );

        printf("\r\n");
    }
    
    /** Set up and start scanning */
    void scan()
    {
        ble_error_t error;

        /* scanning happens repeatedly, interval is the number of milliseconds
         * between each cycle of scanning */
        uint16_t interval = scanning_params.interval;

        /* number of milliseconds we scan for each time we enter
         * the scanning cycle after the interval set above */
        uint16_t window = scanning_params.window;

        /* how long to repeat the cycles of scanning in seconds */
        uint16_t timeout = scanning_params.timeout;

        /* active scanning will send a scan request to any scanable devices that
         * we see advertising */
        bool active = scanning_params.active;

        /* set the scanning parameters according to currently selected set */
        error = _ble.gap().setScanParams(interval, window, timeout, active);

        if (error) {
            printf("Error during Gap::setScanParams\r\n");
            return;
        }

        /* start scanning and attach a callback that will handle advertisements
         * and scan requests responses */
        error = _ble.gap().startScan(this, &GAPDevice::on_scan);

        if (error) {
            printf("Error during Gap::startScan\r\n");
            return;
        }
        #if DEBUG_PRINTS
        printf("Scanning started (interval: %dms, window: %dms, timeout: %ds).\r\n",
               interval, window, timeout);
        #endif
    };
    

    /** After a set duration this cycles to the next demo mode
     *  unless a connection happened first */
    void on_duration_end()
    {
        /* alloted time has elapsed, move to next demo mode */
        _event_queue.call(this, &GAPDevice::demo_mode_end);
    };

    /** Parse scan payload */
    void on_scan(const Gap::AdvertisementCallbackParams_t *params)
    {
        /* Tag */
        uint8_t beacon_tag;
        /* Beacon index */
        uint8_t beacon_idx;
        /* Temperature value */
        uint8_t beacon_temp;
        /* keep track of scan events for performance reporting */
        _scan_count++;

        #if DEBUG_PRINTS
        for (uint8_t i = 0; i < params->advertisingDataLen; ++i) {
            printf("%02x ", params->advertisingData[i]);
        }
        printf("\r\n");
        #endif

        if (params->advertisingDataLen > BLE_BEACON_TEMP_OFFSET + 1)
        {
            beacon_tag = params->advertisingData[BLE_BEACON_TAG_OFFSET];
            /* Check that beacon has our tag */
            if (0xAF == beacon_tag)
            {
                beacon_idx = params->advertisingData[BLE_BEACON_IDX_OFFSET];
                beacon_temp = params->advertisingData[BLE_BEACON_TEMP_OFFSET];
                #if DEBUG_PRINTS
                printf("Beacon index = %d, Beacon temp = %d\n", beacon_idx, beacon_temp);
                #endif
                if (1 /* BLE device connected */ && (connected_beacons < MAX_CONNECTED_BEACONS))
                {
                    uint32_t result = add_beacon(beacon_idx);

                    if (result != INVALID_U32)
                    {
                        data_valid_bmp |= (0x1u << beacon_idx);
                        connected_beacons++;
                    }
                } 
                update_beacon_data(beacon_idx, beacon_temp);
            }
        }
    };

    /**
     * Implementation of Gap::EventHandler::onReadPhy
     */
    virtual void onReadPhy(
        ble_error_t error,
        Gap::Handle_t connectionHandle,
        Gap::Phy_t txPhy,
        Gap::Phy_t rxPhy
    ) {
        if (error) {
            printf(
                "Phy read on connection %d failed with error code %s\r\n",
                connectionHandle,
                BLE::errorToString(error)
            );
        } else {
            printf(
                "Phy read on connection %d - Tx Phy: %s, Rx Phy: %s\r\n",
                connectionHandle,
                to_string(txPhy),
                to_string(rxPhy)
            );
        }
    }

    /**
     * Implementation of Gap::EventHandler::onPhyUpdateComplete
     */
    virtual void onPhyUpdateComplete(
        ble_error_t error,
        Gap::Handle_t connectionHandle,
        Gap::Phy_t txPhy,
        Gap::Phy_t rxPhy
    ) {
        if (error) {
            printf(
                "Phy update on connection: %d failed with error code %s\r\n",
                connectionHandle,
                BLE::errorToString(error)
            );
        } else {
            printf(
                "Phy update on connection %d - Tx Phy: %s, Rx Phy: %s\r\n",
                connectionHandle,
                to_string(txPhy),
                to_string(rxPhy)
            );
        }
    }

    /** called if timeout is reached during advertising, scanning
     *  or connection initiation */
    void on_timeout(const Gap::TimeoutSource_t source)
    {
        _demo_duration.stop();

        switch (source) {
            case Gap::TIMEOUT_SRC_ADVERTISING:
                printf("Stopped advertising early due to timeout parameter\r\n");
                break;
            case Gap::TIMEOUT_SRC_SCAN:
                printf("Stopped scanning early due to timeout parameter\r\n");
                break;
            case Gap::TIMEOUT_SRC_CONN:
                printf("Failed to connect after scanning %d advertisements\r\n", _scan_count);
                _event_queue.call(this, &GAPDevice::demo_mode_end);
                break;
            default:
                printf("Unexpected timeout\r\n");
                break;
        }
    };

    /** clean up after last run, cycle to the next mode and launch it */
    void demo_mode_end()
    {
        /* reset the demo ready for the next mode */
        _scan_count = 0;
        _demo_duration.stop();
        _demo_duration.reset();

        /* cycle through all demo modes */
        _set_index++;

        _ble.shutdown();
        _event_queue.break_dispatch();
    };

    /** Schedule processing of events from the BLE middleware in the event queue. */
    void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context)
    {
        _event_queue.call(mbed::callback(&context->ble, &BLE::processEvents));
    };

    /** Blink LED to show we're running */
    void blink(void)
    {
        _led1 = !_led1;
    };

private:
    BLE                &_ble;
    events::EventQueue  _event_queue;
    DigitalOut          _led1;

    /* Keep track of our progress through demo modes */
    size_t              _set_index;
    bool                _is_in_scanning_mode;
    /* Remember the call id of the function on _event_queue
     * so we can cancel it if we need to end the mode early */
    int                 _on_duration_end_id;

    /* Measure performance of our advertising/scanning */
    Timer               _demo_duration;
    size_t              _scan_count;
};
#endif /* FEA_BLE */



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
    #if FEA_BLE
    GAPDevice gap_device;
    #endif

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

    #if !FEA_BLE
    uint8_t dummy_update_idx = 0;
    #endif
    // Check if client is registering or registered, if true sleep and repeat.
    while (mbedClient.is_register_called())
    {
        #if FEA_BLE
        /* Run BLE scan procedure.
        This also updates the beacon data tables */
        gap_device.run();
        #else
        /* Dummy version */
        if (connected_beacons < MAX_CONNECTED_BEACONS)
        {
            uint32_t tbl_idx = add_dummy_beacon();

            if (tbl_idx != INVALID_U32)
            {
                data_valid_bmp |= (0x1u << tbl_idx);
                connected_beacons++;
            }
        }
        dummy_update_beacon_data(dummy_update_idx);
        dummy_update_idx = (dummy_update_idx < (MAX_CONNECTED_BEACONS -1)) ? (dummy_update_idx + 1) : 0;
        #endif
        /* Update scanned/dummy BLE data to Pelion cloud */
        update_beacon_cloud_data();
        /* Sleep for 10s */
        mcc_platform_do_wait(10000);   
    }
    // Client unregistered, exit program.
}
