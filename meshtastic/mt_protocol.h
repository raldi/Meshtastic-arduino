#ifndef MT_PROTOCOL_H
#define MT_PROTOCOL_H

#include "mt_nanopb.h"

// The buffer used for protobuf encoding/decoding. Since there's only one, and it's global, we
// have to make sure we're only ever doing one encoding or decoding at a time.
#define PB_BUFSIZE 512
extern pb_byte_t pb_buf[PB_BUFSIZE+4];
extern size_t pb_size; // Number of bytes currently in the buffer

extern uint32_t my_node_num; // Node number of the MT node hosting our WiFi

// Request a node report from our MT
bool mt_protocol_send_wantconfig();

// Check to see if we have a complete, valid packet waiting, and if so, process it
void mt_protocol_check_packet(uint32_t now);

#endif
