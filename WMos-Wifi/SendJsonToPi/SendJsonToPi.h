/**
 * @file SendJsonToPi.h
 * @brief Arduino library for sending JSON sensor/output data to Pi-1 over HTTP.
 *
 * Provides a single function, SendJsonToPi(), that builds and POSTs a JSON
 * message in the format:
 *
 *   {"Device":"<device>","Sensor":"<sensor>","Data":<data>}
 *
 * The Pi-1 hostname/IP and port are stored in the library-level variables
 * @c piHost and @c piPort.  You can override the defaults in your sketch
 * before calling SendJsonToPi():
 *
 * @code
 * #include "SendJsonToPi/SendJsonToPi.h"
 *
 * void setup() {
 *   piHost = "192.168.1.10";  // override default 10.0.42.1
 *   piPort = 8080;            // override default 9000
 * }
 * @endcode
 *
 * Two overloads are provided so Data can be passed as either a String or an
 * integer (produces a JSON number rather than a quoted string).
 */

#ifndef SENDJSONTOPI_H
#define SENDJSONTOPI_H

#include <Arduino.h>

/* ── Connection configuration ──────────────────────────────────────────── */

/**
 * @brief IP address or hostname of Pi-1.
 *
 * Defined in SendJsonToPi.cpp with the default value "10.0.42.1".
 * Reassign in setup() to point to a different host:
 * @code
 *   piHost = "192.168.1.10";
 * @endcode
 */
extern const char *piHost;

/**
 * @brief TCP port Pi-1 is listening on.
 *
 * Defined in SendJsonToPi.cpp with the default value 9000.
 * Reassign in your sketch if Pi-1 uses a different port.
 */
extern int piPort;

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * @brief Send a JSON message to Pi-1 with a string Data field.
 *
 * Produces: {"Device":"<device>","Sensor":"<sensor>","Data":"<data>"}
 *
 * @param device  Identifies the sending device (e.g. "Wmos").
 * @param sensor  Sensor or output name (e.g. "ButtonD2", "Servo1").
 * @param data    State or reading as a string (e.g. "pressed", "on").
 */
void SendJsonToPi(const String &device, const String &sensor,
                  const String &data);

/**
 * @brief Send a JSON message to Pi-1 with an integer Data field.
 *
 * Produces: {"Device":"<device>","Sensor":"<sensor>","Data":<data>}
 * (Data is a JSON number, not a quoted string.)
 *
 * @param device  Identifies the sending device (e.g. "Wmos").
 * @param sensor  Sensor or output name (e.g. "ButtonD2").
 * @param data    Numeric reading or counter value.
 */
void SendJsonToPi(const String &device, const String &sensor, int data);

#endif /* SENDJSONTOPI_H */
