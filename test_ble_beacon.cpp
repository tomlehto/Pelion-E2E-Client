#include "gtest/gtest.h"
extern "C"
{
#include "ble_beacon.h"
}
#include <stdio.h>
#include <string.h>

class TestBleBeacon : public testing::Test {
    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

TEST_F(TestBleBeacon, ble_beacon_test)
{
    uint32_t res;
    uint8_t i;
    BEACON_DATA_T *p_beacon_table;

    // test init_beacon_tbl() and get_beacon_tbl()
    init_beacon_tbl();
    p_beacon_table = get_beacon_tbl();

    EXPECT_NE((void *) 0, p_beacon_table);

    //test add_beacon(uint8_t i)
    for(i = 0; i < MAX_CONNECTED_BEACONS; i++)
    {
        EXPECT_EQ(0,add_beacon(i));
    }
    EXPECT_EQ(INVALID_U32, add_beacon(MAX_CONNECTED_BEACONS+1));

    //test delete_beacon(uint8_t tbl_idx)
    delete_beacon(0);
    EXPECT_EQ(0,p_beacon_table[0].element_used);
    EXPECT_EQ(0,p_beacon_table[0].updated);
    EXPECT_EQ(0,p_beacon_table[0].update_time);
    EXPECT_EQ(0,p_beacon_table[0].rstp);
    EXPECT_EQ(0,p_beacon_table[0].lat);
    EXPECT_EQ(0,p_beacon_table[0].lon);
    EXPECT_EQ(0,p_beacon_table[0].temp);


    //test update_beacon_data()
    update_beacon_data(1, 15);
    EXPECT_FLOAT_EQ(15, p_beacon_table[1].temp);
    
}
