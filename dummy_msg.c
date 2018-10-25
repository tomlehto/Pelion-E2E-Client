
#include <stdio.h>
#include <string.h>
#include "dummy_msg.h"

// TODO: example functionality
// we can probably just push all the data straight from BLE device to a string

void generate_dummy_msg(DUMMY_MSG_T* p_msg)
{
    printf("Generating dummy message parameters.\n");

    p_msg->device_data.updated         = 1u;
    p_msg->device_data.device_id       = FAKE_BLE_DEVICE_ID;
    p_msg->device_data.temp            = 0u;
    p_msg->device_data.signal_strength = 0xFFFFFFFFu;
    
    p_msg->timestamp = time(NULL);
    p_msg->lat       = 65.0593177; // uni oulu
    p_msg->lon       = 25.4662935;
}

void format_pelion_message(DUMMY_MSG_T* p_msg, char* p_out_msg)
{
    memset(p_out_msg, 0, MSG_LEN);
    sprintf(p_out_msg, "%llu: ID=0x%x t=%lu P=%lu lat=%.6f lon=%.6f",
        p_msg->timestamp,
        p_msg->device_data.device_id,
        p_msg->device_data.temp,
        p_msg->device_data.signal_strength,
        p_msg->lat,
        p_msg->lon);

    p_msg->device_data.updated = 0;
}
