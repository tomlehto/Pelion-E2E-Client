/*
    Receive message from "virtual" BLE device
    Create message to be sent to Web App (using Pelion Cloud)
    Sent simulated messages in certain interval
    Randomize BLE device data (e.g. I can see 0-3 devices with different signal strengths)

BLE device message parameters:

    Temperature

    Device ID

Final message to be sent:

    BLE device temp

    BLE device deviceID

    BLE device signal strength

    BLE Node's location (BLE Node = device talking to Pelion Cloud)

    Timestamp

*/

#include <inttypes.h>
#include <time.h>

#define MSG_LEN (256)
#define FAKE_BLE_DEVICE_ID (0xFFFFu)

typedef struct
{
    uint16_t device_id;       // BLE device ID, TODO: format?
    uint8_t  updated;         // 1: y, 0: n 
    float    temp;            // temperature TODO: format?
    uint32_t signal_strength; // TODO: format?
} FAKE_BLE_DEVICE_T;

typedef struct
{
    uint32_t msg_id; // seconds from epoch

    FAKE_BLE_DEVICE_T device_data;

    // coordinates of device (K64F) sending messages to Pelion
    float lat;
    float lon;
} DUMMY_MSG_T;

void generate_dummy_msg(DUMMY_MSG_T* msg);
void format_pelion_message(DUMMY_MSG_T* p_msg, char* p_out_msg);
