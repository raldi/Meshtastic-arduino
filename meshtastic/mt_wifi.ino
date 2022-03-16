#include "mt_wifi.h"

#include "arduino_secrets.h"  // <-- Put your SSID and wifi password in here

// If we go this long without making a connection, try again
#define CONNECT_TIMEOUT (10 * 1000)

// If we go this long without receiving a valid packet, reconnect
#define IDLE_TIMEOUT (60 * 1000)

// These are the pins for an Adafruit Feather M0 WiFi
#define WIFI_CS_PIN 8
#define WIFI_IRQ_PIN 7
#define WIFI_RESET_PIN 4
#define WIFI_ENABLE_PIN 2

// These are the default IP and port for a MT node in AP mode.
#define RADIO_IP "192.168.42.1"
#define RADIO_PORT 4403

// An invalid status code we use when we need a value that's
// assuredly different from any actual one
#define UNUSED_WIFI_STATUS 254

uint8_t last_wifi_status;  // The wifi status from the previous loop, so we can see when it changes
uint32_t next_connect_attempt;  // When millis() >= this, it's time to connect

WiFiClient client;

void mt_wifi_init() {
  WiFi.setPins(WIFI_CS_PIN, WIFI_IRQ_PIN, WIFI_RESET_PIN, WIFI_ENABLE_PIN);
  next_connect_attempt = 0;
  last_wifi_status = UNUSED_WIFI_STATUS;
}

bool mt_wifi_loop() {
  uint32_t now = millis();
  uint8_t wifi_status = WiFi.status();

  // Is it time to try (re)connecting?
  if (now >= next_connect_attempt) {
    // Force a new connect attempt as if from the beginning
    last_wifi_status = UNUSED_WIFI_STATUS;
    wifi_status = WL_IDLE_STATUS;
  }

  // If the status hasn't changed, we have nothing more to do.
  if (wifi_status == last_wifi_status) return false;
  last_wifi_status = wifi_status;

  switch (wifi_status) {
    case WL_NO_SHIELD:
      Serial.println("No WiFi shield detected");
      while(true);
    case WL_CONNECT_FAILED:
    case WL_CONNECTION_LOST:
    case WL_IDLE_STATUS:
      // We just lost a connection, or we're starting up, or we've timed out and
      // want to reconnect.
      WiFi.setTimeout(CONNECT_TIMEOUT);
      next_connect_attempt = now + CONNECT_TIMEOUT;
      Serial.println("Attempting to connect to WiFi...");

      // FYI, this can block for up to CONNECT_TIMEOUT
      if (WIFI_PASS == NULL) {
        WiFi.begin(WIFI_SSID);
      } else {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
      }
      return false;
    case WL_CONNECTED:
      // We just connected to WiFi! Now try to make a TCP connection. If it fails, nobody's
      // actually noticing, but we'll time out soon enough when no packets come in, and then
      // we'll try again from the beginning.
      print_wifi_status();
      mt_wifi_reset_idle_timeout(now);
      return open_tcp_connection();
    case WL_DISCONNECTED:
      // We maybe just started trying to join a network. Be patient...
      return false;
    case WL_NO_SSID_AVAIL:
    case WL_SCAN_COMPLETED:
    case WL_AP_LISTENING:
    case WL_AP_CONNECTED:
    case WL_AP_FAILED:
    case WL_PROVISIONING:
    case WL_PROVISIONING_FAILED:
    default:
      // None of these should ever happen. If they do, let me know what led to them and I'll
      // update this switch statement.
      Serial.print("Unknown WiFi status ");
      Serial.println(wifi_status);
      while(true);
  }
}

bool open_tcp_connection() {
  if (!client.connect(RADIO_IP, RADIO_PORT)) {
    Serial.println("Failed to establish TCP connection");
    return false;
  } else {
    Serial.println("TCP connection established");
    return true;
  }
}

// Check for bytes waiting on the TCP connection.
// If found, add them to buf and return how many were read.
size_t mt_wifi_check_radio(char * buf, size_t space_left) {
  if (!client.connected()) {
    Serial.println("Lost TCP connection");
    return 0;
  }
  size_t bytes_read = 0;
  while (client.available()) {
    char c = client.read();
    *buf++ = c;
    if (++bytes_read >= space_left) {
      Serial.println("TCP overflow");
      client.stop();
      break;
    }
  }
  return bytes_read;
}

// Send a packet over the TCP connection
bool mt_wifi_send_radio(const char * buf, size_t len) {
  if (!client.connected()) {
    Serial.println("Lost TCP connection? Attempting to reconnect...");
    if (!open_tcp_connection()) return false;
  }
  /*
  Serial.print("About to send ");
  for (int i = 0 ; i < len ; i++) {
    Serial.print(buf[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  */
  size_t wrote = client.write(buf, len);
  if (wrote == len) return true;

  Serial.print("Tried to sendRadio ");
  Serial.print(len);
  Serial.print(" but actually sent ");
  Serial.println(wrote);
  client.stop();
  return false;
}

// Call this whenever we receive a node report. If we go too long without one,
// we'll reset the connection and start over from the beginning.
void mt_wifi_reset_idle_timeout(uint32_t now) {
  next_connect_attempt = now + IDLE_TIMEOUT;
}

void print_wifi_status() {
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
