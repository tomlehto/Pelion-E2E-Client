
####################
# UNIT TESTS
####################

set(unittest-includes ${unittest-includes}
  ../ble_beacon
)

set(unittest-sources
  ../ble_beacon/ble_beacon.c
)

set(unittest-test-sources
  ble_beacon/test_ble_beacon.cpp
)