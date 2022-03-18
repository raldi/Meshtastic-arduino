#include "mt_internals.h"

void mt_serial_init(int8_t rx_pin, int8_t tx_pin) {
  mt_wifi_mode = false;
  mt_serial_mode = true;
}

bool mt_serial_send_radio(const char * buf, size_t len) {
}

bool mt_serial_loop() {
}

size_t mt_serial_check_radio(char * buf, size_t space_left) {
  return 0;
}

