/**
 * ================================================================
 *  Teclado Mecánico HID Dual  –  Matriz 5 × 18  (90 teclas)
 *  Plataforma : ESP32-S3 (USB nativo + BLE)
 *  Framework  : Arduino IDE  (ESP32 Arduino Core >= 2.0.0)
 *
 *  Librerías requeridas:
 *    · USB.h + USBHIDKeyboard.h  → incluidas en ESP32 Arduino Core
 *    · BleKeyboard               → https://github.com/T-vK/ESP32-BLE-Keyboard
 *    · EEPROM.h                  → incluida en ESP32 Arduino Core
 *
 * ================================================================
 *  RESUMEN DE CARACTERÍSTICAS
 * ================================================================
 *
 *  MATRIZ Y HID
 *  · Escaneo 5 × 18 con anti-ghosting / NKRO
 *  · Debounce independiente por tecla
 *  · Modificadores completos (Shift, Ctrl, Alt, GUI)
 *  · Capa Fn: flechas de cursor + gestión BLE
 *  · Modo dual automático: USB HID (VBUS presente) / BLE HID
 *
 *  BLE + EEPROM
 *  · 3 slots BLE con MAC almacenada en EEPROM
 *  · Fn+1/2/3  → seleccionar slot
 *  · Fn+P      → modo pairing (graba MAC en slot activo)
 *
 *  LED BICOLOR (cátodo común, GPIO 47=verde / GPIO 48=rojo)
 *  · Verde fijo        → funcionamiento normal / USB
 *  · Rojo fijo         → batería baja (< BAT_MV_WARN)
 *  · N parpadeos verdes → confirmación cambio de slot (N = slot+1)
 *  · Parpadeo verde rápido alternado → modo pairing activo
 *  · Parpadeo rojo/apagado rápido    → batería crítica
 *  · Fundido a negro (3 destellos)   → aviso de deep sleep
 *
 *  AHORRO DE ENERGÍA
 *  · Deep sleep tras SLEEP_TIMEOUT_MS de inactividad
 *  · Despertar por GPIO: tecla configurada en WAKE_ROW / WAKE_COL
 *    (la fila correspondiente se mantiene LOW durante el sueño
 *     y el ESP32 despierta cuando la columna cae a LOW)
 *  · Al despertar: reinicio limpio (esp_restart)
 *
 * ================================================================
 *  CONEXIONES SUGERIDAS
 * ================================================================
 *
 *  Filas    (OUTPUT activo-LOW) : GPIO 4, 5, 6, 7, 8
 *  Columnas (INPUT_PULLUP)      : GPIO 9-21, 33-37
 *  LED Verde                    : GPIO 47  (100 Ω a GND)
 *  LED Rojo                     : GPIO 48  (100 Ω a GND)
 *  Batería ADC                  : GPIO 1   (divisor resistivo)
 *  VBUS detect                  : GPIO 2   (HIGH = USB presente)
 *
 *  WAKE GPIO: La columna de la tecla de despertar debe conectarse
 *  a un pin con capacidad EXT1 wake-up (RTC GPIO).
 *  En ESP32-S3 los RTC GPIOs son: 0-21.
 *  Asegúrate de que WAKE_COL_PIN esté en ese rango.
 *
 * ================================================================
 *  EEPROM LAYOUT  (20 bytes)
 * ================================================================
 *
 *   0–5   MAC slot 0
 *   6–11  MAC slot 1
 *  12–17  MAC slot 2
 *  18     Slot activo (0–2)
 *  19     Flag de validez (0xAB)
 *
 * ================================================================
 */

#include "USB.h"
#include "USBHIDKeyboard.h"
#include <BleKeyboard.h>
#include <EEPROM.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_sleep.h>

// ════════════════════════════════════════════════════════════════
//  ①  CONFIGURACIÓN – edita esta sección para adaptar el hardware
// ════════════════════════════════════════════════════════════════

// ── Matriz ───────────────────────────────────────────────────────
#define ROWS  5
#define COLS  18

const uint8_t ROW_PINS[ROWS] = { 4, 5, 6, 7, 8 };
const uint8_t COL_PINS[COLS] = {
     9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 33, 34, 35, 36, 37
};

// ── Tecla de despertar (debe ser un RTC GPIO, rango 0-21 en S3) ──
// Usa la fila y columna del mapa KEY_MAP que corresponda.
// Por defecto: Espacio → fila 4, col 6  (ajusta según tu layout)
#define WAKE_ROW      4     // Fila de la tecla que despierta
#define WAKE_COL      6     // Columna de la tecla que despierta
#define WAKE_COL_PIN  COL_PINS[WAKE_COL]   // GPIO de esa columna

