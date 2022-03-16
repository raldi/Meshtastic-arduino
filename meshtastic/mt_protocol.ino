#include "mt_protocol.h"

// Magic number at the start of all MT packets
#define MT_MAGIC_0 0x94
#define MT_MAGIC_1 0xc3

// The header is the magic number plus a 16-bit payload-length field
#define MT_HEADER_SIZE 4

// The buffer used for protobuf encoding/decoding. Since there's only one, and it's global, we
// have to make sure we're only ever doing one encoding or decoding at a time.
pb_byte_t pb_buf[PB_BUFSIZE+4];
size_t pb_size = 0; // Number of bytes currently in the buffer

// Some sane limits on a few strings that the protocol would otherwise allow to be unlimited length
#define MAX_USER_ID_LEN 32
#define MAX_LONG_NAME_LEN 32
#define MAX_SHORT_NAME_LEN 8
#define MAX_MACADDR_LEN 32

// The ID of the current WANT_CONFIG request
uint32_t want_config_id = 0;

// Node number of the MT node hosting our WiFi
uint32_t my_node_num = 0;

// Request a node report from our MT
bool mt_protocol_send_wantconfig() {
  ToRadio toRadio = ToRadio_init_default;
  toRadio.which_payloadVariant = ToRadio_want_config_id_tag;
  want_config_id = random(0x7FffFFff);  // random() can't handle anything bigger
  toRadio.payloadVariant.want_config_id = want_config_id;

  pb_buf[0] = MT_MAGIC_0;
  pb_buf[1] = MT_MAGIC_1;

  pb_ostream_t stream = pb_ostream_from_buffer(pb_buf + 4, PB_BUFSIZE);
  bool status = pb_encode(&stream, ToRadio_fields, &toRadio);
  if (!status) {
    Serial.println("Couldn't encode wantconfig");
    return false;
  }

  Serial.print("Requesting node report with random ID ");
  Serial.println(want_config_id);

  // Store the payload length in the header
  pb_buf[2] = stream.bytes_written / 256;
  pb_buf[3] = stream.bytes_written % 256;

  bool rv = mt_wifi_send_radio((const char *)pb_buf, 4 + stream.bytes_written);

  // Clear the buffer so it can be used to hold reply packets
  pb_size = 0;
  return rv;
}

bool handle_my_info(MyNodeInfo *myNodeInfo) {
  my_node_num = myNodeInfo->my_node_num;
  Serial.print("Looks like my node number is ");
  Serial.println(my_node_num);
  return true;
}

bool handle_node_info(NodeInfo *nodeInfo, pb_byte_t * user_id, pb_byte_t * long_name,
                      pb_byte_t * short_name, pb_byte_t * macaddr) {
  Serial.print("The node at ");
  Serial.print(nodeInfo->num);
  if (nodeInfo->num == my_node_num) {
    Serial.print(" (that's me!)");
  }
  Serial.print(", last reached at time=");
  Serial.print(nodeInfo->last_heard);

  if (nodeInfo->has_user) {
    Serial.print(", belongs to '");
    Serial.print((char *)long_name);
    Serial.print("' (a.k.a. '");
    Serial.print((char *)short_name);
    Serial.print("' or '");
    Serial.print((char *)user_id);
    Serial.print("' at '");
    Serial.print((char *)macaddr);
    Serial.print("') ");
  } else {
    Serial.print(", is anonymous ");
  }

  if (nodeInfo->has_position) {
    Serial.print("and is at ");
    Serial.print(nodeInfo->position.latitude_i / 1e7);
    Serial.print(", ");
    Serial.print(nodeInfo->position.longitude_i / 1e7);
    Serial.print("; ");
    Serial.print(nodeInfo->position.altitude);
    Serial.print("m above sea level moving at ");
    Serial.print(nodeInfo->position.ground_speed);
    Serial.print(" m/s as of time=");
    Serial.print(nodeInfo->position.pos_timestamp);
    Serial.print(" and their battery is at ");
    Serial.print(nodeInfo->position.battery_level);
    Serial.print(" and they told our node at time=");
    Serial.println(nodeInfo->position.time);
  } else {
    Serial.println(" has no position");
  }

  return true;
}

bool handle_config_complete_id(uint32_t now, uint32_t config_complete_id) {
  if (config_complete_id == want_config_id) {
    Serial.println("And that's all the all nodes");
    mt_wifi_reset_idle_timeout(now);
    want_config_id = 0;
  } else {
    Serial.println("Disregard all of the above; it was an answer to someone else's question");  // Return true anyway
  }
  return true;
}

