
#include <stdio.h>
#include <string.h>
#include "dummy_msg.h"

// TODO: example functionality
// we can probably just push all the data straight from BLE device to a string

void generate_dummy_msg(DUMMY_MSG_T* p_msg)
{
    printf("Generating dummy message parameters.\n");

    static float temp = 0.0;
    static uint32_t msg_id;

    p_msg->device_data.updated         = 1u;
    p_msg->device_data.device_id       = FAKE_BLE_DEVICE_ID;
    p_msg->device_data.temp            = temp;
    p_msg->device_data.signal_strength = 0xFFFFFFFFu;
    
    p_msg->msg_id = msg_id;
    p_msg->lat    = 65.0593177; // uni oulu
    p_msg->lon    = 25.4662935;

    msg_id++;
    temp = (temp < 50.0) ? (temp+1.0) : 0.0;
}

void format_pelion_message(DUMMY_MSG_T* p_msg, char* p_out_msg)
{
    memset(p_out_msg, 0, MSG_LEN);
    sprintf(p_out_msg, "%llu: ID=0x%x t=%.2f P=%lu lat=%.6f lon=%.6f",
        p_msg->msg_id,
        p_msg->device_data.device_id,
        p_msg->device_data.temp,
        p_msg->device_data.signal_strength,
        p_msg->lat,
        p_msg->lon);

    p_msg->device_data.updated = 0;
}