// ── LED bicolor (cátodo común) ────────────────────────────────────
#define PIN_LED_GREEN  47
#define PIN_LED_RED    48

// ── Batería ───────────────────────────────────────────────────────
#define PIN_BAT_ADC    1
#define PIN_VBUS       2       // HIGH = cable USB presente

#define BAT_R_HIGH   100000UL  // 100 kΩ – resistencia alta del divisor
#define BAT_R_LOW     47000UL  //  47 kΩ – resistencia baja del divisor
#define BAT_MV_MAX     4200UL  // mV al 100 %
#define BAT_MV_MIN     3300UL  // mV al   0 % (corte de seguridad)
#define BAT_MV_WARN    3600UL  // mV → LED rojo (batería baja)
#define BAT_MV_CRIT    3450UL  // mV → alerta crítica (parpadeo rojo)

// ── Tiempos (ms) ─────────────────────────────────────────────────
#define DEBOUNCE_MS         10UL
#define SCAN_INTERVAL_US   500UL
#define BAT_CHECK_MS     30000UL   // Intervalo de revisión de batería

// ── Deep sleep ────────────────────────────────────────────────────
// Tiempo de inactividad antes de dormir. Cambia este valor.
#define SLEEP_TIMEOUT_MS  300000UL   // 5 minutos por defecto

// ── BLE ───────────────────────────────────────────────────────────
#define BLE_DEVICE_NAME  "MiTeclado"
#define BLE_MANUFACTURER "DIY"

// ════════════════════════════════════════════════════════════════
//  ②  MAPA DE TECLAS
// ════════════════════════════════════════════════════════════════

// Códigos internos (no se envían por HID)
#define KEY_FN       0xF0
#define KEY_PAIRING  0xF1
#define FN_SLOT1     0xE1   // Fn+1 → slot 0
#define FN_SLOT2     0xE2   // Fn+2 → slot 1
#define FN_SLOT3     0xE3   // Fn+3 → slot 2

// Capa 0 – pulsación normal
const uint8_t KEY_MAP[ROWS][COLS] = {
  { KEY_ESC,        KEY_F1,  KEY_F2,  KEY_F3,  KEY_F4,  KEY_F5,  KEY_F6,  KEY_F7,
    KEY_F8,         KEY_F9,  KEY_F10, KEY_F11, KEY_F12,
    KEY_DELETE, KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN },

  { '`',   '1','2','3','4','5','6','7','8','9','0','-','=',
    KEY_BACKSPACE, KEY_INSERT, 0x00, KEY_NUM_LOCK, '/' },

  { KEY_TAB, 'q','w','e','r','t','y','u','i','o','p','[',']','\\',
    0x00, 0x00, '*', '-' },

  { KEY_CAPS_LOCK, 'a','s','d','f','g','h','j','k','l',';','\'',
    KEY_RETURN, 0x00, 0x00, 0x00, 0x00, '+' },

  { KEY_LEFT_SHIFT, 'z','x','c','v','b','n','m',',','.','/',
    KEY_RIGHT_SHIFT, 0x00, KEY_FN,
    KEY_LEFT_CTRL, KEY_LEFT_ALT, KEY_RETURN, 0x00 }
};

// Capa 1 – Fn activa
const uint8_t FN_MAP[ROWS][COLS] = {
  { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },

  // Fn+1/2/3 → seleccionar slot BLE
  { 0x00, FN_SLOT1, FN_SLOT2, FN_SLOT3, 0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },

  // Fn+P → pairing  |  Fn+[ → flecha arriba
  { 0x00,0x00,0x00,0x00, KEY_PAIRING, 0x00,0x00,0x00,
    0x00,0x00,0x00, KEY_UP_ARROW, 0x00,0x00,0x00,0x00,0x00,0x00 },

  // Fn+; → izq  |  Fn+' → abajo  |  Fn+Enter → der
  { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00, KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_ARROW,
    0x00,0x00,0x00,0x00,0x00 },

  { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }
};

