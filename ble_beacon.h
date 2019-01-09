
#include <inttypes.h>
#include <time.h>

#define MAX_CONNECTED_BEACONS (10)
#define INVALID_U32           (0xFFFFFFFFu)

typedef struct
{
    uint8_t element_used;  // 0: free, 1: used
    uint8_t updated;       // range 0,1
    time_t update_time;    // remove device when X amount of time with no updated?
    uint32_t rstp;         // received TX power from beacon device TODO: format? dBm in sX.X FXP??
    float lat;             // latitude coordinate
    float lon;             // longitude coordinate
    float temp;            // temperature in degrees celsius? TODO
} BEACON_DATA_T;


BEACON_DATA_T* get_beacon_tbl();
uint32_t add_dummy_beacon();
uint32_t add_beacon(uint8_t i);
void init_beacon_tbl();
void dummy_update_beacon_data(uint8_t index);
void update_beacon_data(uint8_t index, uint8_t temp);
void delete_beacon(uint8_t tbl_idx);
