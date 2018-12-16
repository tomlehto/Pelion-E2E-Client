
#include <stdio.h>
#include <string.h>
#include "ble_beacon.h"


static BEACON_DATA_T beacon_tbl[MAX_CONNECTED_BEACONS];


void init_beacon_tbl()
{
    memset(beacon_tbl, 0, sizeof(beacon_tbl));
}

// add dummy beacon device to table, return tbl index if ok, else INVALID_U32
uint32_t add_dummy_beacon( /* TODO: parameters needed when actual beacon connected */ )
{
    uint32_t i;

    for(i = 0; i < MAX_CONNECTED_BEACONS; i++)
    {
        if(beacon_tbl[i].element_used == 0)
            break;
    }

    if(i == MAX_CONNECTED_BEACONS)
    {
        printf("Beacon device adding failed: maximum number of devices already connected\n");
        return INVALID_U32;
    }

    beacon_tbl[i].element_used = 1u;
    beacon_tbl[i].updated      = 1u;

    // TODO: real data
    beacon_tbl[i].rstp      = INVALID_U32;
    beacon_tbl[i].lat       = 65.0593177; // uni oulu
    beacon_tbl[i].lon       = 25.4662935;
    beacon_tbl[i].temp      = 23.0;

    return i;
}

// add beacon device to table, return tbl index if ok, else INVALID_U32
uint32_t add_beacon(uint8_t i)
{
    //uint32_t i;
    /*
    for(i = 0; i < MAX_CONNECTED_BEACONS; i++)
    {
        if(beacon_tbl[i].element_used == 0)
            break;
    }
    */
    /* Check if beacon is already added */
    if(beacon_tbl[i].element_used)
    {
        return 0;
    }
    if(i >= MAX_CONNECTED_BEACONS)
    {
        printf("Beacon device adding failed: maximum number of devices already connected\n");
        return INVALID_U32;
    }

    beacon_tbl[i].element_used = 1u;
    beacon_tbl[i].updated      = 1u;

    // TODO: real data
    beacon_tbl[i].rstp      = INVALID_U32;
    beacon_tbl[i].lat       = 65.0593177; // uni oulu
    beacon_tbl[i].lon       = 25.4662935;
    beacon_tbl[i].temp      = .0;

    return 0;
}

void delete_beacon(uint8_t tbl_idx)
{
    printf("Deleting beacon %u\n", tbl_idx);
    memset(&(beacon_tbl[tbl_idx]), 0, sizeof(BEACON_DATA_T));
}

BEACON_DATA_T* get_beacon_tbl()
{
    return beacon_tbl;
}

void dummy_update_beacon_data(uint8_t index)
{
    if(beacon_tbl[index].element_used)
    {
        beacon_tbl[index].temp        = (beacon_tbl[index].temp < 50) ? (beacon_tbl[index].temp + 1.0) : 23.0;
        beacon_tbl[index].updated     = 1;
        beacon_tbl[index].update_time = time(NULL);
    }
    else
    {
        printf("dummy_update_beacon_data: Invalid dummy device index!\n");
    }
}

void update_beacon_data(uint8_t index, uint8_t temp)
{
    if(beacon_tbl[index].element_used)
    {
        beacon_tbl[index].temp        = (float) temp;
        beacon_tbl[index].updated     = 1;
        beacon_tbl[index].update_time = time(NULL);
    }
    else
    {
        printf("update_beacon_data: Invalid device index %d!\n", index);
    }
}