// ════════════════════════════════════════════════════════════════
//  ③  EEPROM
// ════════════════════════════════════════════════════════════════
#define EEPROM_SIZE        20
#define EEPROM_SLOT_BASE    0    // Bytes  0-17: MACs
#define EEPROM_ACTIVE_SLOT 18    // Byte  18:   slot activo
#define EEPROM_VALID_FLAG  19    // Byte  19:   magic 0xAB
#define EEPROM_MAGIC     0xAB
#define MAC_LEN             6
#define NUM_SLOTS           3

// ════════════════════════════════════════════════════════════════
//  ④  OBJETOS GLOBALES
// ════════════════════════════════════════════════════════════════
USBHIDKeyboard UsbKeyboard;
BleKeyboard    BleKbd(BLE_DEVICE_NAME, BLE_MANUFACTURER, 100);

// ── Estado de la matriz ───────────────────────────────────────
bool     keyState[ROWS][COLS]         = {};
bool     rawState[ROWS][COLS]         = {};
uint32_t debounceTimer[ROWS][COLS]    = {};

// ── Estado global ─────────────────────────────────────────────
bool     fnActive     = false;
bool     usbMode      = false;
bool     pairingMode  = false;
uint8_t  activeSlot   = 0;
uint8_t  savedMAC[NUM_SLOTS][MAC_LEN] = {};

// ── Temporización ─────────────────────────────────────────────
uint32_t lastScanUs   = 0;
uint32_t lastBatMs    = 0;
uint32_t lastActivityMs = 0;   // Marca de última pulsación

// ── LED ───────────────────────────────────────────────────────
bool     batLow       = false;
bool     batCrit      = false;

// ════════════════════════════════════════════════════════════════
//  ⑤  LED – funciones de bajo nivel y secuencias
// ════════════════════════════════════════════════════════════════

void ledGreen() { digitalWrite(PIN_LED_GREEN,HIGH); digitalWrite(PIN_LED_RED,LOW);  }
void ledRed()   { digitalWrite(PIN_LED_GREEN,LOW);  digitalWrite(PIN_LED_RED,HIGH); }
void ledOff()   { digitalWrite(PIN_LED_GREEN,LOW);  digitalWrite(PIN_LED_RED,LOW);  }

/**
 * Parpadea el LED verde N veces (bloqueante, uso puntual).
 * Se usa para confirmar el slot seleccionado.
 *   slot 0 → 1 parpadeo
 *   slot 1 → 2 parpadeos
 *   slot 2 → 3 parpadeos
 */
void ledBlinkGreen(uint8_t times, uint16_t onMs = 120, uint16_t offMs = 120) {
    for (uint8_t i = 0; i < times; i++) {
        ledGreen(); delay(onMs);
        ledOff();   delay(offMs);
    }
}

/**
 * Parpadea el LED rojo N veces (bloqueante, uso puntual).
 */
void ledBlinkRed(uint8_t times, uint16_t onMs = 120, uint16_t offMs = 120) {
    for (uint8_t i = 0; i < times; i++) {
        ledRed();  delay(onMs);
        ledOff();  delay(offMs);
    }
}

/**
 * Secuencia de aviso de deep sleep inminente:
 * 3 destellos rápidos del color actual → apaga.
 */
void ledSleepWarning() {
    for (uint8_t i = 0; i < 3; i++) {
        batLow ? ledRed() : ledGreen();
        delay(80);
        ledOff();
        delay(80);
    }
}

/**
 * Actualiza el LED en el loop según el estado actual.
 * Las secuencias de confirmación (bloqueantes) se llaman
 * directamente desde los eventos, no desde aquí.
 *
 * Estados en orden de prioridad:
 *   1. Modo pairing   → parpadeo verde/apagado rápido (no bloqueante)
 *   2. Batería crítica → parpadeo rojo rápido (no bloqueante)
 *   3. Batería baja   → rojo fijo
 *   4. Normal / USB   → verde fijo
 */
void updateLED() {
    static uint32_t lastToggleMs = 0;
    static bool     ledToggle    = false;

    uint32_t now = millis();

    if (pairingMode) {
        // Parpadeo verde 4 Hz
        if ((now - lastToggleMs) >= 125) {
            lastToggleMs = now;
            ledToggle = !ledToggle;
            ledToggle ? ledGreen() : ledOff();
        }
        return;
    }

    if (batCrit && !usbMode) {
        // Parpadeo rojo 2 Hz
        if ((now - lastToggleMs) >= 250) {
            lastToggleMs = now;
            ledToggle = !ledToggle;
            ledToggle ? ledRed() : ledOff();
        }
        return;
    }

    // Estado estático
    (batLow && !usbMode) ? ledRed() : ledGreen();
}

