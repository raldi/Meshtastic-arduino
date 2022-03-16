#ifndef MT_WIFI_H
#define MT_WIFI_H

#include <WiFi101.h>

extern uint8_t last_wifi_status;

void mt_wifi_init();

// Tries to connect to the MT radio and, if so, to establish a TCP connection.
// Returns true iff we just did that on the current loop.
bool mt_wifi_loop();

// Check for bytes waiting on the TCP connection.
// If found, add them to buf and return how many were read.
size_t mt_wifi_check_radio(char * buf, size_t space_left);

// Send a packet over the TCP connection
bool mt_wifi_send_radio(const char * buf, size_t len);

// Call this whenever we receive a node report. If we go too long without one,
// we'll reset the connection and start over from the beginning.
void mt_wifi_reset_idle_timeout(uint32_t now);

#endif
