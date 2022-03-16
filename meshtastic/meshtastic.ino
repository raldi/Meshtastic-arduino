// Meshtastic Arduino "Hello World"
// by Mike Schiraldi
//
// Get an Adafruit Feather M0 WiFi (or some other board, but then edit the pin config in mt_wifi.ino)
// and put your Meshtastic node in WiFi AP mode, then put your SSID and wifi password in arduino_secrets.h
// and run this code with the serial monitor open. Your arduino should connect to the node, request information
// on it and all other nodes it knows about, and print a report of what it finds.
#include <SnappyProto.h>
#include "mt_protocol.h"
#include "mt_wifi.h"

// Request a node report every this many msec
#define NODE_REPORT_PERIOD (30 * 1000)

uint32_t next_node_report_time = 0;

void setup() {
  // Try for up to five seconds to find a serial port; if not, the show must go on
  Serial.begin(9600);
  while(true) {
    if (Serial) break;
    if (millis() > 5000) break;
  }

  Serial.println("Booted Meshtastic example client v1.0");
  randomSeed(micros());

  mt_wifi_init();
}

void loop() {
  uint32_t now = millis();

  mt_wifi_loop();

  // Don't go on until we have a WiFi connection
  if (last_wifi_status != WL_CONNECTED) {
    delay(100);
    return;
  }

  // Have we asked for a node report in the last NODE_REPORT_PERIOD? If not, ask now.
  if (now >= next_node_report_time) {
    mt_protocol_send_wantconfig();
    next_node_report_time = now + NODE_REPORT_PERIOD;
  }

  // See if there are any more bytes to add to our buffer.
  size_t space_left = PB_BUFSIZE - pb_size;
  size_t bytes_read = mt_wifi_check_radio((char *)pb_buf + pb_size, space_left);
  pb_size += bytes_read;

  if (pb_size == 0) {
    // No bytes to look at. Rest a moment and loop again.
    delay(25);
    return;
  }

  // We have some bytes. See if there are any full packets in there that we can process.
  mt_protocol_check_packet(now);
}