// ════════════════════════════════════════════════════════════════
//  ⑥  BATERÍA
// ════════════════════════════════════════════════════════════════

uint32_t readBatteryMv() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 8; i++) {
        sum += analogReadMilliVolts(PIN_BAT_ADC);
        delay(1);
    }
    // Compensar divisor: V_bat = V_adc × (R_H + R_L) / R_L
    return (sum / 8) * (BAT_R_HIGH + BAT_R_LOW) / BAT_R_LOW;
}

uint8_t batteryPercent(uint32_t mv) {
    if (mv >= BAT_MV_MAX) return 100;
    if (mv <= BAT_MV_MIN) return 0;
    return (uint8_t)((mv - BAT_MV_MIN) * 100UL / (BAT_MV_MAX - BAT_MV_MIN));
}

void checkBattery() {
    uint32_t mv  = readBatteryMv();
    uint8_t  pct = batteryPercent(mv);

    bool prevCrit = batCrit;

    batLow  = (mv < BAT_MV_WARN) && !usbMode;
    batCrit = (mv < BAT_MV_CRIT) && !usbMode;

    // Primera vez que entra en crítico: 5 parpadeos rojos de alerta
    if (batCrit && !prevCrit) {
        ledBlinkRed(5, 80, 80);
        Serial.println("[BAT] *** BATERÍA CRITICA ***");
    }

    if (!usbMode && BleKbd.isConnected())
        BleKbd.setBatteryLevel(pct);

    Serial.printf("[BAT] %lu mV  %u%%  [%s]  [%s]\n",
        mv, pct,
        batCrit ? "CRITICA" : batLow ? "BAJA" : "OK",
        usbMode ? "USB" : "BLE");
}

// ════════════════════════════════════════════════════════════════
//  ⑦  EEPROM
// ════════════════════════════════════════════════════════════════

void eepromSaveAll() {
    for (uint8_t s = 0; s < NUM_SLOTS; s++)
        for (uint8_t b = 0; b < MAC_LEN; b++)
            EEPROM.write(EEPROM_SLOT_BASE + s * MAC_LEN + b, savedMAC[s][b]);
    EEPROM.write(EEPROM_ACTIVE_SLOT, activeSlot);
    EEPROM.write(EEPROM_VALID_FLAG,  EEPROM_MAGIC);
    EEPROM.commit();
}

void eepromSaveSlot(uint8_t slot) {
    if (slot >= NUM_SLOTS) return;
    for (uint8_t b = 0; b < MAC_LEN; b++)
        EEPROM.write(EEPROM_SLOT_BASE + slot * MAC_LEN + b, savedMAC[slot][b]);
    EEPROM.write(EEPROM_ACTIVE_SLOT, activeSlot);
    EEPROM.write(EEPROM_VALID_FLAG,  EEPROM_MAGIC);
    EEPROM.commit();
}

void eepromLoad() {
    if (EEPROM.read(EEPROM_VALID_FLAG) != EEPROM_MAGIC) {
        Serial.println("[EEPROM] Primera inicializacion");
        memset(savedMAC, 0, sizeof(savedMAC));
        activeSlot = 0;
        eepromSaveAll();
        return;
    }
    for (uint8_t s = 0; s < NUM_SLOTS; s++)
        for (uint8_t b = 0; b < MAC_LEN; b++)
            savedMAC[s][b] = EEPROM.read(EEPROM_SLOT_BASE + s * MAC_LEN + b);
    activeSlot = EEPROM.read(EEPROM_ACTIVE_SLOT) % NUM_SLOTS;
}

void eepromPrintMACs() {
    for (uint8_t s = 0; s < NUM_SLOTS; s++) {
        Serial.printf("  Slot %u: %02X:%02X:%02X:%02X:%02X:%02X%s\n", s,
            savedMAC[s][0], savedMAC[s][1], savedMAC[s][2],
            savedMAC[s][3], savedMAC[s][4], savedMAC[s][5],
            (s == activeSlot) ? "  <- activo" : "");
    }
}

// ════════════════════════════════════════════════════════════════
//  ⑧  DEEP SLEEP
// ════════════════════════════════════════════════════════════════

