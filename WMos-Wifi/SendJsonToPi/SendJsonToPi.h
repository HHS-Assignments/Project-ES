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
 * @c piHost and @c piPort.  Override the defaults in setup() before calling
 * SendJsonToPi():
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
 * The implementation is written in pure C (SendJsonToPi.c).  The header
 * exposes two underlying C functions and uses either C11 @c _Generic (when
 * compiled as C) or inline C++ overloads (when compiled as C++) so that the
 * caller can always write @c SendJsonToPi(device, sensor, data) regardless of
 * whether @p data is a string or an integer.
 */

#ifndef SENDJSONTOPI_H
#define SENDJSONTOPI_H

/* ── C / C++ linkage ────────────────────────────────────────────────────── */

#ifdef __cplusplus
extern "C" {
#endif

/* ── Connection configuration ──────────────────────────────────────────── */

/**
 * @brief IP address or hostname of Pi-1.
 *
 * Defined in SendJsonToPi.c with the default value @c "10.0.42.1".
 * Reassign the pointer in setup() to point to a different host:
 * @code
 *   piHost = "192.168.1.10";
 * @endcode
 */
extern const char *piHost;

/**
 * @brief TCP port Pi-1 is listening on.
 *
 * Defined in SendJsonToPi.c with the default value @c 9000.
 * Reassign in setup() if Pi-1 uses a different port.
 */
extern int piPort;

/* ── Internal C functions (not called directly) ─────────────────────────── */

/**
 * @brief Send a JSON message to Pi-1 with a string Data field.
 *
 * Produces: {"Device":"<device>","Sensor":"<sensor>","Data":"<data>"}
 *
 * @param device  Identifies the sending device (e.g. "Wmos").
 * @param sensor  Sensor or output name (e.g. "ButtonD2", "Servo1").
 * @param data    State or reading as a C string (e.g. "pressed", "on").
 */
void SendJsonToPi_str(const char *device, const char *sensor,
                      const char *data);

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
void SendJsonToPi_int(const char *device, const char *sensor, int data);

#ifdef __cplusplus
} /* extern "C" */

/* ── C++ public API: overloaded SendJsonToPi() ──────────────────────────── */

/**
 * @brief Send a JSON message to Pi-1 (string Data overload).
 * @see SendJsonToPi_str
 */
inline void SendJsonToPi(const char *device, const char *sensor,
                         const char *data)
{
    SendJsonToPi_str(device, sensor, data);
}

/**
 * @brief Send a JSON message to Pi-1 (integer Data overload).
 * @see SendJsonToPi_int
 */
inline void SendJsonToPi(const char *device, const char *sensor, int data)
{
    SendJsonToPi_int(device, sensor, data);
}

#else /* C public API: _Generic dispatch ─────────────────────────────────── */

/**
 * @brief Send a JSON message to Pi-1.
 *
 * Dispatches to SendJsonToPi_int() when @p data is an @c int, and to
 * SendJsonToPi_str() for all other types (e.g. @c const @c char*).
 * Requires C11 or later.
 *
 * @param device  Identifies the sending device.
 * @param sensor  Sensor or output name.
 * @param data    Numeric or string reading / state.
 */
#define SendJsonToPi(device, sensor, data) \
    _Generic((data), \
        int:     SendJsonToPi_int, \
        default: SendJsonToPi_str \
    )(device, sensor, data)

#endif /* __cplusplus */

#endif /* SENDJSONTOPI_H */