// The nanopb library we're using to decode protobufs requires callback functions to handle
// arbitrary-length strings. So here they all are:
bool decode_string(pb_istream_t *stream, const pb_field_t *field, void ** arg, uint8_t len) {
  pb_byte_t *buf = *(pb_byte_t **)arg;
  if (stream->bytes_left < len) len = stream->bytes_left;
  return pb_read(stream, buf, len);
}

bool decode_user_id (pb_istream_t *stream, const pb_field_t *field, void ** arg) {
  return decode_string(stream, field, arg, MAX_USER_ID_LEN);
}

bool decode_long_name (pb_istream_t *stream, const pb_field_t *field, void ** arg) {
  return decode_string(stream, field, arg, MAX_LONG_NAME_LEN);
}

bool decode_short_name (pb_istream_t *stream, const pb_field_t *field, void ** arg) {
  return decode_string(stream, field, arg, MAX_SHORT_NAME_LEN);
}

bool decode_macaddr (pb_istream_t *stream, const pb_field_t *field, void ** arg) {
  return decode_string(stream, field, arg, MAX_MACADDR_LEN);
}

// Parse a packet that came in, and handle it. Return true iff we were able to parse it.
bool handle_packet(uint32_t now, size_t payload_len) {
  FromRadio fromRadio = FromRadio_init_zero;
  pb_byte_t user_id[MAX_USER_ID_LEN+1] = {0};
  pb_byte_t long_name[MAX_LONG_NAME_LEN+1] = {0};
  pb_byte_t short_name[MAX_SHORT_NAME_LEN+1] = {0};
  pb_byte_t macaddr[MAX_MACADDR_LEN+1] = {0};

  // Set up some callback functions
  fromRadio.payloadVariant.node_info.user.id.funcs.decode = &decode_user_id;
  fromRadio.payloadVariant.node_info.user.id.arg = &user_id;
  fromRadio.payloadVariant.node_info.user.long_name.funcs.decode = &decode_long_name;
  fromRadio.payloadVariant.node_info.user.long_name.arg = &long_name;
  fromRadio.payloadVariant.node_info.user.short_name.funcs.decode = &decode_short_name;
  fromRadio.payloadVariant.node_info.user.short_name.arg = &short_name;
  fromRadio.payloadVariant.node_info.user.macaddr.funcs.decode = &decode_macaddr;
  fromRadio.payloadVariant.node_info.user.macaddr.arg = &macaddr;

  // Decode the protobuf and shift forward any remaining bytes in the buffer (which, if
  // present, belong to the packet that we're going to process on the next loop)
  pb_istream_t stream;
  stream = pb_istream_from_buffer(pb_buf + 4, payload_len);
  bool status = pb_decode(&stream, FromRadio_fields, &fromRadio);
  memmove(pb_buf, pb_buf+4+payload_len, PB_BUFSIZE-4-payload_len);
  pb_size -= 4 + payload_len;

  if (!status) {
    Serial.println("Decoding failed");
    return false;
  }

  switch (fromRadio.which_payloadVariant) {
    case FromRadio_my_info_tag:
      return handle_my_info(&fromRadio.payloadVariant.my_info);
    case FromRadio_node_info_tag:
      return handle_node_info(&fromRadio.payloadVariant.node_info, user_id, long_name, short_name, macaddr);
    case FromRadio_config_complete_id_tag:
      return handle_config_complete_id(now, fromRadio.payloadVariant.config_complete_id);
    case FromRadio_packet_tag:
      return false;  // A packet was sent over the network. Could be anything! This would be a good place
                     // to expand this library's functionality in the future, adding support for new kinds
                     // of packet as needed. See
                     // https://github.com/meshtastic/Meshtastic-protobufs/blob/3bd1aec912d4bc1f4d9c42f6c60c766ed281d801/mesh.proto#L721-L922
                     // (or the latest version of that file) for all the fields you'll need to implement.
    default:
      Serial.print("Got a payloadVariant we don't recognize: ");
      Serial.println(fromRadio.which_payloadVariant);
      return false;
  }

  Serial.println("Handled a packet");
}


void mt_protocol_check_packet(uint32_t now) {
  if (pb_size < MT_HEADER_SIZE) return;  // We don't even have a header yet

  if (pb_buf[0] != MT_MAGIC_0 || pb_buf[1] != MT_MAGIC_1) {
    Serial.println("Got bad magic");
    return;
  }

  uint16_t payload_len = pb_buf[2] << 8 | pb_buf[3];
  if (payload_len > PB_BUFSIZE) {
    Serial.println("Got packet claiming to be ridiculous length");
    return;
  }

  if (payload_len + 4 > pb_size) {
    // Serial.println("Partial packet");
    return;
  }

  /*
  Serial.print("Got a full packet! ");
  for (int i = 0 ; i < pb_size ; i++) {
    Serial.print(pb_buf[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  */

  handle_packet(now, payload_len);
}