/**
 * Configura el wake-up por EXT1 (nivel LOW) en el pin de columna
 * de la tecla de despertar, mantiene la fila correspondiente en
 * LOW (activa) durante el sueño y entra en deep sleep.
 *
 * Al despertar el ESP32-S3 hace un reset completo, por lo que
 * setup() se ejecuta de nuevo como un arranque normal.
 *
 * IMPORTANTE: La fila WAKE_ROW debe permanecer en LOW durante el
 * sueño para que la tecla pueda crear el nivel de wake-up.
 * El resto de filas se ponen en HIGH-Z (INPUT) para no consumir.
 */
void enterDeepSleep() {
    Serial.println("[SLEEP] Entrando en deep sleep...");

    // Aviso visual: 3 destellos
    ledSleepWarning();
    ledOff();

    // Desconectar BLE limpiamente
    if (!usbMode && BleKbd.isConnected()) BleKbd.end();

    // Preparar la matriz para el wake-up:
    // Poner todas las filas como INPUT (alta impedancia) salvo WAKE_ROW
    for (uint8_t r = 0; r < ROWS; r++) {
        if (r == WAKE_ROW) {
            pinMode(ROW_PINS[r], OUTPUT);
            digitalWrite(ROW_PINS[r], LOW);   // Mantener activa
        } else {
            pinMode(ROW_PINS[r], INPUT);       // Alta impedancia
        }
    }

    // Wake-up por EXT1: despierta cuando WAKE_COL_PIN baje a LOW
    // (el pull-up interno sigue activo durante el sueño en EXT1)
    uint64_t wakeMask = (1ULL << WAKE_COL_PIN);
    esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW);

    delay(100);   // Tiempo para que el serial envíe el último mensaje
    esp_deep_sleep_start();
    // ── El programa no continúa desde aquí ──
}

// ════════════════════════════════════════════════════════════════
//  ⑨  BLE – SLOTS Y PAIRING
// ════════════════════════════════════════════════════════════════

bool detectUSB() { return digitalRead(PIN_VBUS) == HIGH; }

void selectSlot(uint8_t slot) {
    if (slot >= NUM_SLOTS) return;
    activeSlot = slot;
    EEPROM.write(EEPROM_ACTIVE_SLOT, activeSlot);
    EEPROM.commit();

    Serial.printf("[BLE] Slot %u seleccionado\n", activeSlot);
    eepromPrintMACs();

    // Confirmación visual: N parpadeos verdes (N = slot + 1)
    ledBlinkGreen(activeSlot + 1, 120, 120);

    if (!usbMode) { BleKbd.end(); delay(300); BleKbd.begin(); }
}

void enterPairingMode() {
    pairingMode = true;
    Serial.printf("[BLE] Modo pairing activo → slot %u\n", activeSlot);
    // El LED parpadeará verde desde updateLED() hasta que se complete
    if (!usbMode) { BleKbd.end(); delay(300); BleKbd.begin(); }
}

void onBleConnected() {
    if (!pairingMode) return;

    int count = esp_ble_get_bond_device_num();
    if (count > 0) {
        esp_ble_bond_dev_t *list =
            (esp_ble_bond_dev_t *)malloc(count * sizeof(esp_ble_bond_dev_t));
        if (list) {
            esp_ble_get_bond_device_list(&count, list);
            memcpy(savedMAC[activeSlot], list[count - 1].bd_addr, MAC_LEN);
            free(list);
            eepromSaveSlot(activeSlot);
            Serial.printf("[BLE] MAC guardada en slot %u: "
                          "%02X:%02X:%02X:%02X:%02X:%02X\n",
                activeSlot,
                savedMAC[activeSlot][0], savedMAC[activeSlot][1],
                savedMAC[activeSlot][2], savedMAC[activeSlot][3],
                savedMAC[activeSlot][4], savedMAC[activeSlot][5]);
        }
    }
    pairingMode = false;
    // Confirmación: 3 parpadeos rápidos verdes
    ledBlinkGreen(3, 80, 80);
}

// ════════════════════════════════════════════════════════════════
//  ⑩  ESCANEO DE MATRIZ Y EVENTOS HID
// ════════════════════════════════════════════════════════════════

void scanMatrix() {
    for (uint8_t row = 0; row < ROWS; row++) {
        digitalWrite(ROW_PINS[row], LOW);
        delayMicroseconds(10);
        for (uint8_t col = 0; col < COLS; col++)
            rawState[row][col] = (digitalRead(COL_PINS[col]) == LOW);
        digitalWrite(ROW_PINS[row], HIGH);
    }
}

void hidPress(uint8_t key) {
    if (usbMode)                   UsbKeyboard.press(key);
    else if (BleKbd.isConnected()) BleKbd.press(key);
}

