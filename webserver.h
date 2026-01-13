// webserver.h
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <stdbool.h>
#include "flash.h"

/**
 * Initializes the web server (binds/listens on port 80).
 * Call once after WiFi connection is established.
 * 
 * @return true on success, false on failure.
 */
bool webserver_init(bool provisioning);

/**
 * Polls the web server (must be called frequently from main loop).
 * Handles lwIP work for connections.
 */
void webserver_poll(void);

/**
 * Pushes a real-time update to all connected SSE clients.
 * Call whenever RSSI or trend changes significantly.
 * 
 * @param rssi Current RSSI value (e.g., -65).
 * @param trend Text description ("APPROACHING", "LEAVING", "STATIONARY").
 */
void webserver_push_update(char *payload, size_t size)
;
#endif // WEBSERVER_H