#ifndef _STRUCTURES_H_
#define _STRUCTURES_H_

// (C) 2023 OE1KBC Kurt Baumann, OE1KFR Rainer 
// (C) 2016, 2017, 2018, 2018, 2019, 2020 OE1KBC Kurt Baumann
//
// This code is not for distribution and may not be published!
// Vervielfältigung und Veröffentlichung des Codes sind nicht gestattet!
// Lizenz: kein Open Source.
// zugelassen ausschließlich für OE1KBC, OE1KFR
// alle anderen Finger weg !
// 20230326: Version 4.00: START

#include <Arduino.h>

// APRS protokol

struct aprsMessage
{
    uint8_t msg_len;
    char payload_type;
    unsigned int msg_id;
    uint8_t max_hop;
    bool  msg_server;
    String msg_source_path;
    String msg_destination_path;
    String msg_payload;
    unsigned int msg_fcs;
    bool msg_4_0;
    unsigned int msg_source_id;     // only for compatibility 2.0
    uint8_t msg_source_hw;     // only for compatibility 2.0
};

struct aprsPosition
{
    double lat;
    char lat_c;
    double lon;
    char lon_c;
    int alt;
};

#endif // _APRS_FUNCTIONS_H_