void hidRelease(uint8_t key) {
    if (usbMode)                   UsbKeyboard.release(key);
    else if (BleKbd.isConnected()) BleKbd.release(key);
}

void onKeyEvent(uint8_t row, uint8_t col, bool pressed) {
    uint8_t baseKey = KEY_MAP[row][col];
    uint8_t keycode = fnActive ? FN_MAP[row][col] : baseKey;

    // Tecla Fn: actualiza estado, no genera HID
    if (baseKey == KEY_FN) { fnActive = pressed; return; }

    if (!pressed) {
        if (keycode != 0x00 && keycode < 0xE0) hidRelease(keycode);
        return;
    }

    // Solo al pulsar: registrar actividad para el timeout de sleep
    lastActivityMs = millis();

    switch (keycode) {
        case FN_SLOT1:    selectSlot(0);      return;
        case FN_SLOT2:    selectSlot(1);      return;
        case FN_SLOT3:    selectSlot(2);      return;
        case KEY_PAIRING: enterPairingMode(); return;
        case 0x00:        return;
        default:          hidPress(keycode);  return;
    }
}

void processDebounce() {
    uint32_t now = millis();
    for (uint8_t row = 0; row < ROWS; row++) {
        for (uint8_t col = 0; col < COLS; col++) {
            bool raw     = rawState[row][col];
            bool current = keyState[row][col];
            if (raw != current) {
                if (!debounceTimer[row][col])
                    debounceTimer[row][col] = now ? now : 1;
                else if ((now - debounceTimer[row][col]) >= DEBOUNCE_MS) {
                    keyState[row][col]      = raw;
                    debounceTimer[row][col] = 0;
                    onKeyEvent(row, col, raw);
                }
            } else {
                debounceTimer[row][col] = 0;
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== Teclado 5x18 Dual HID – v3.0 ===");

    // Pines de matriz
    for (uint8_t r = 0; r < ROWS; r++) {
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], HIGH);
    }
    for (uint8_t c = 0; c < COLS; c++)
        pinMode(COL_PINS[c], INPUT_PULLUP);

    // LED y detección VBUS
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED,   OUTPUT);
    pinMode(PIN_VBUS,      INPUT);
    ledOff();

    // EEPROM
    EEPROM.begin(EEPROM_SIZE);
    eepromLoad();
    Serial.printf("[EEPROM] Slot activo: %u\n", activeSlot);
    eepromPrintMACs();

    // Modo de conexión
    usbMode = detectUSB();
    Serial.printf("[MODO] %s\n", usbMode ? "USB HID" : "BLE HID");

    if (usbMode) {
        UsbKeyboard.begin();
        USB.begin();
    } else {
        BleKbd.begin();
    }

    // Batería inicial
    checkBattery();

    // Inicializar temporizador de actividad
    lastActivityMs = millis();
    lastBatMs      = millis();

    // Parpadeo de inicio: 1 destello verde rápido
    ledBlinkGreen(1, 200, 0);
    updateLED();

    Serial.printf("[SLEEP] Timeout de inactividad: %lu ms\n", SLEEP_TIMEOUT_MS);
}

// ════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ════════════════════════════════════════════════════════════════
void loop() {
    uint32_t nowUs = micros();
    uint32_t nowMs = millis();

    // ── Escaneo de matriz ─────────────────────────────────────
    if ((nowUs - lastScanUs) >= SCAN_INTERVAL_US) {
        lastScanUs = nowUs;
        scanMatrix();
        processDebounce();
    }

    // ── Batería + detección de modo ───────────────────────────
    if ((nowMs - lastBatMs) >= BAT_CHECK_MS) {
        lastBatMs = nowMs;

        bool wasUsb = usbMode;
        usbMode = detectUSB();
        if (usbMode != wasUsb) {
            Serial.printf("[MODO] Cambio → %s\n", usbMode ? "USB HID" : "BLE HID");
            if (!usbMode) BleKbd.begin();
        }

        checkBattery();

        if (!usbMode && BleKbd.isConnected())
            onBleConnected();
    }

    // ── Timeout de inactividad → deep sleep ──────────────────
    // No entrar en sleep si el cable USB está conectado
    if (!usbMode && ((nowMs - lastActivityMs) >= SLEEP_TIMEOUT_MS)) {
        enterDeepSleep();   // No retorna
    }

    // ── LED ───────────────────────────────────────────────────
    updateLED();
}
