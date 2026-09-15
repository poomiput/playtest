/**
 * =============================================================================
 * shared_protocol.h — Shared Data Structures and Configuration
 * =============================================================================
 *
 * Standard 8-byte USB HID Keyboard Report:
 *   Byte 0: Modifier keys (Ctrl, Shift, Alt, GUI bitmask)
 *   Byte 1: Reserved (always 0x00)
 *   Byte 2-7: Up to 6 simultaneous keycodes
 *
 * =============================================================================
 */

#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

#include <stdint.h>

// --- Standard 8-byte HID Keyboard Report ---
typedef struct __attribute__((packed))
{
  uint8_t modifier;    // Bit 0: Left Ctrl,  Bit 1: Left Shift
                       // Bit 2: Left Alt,   Bit 3: Left GUI
                       // Bit 4: Right Ctrl, Bit 5: Right Shift
                       // Bit 6: Right Alt,  Bit 7: Right GUI
  uint8_t reserved;    // Always 0x00
  uint8_t keycodes[6]; // Up to 6 simultaneous key presses (USB HID keycodes)
} kbd_report_t;

// --- Wi-Fi AP Configuration ---
#define WIFI_CHANNEL 1
#define AP_MAX_CONNECTIONS 2
#define AP_HIDDEN 0

// --- WebServer Configuration ---
#define WS_PORT 80

#endif // SHARED_PROTOCOL_H
