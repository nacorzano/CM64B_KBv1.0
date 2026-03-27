/**
 * ================================================================
 *  Teclado Mecánico HID Dual  –  Matriz 5 × 18
 *  Layout  : Commodore-based ANSI (commodore-based-ansi.json)
 *  Hardware: ESP32-S3-DevKitC-1 N16R8
 *  Framework: Arduino IDE + ESP32 Arduino Core >= 2.0.5
 *
 *  Librerías requeridas (instalar en Arduino IDE):
 *    · BleKeyboard  → https://github.com/T-vK/ESP32-BLE-Keyboard
 *    · EEPROM.h     → incluida en el core (NVS flash)
 *    · USB.h        → incluida en el core (ESP32 Arduino >= 2.x)
 *    · USBHIDKeyboard.h → incluida en el core
 *
 *  Configuración en Arduino IDE:
 *    · Placa  : "ESP32S3 Dev Module"
 *    · USB Mode: "USB-OTG (TinyUSB)"   ← IMPRESCINDIBLE
 *    · USB CDC On Boot: "Enabled"
 *    · Flash Size: "16MB"
 *    · PSRAM: "OPI PSRAM"
 *    · Partition Scheme: "16MB Flash (3MB APP/9.9MB FATFS)"
 *
 * ================================================================
 *
 *  CARACTERÍSTICAS
 *  ───────────────
 *  · Modo dual automático:
 *      USB HID  cuando el conector "USB" (OTG) está conectado al PC
 *      BLE HID  cuando no hay cable USB
 *  · Matriz 5×18 con debounce independiente por tecla
 *  · Anti-ghosting / NKRO
 *  · 3 slots BLE con MAC en EEPROM (NVS flash)
 *  · Capa Fn: F1-F12, slots BLE, Print Screen
 *  · Teclas macro: F13-F16 y F18-F20
 *  · LED bicolor R/G: verde = OK, rojo = batería baja
 *  · Deep sleep tras inactividad configurable
 *  · Monitor de batería por ADC con alerta crítica
 *
 *  MAPA DE TECLAS ESPECIALES
 *  ──────────────────────────
 *  F13 → LAlt + Tab          (cambiar ventana)
 *  F14 → Ctrl + C            (copiar)
 *  F15 → Ctrl + V            (pegar)
 *  F16 → GUI  + V            (portapapeles Windows)
 *  F18 → Ctrl + Alt + Supr   (administrador de tareas)
 *  F19 → Vol+
 *  F20 → Vol-
 *
 *  Fn + 1..0,-,= → F1..F12
 *  Fn + Q/W/E    → Slot BLE 0/1/2  (N parpadeos de confirmación)
 *  Fn + R        → Modo pairing
 *  Fn + P        → Print Screen
 *
 * ================================================================
 *  PINOUT – ESP32-S3-DevKitC-1 N16R8
 * ================================================================
 *
 *  ┌─────────────────────────────────────────────────────────┐
 *  │  SEÑAL          GPIO   Header  Notas                    │
 *  ├─────────────────────────────────────────────────────────┤
 *  │  ROW 0          GPIO 4   J1    Fila 0 (top)            │
 *  │  ROW 1          GPIO 5   J1                            │
 *  │  ROW 2          GPIO 6   J1                            │
 *  │  ROW 3          GPIO 7   J1                            │
 *  │  ROW 4          GPIO 8   J1    Fila 4 (bottom)         │
 *  ├─────────────────────────────────────────────────────────┤
 *  │  COL  0         GPIO 9   J1    F13/F14/F15/F16/---     │
 *  │  COL  1         GPIO10   J1    ESC/Tab/Caps/Shift/---  │
 *  │  COL  2         GPIO11   J1    1/Q/A/Z/CTR             │
 *  │  COL  3         GPIO12   J1    2/W/S/X/MENU            │
 *  │  COL  4         GPIO13   J1    3/E/D/C/---             │
 *  │  COL  5         GPIO14   J1    4/R/F/V/SPACE ←WAKE    │
 *  │  COL  6         GPIO15   J1    5/T/G/B/SPACE           │
 *  │  COL  7         GPIO16   J1    6/Y/H/N/SPACE           │
 *  │  COL  8         GPIO17   J1    7/U/J/M/SPACE           │
 *  │  COL  9         GPIO18   J1    8/I/K/,/SPACE           │
 *  │  COL 10         GPIO21   J1    9/O/L/./FN              │
 *  │  COL 11         GPIO39   J1    0/P/;///ALT             │
 *  │  COL 12         GPIO40   J1    -/[/'/RShift/---        │
 *  │  COL 13         GPIO41   J1    =/]/"/---/---           │
 *  │  COL 14         GPIO42   J1    Bksp/\/Enter/---/---    │
 *  │  COL 15         GPIO47   J3    F18/DEL/HOME/←/---      │
 *  │  COL 16         GPIO48   J3    F19/PgUp/↑/↓/---        │
 *  │  COL 17         GPIO45   J3    F20/PgDn/END/→/---  ⚠  │
 *  ├─────────────────────────────────────────────────────────┤
 *  │  LED VERDE      GPIO 2   J1    100Ω a GND, cátodo com. │
 *  │  LED ROJO       GPIO 3   J1    100Ω a GND, cátodo com. │
 *  │  BAT ADC        GPIO 1   J1    ADC1-CH0, divisor R     │
 *  │  VBUS detect    interno  ---   USB.vbusPresent()       │
 *  └─────────────────────────────────────────────────────────┘
 *
 *  ⚠  GPIO 45 (COL17): strapping SPI voltage, pull-down débil.
 *     El INPUT_PULLUP del código lo lleva a HIGH tras el boot.
 *     No afecta al funcionamiento una vez arrancado.
 *
 *  ⚠  GPIO  3 (LED rojo): strapping JTAG. Cátodo común → HIGH=on,
 *     al boot GPIO3=LOW → LED apagado → no activa JTAG. ✓
 *     Añadir resistencia pull-down 10kΩ a GND para mayor seguridad.
 *
 *  ⚠  GPIO35,36,37: reservados por OPI PSRAM (N16R8) → NO usar.
 *
 *  DIVISOR RESISTIVO PARA ADC DE BATERÍA (GPIO 1)
 *  ────────────────────────────────────────────────
 *  V_bat ──[ 100kΩ ]──┬──[ 47kΩ ]── GND
 *                      └── GPIO 1 (ADC)
 *  V_adc = V_bat × 47k / (100k + 47k) = V_bat × 0.32
 *  Para V_bat = 4.2V → V_adc ≈ 1.34V  (dentro del rango 0-3.3V ✓)
 *
 *  CONEXIÓN DEL LED BICOLOR (cátodo común)
 *  ─────────────────────────────────────────
 *  GPIO 2 ──[ 100Ω ]── Ánodo Verde ─┐
 *  GPIO 3 ──[ 100Ω ]── Ánodo Rojo  ─┤── Cátodo común ── GND
 *
 * ================================================================
 *  EEPROM LAYOUT (20 bytes en NVS flash)
 * ================================================================
 *   0– 5  MAC slot 0
 *   6–11  MAC slot 1
 *  12–17  MAC slot 2
 *  18     Slot activo (0–2)
 *  19     Magic flag (0xAB = inicializado)
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
//  ①  CONFIGURACIÓN DE HARDWARE
// ════════════════════════════════════════════════════════════════

#define ROWS  5
#define COLS  18

// Filas: OUTPUT activo-LOW
const uint8_t ROW_PINS[ROWS] = { 4, 5, 6, 7, 8 };

// Columnas: INPUT_PULLUP (todos los GPIOs del S3 tienen pull-up interno)
// Índice:                  C0   C1   C2   C3   C4   C5   C6   C7   C8   C9
//                         C10  C11  C12  C13  C14  C15  C16  C17
const uint8_t COL_PINS[COLS] = {
     9,  10,  11,  12,  13,  14,  15,  16,  17,  18,
    21,  39,  40,  41,  42,  47,  48,  45
};

// LED bicolor (cátodo común: HIGH = encendido)
#define PIN_LED_GREEN   2
#define PIN_LED_RED     3

// Batería: divisor resistivo en GPIO 1 (ADC1-CH0)
#define PIN_BAT_ADC     1
#define BAT_R_HIGH   100000UL   // 100 kΩ
#define BAT_R_LOW     47000UL   //  47 kΩ
#define BAT_MV_MAX     4200UL   // mV al 100%
#define BAT_MV_MIN     3300UL   // mV al   0%
#define BAT_MV_WARN    3600UL   // mV → LED rojo
#define BAT_MV_CRIT    3450UL   // mV → alerta parpadeo + deep sleep acelerado

// ════════════════════════════════════════════════════════════════
//  ②  TEMPORIZACIÓN
// ════════════════════════════════════════════════════════════════

#define DEBOUNCE_MS          10UL
#define SCAN_INTERVAL_US    500UL
#define BAT_CHECK_MS      30000UL

// Timeout de inactividad antes de deep sleep (modificar según necesidad)
#define SLEEP_TIMEOUT_MS  300000UL   // 5 minutos

// Tecla de despertar: Espacio (fila 4, col 5 → GPIO 14, RTC GPIO ✓)
#define WAKE_ROW          4
#define WAKE_COL          5
#define WAKE_COL_PIN      COL_PINS[WAKE_COL]   // GPIO 14

// ════════════════════════════════════════════════════════════════
//  ③  BLE Y EEPROM
// ════════════════════════════════════════════════════════════════

#define BLE_DEVICE_NAME   "MiTeclado"
#define BLE_MANUFACTURER  "DIY"
#define NUM_SLOTS          3
#define MAC_LEN            6

#define EEPROM_SIZE         20
#define EEPROM_SLOT_BASE     0
#define EEPROM_ACTIVE_SLOT  18
#define EEPROM_VALID_FLAG   19
#define EEPROM_MAGIC      0xAB

// ════════════════════════════════════════════════════════════════
//  ④  CÓDIGOS INTERNOS (no se envían por HID)
// ════════════════════════════════════════════════════════════════

#define KC_FN        0xF0   // Tecla Fn
#define KC_PAIRING   0xF1   // Fn+R → modo pairing
#define KC_SLOT0     0xE0   // Fn+Q → slot BLE 0
#define KC_SLOT1     0xE1   // Fn+W → slot BLE 1
#define KC_SLOT2     0xE2   // Fn+E → slot BLE 2

// Macros multi-tecla (se ejecutan en sendMacro)
#define KC_MACRO_F13 0xD0   // LAlt + Tab
#define KC_MACRO_F14 0xD1   // Ctrl + C
#define KC_MACRO_F15 0xD2   // Ctrl + V
#define KC_MACRO_F16 0xD3   // GUI  + V
#define KC_MACRO_F18 0xD4   // Ctrl + Alt + Supr
#define KC_MEDIA_VUP 0xD5   // Volumen +
#define KC_MEDIA_VDN 0xD6   // Volumen -
#define KC_MENU_KEY  0xFE   // Tecla MENU del layout

// ════════════════════════════════════════════════════════════════
//  ⑤  MAPAS DE TECLAS
// ════════════════════════════════════════════════════════════════
//
//  Layout físico (columnas):
//  C0   C1    C2  C3  C4  C5  C6  C7  C8  C9  C10 C11  C12   C13  C14     C15   C16   C17
//  Fx | ESC   1   2   3   4   5   6   7   8   9   0   -     =    BKSP  | F18  F19   F20   ← F0
//  Fx | TAB   Q   W   E   R   T   Y   U   I   O   P   [     ]    \     | DEL  PGUP  PGDN  ← F1
//  Fx | CAP   A   S   D   F   G   H   J   K   L   ;   '     -    ENT   | HOME UP    END   ← F2
//  Fx | LSH   Z   X   C   V   B   N   M   ,   .   /   RSH   -    -     | LEFT DOWN  RIGHT ← F3
//  -    -    CTR MNU  -  SPC SPC SPC SPC SPC  FN  ALT  -    -    -       -     -     -    ← F4

// Capa 0 – normal
const uint8_t KEY_MAP[ROWS][COLS] = {
 { KC_MACRO_F13, KEY_ESC,        '1',           '2',    '3',    '4',    '5',    '6',
   '7',          '8',            '9',           '0',    '-',    '=',    KEY_BACKSPACE,
   KC_MACRO_F18, KC_MEDIA_VUP,   KC_MEDIA_VDN },

 { KC_MACRO_F14, KEY_TAB,        'q',           'w',    'e',    'r',    't',    'y',
   'u',          'i',            'o',           'p',    '[',    ']',    '\\',
   KEY_DELETE,   KEY_PAGE_UP,    KEY_PAGE_DOWN },

 { KC_MACRO_F15, KEY_CAPS_LOCK,  'a',           's',    'd',    'f',    'g',    'h',
   'j',          'k',            'l',           ';',    '\'',   0x00,   KEY_RETURN,
   KEY_HOME,     KEY_UP_ARROW,   KEY_END },

 { KC_MACRO_F16, KEY_LEFT_SHIFT, 'z',           'x',    'c',    'v',    'b',    'n',
   'm',          ',',            '.',           '/',    KEY_RIGHT_SHIFT, 0x00, 0x00,
   KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_ARROW },

 { 0x00,         0x00,           KEY_LEFT_CTRL, KC_MENU_KEY, 0x00, ' ', ' ', ' ',
   ' ',          ' ',            KC_FN,         KEY_LEFT_ALT, 0x00, 0x00, 0x00,
   0x00,         0x00,           0x00 }
};

// Capa 1 – con Fn
const uint8_t FN_MAP[ROWS][COLS] = {
 // Fn+1=F1 … Fn+0=F10  Fn+-=F11  Fn+==F12
 { 0x00, 0x00,    KEY_F1, KEY_F2, KEY_F3,  KEY_F4,  KEY_F5,  KEY_F6,
   KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, 0x00,
   0x00, 0x00, 0x00 },

 // Fn+Q=slot0  Fn+W=slot1  Fn+E=slot2  Fn+R=pairing  Fn+P=PrintScr
 { 0x00, 0x00, KC_SLOT0, KC_SLOT1, KC_SLOT2, KC_PAIRING, 0x00, 0x00,
   0x00, 0x00, 0x00, KEY_PRINT_SCREEN, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00 },

 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00 },

 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00 },

 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00 }
};

// ════════════════════════════════════════════════════════════════
//  ⑥  OBJETOS HID
// ════════════════════════════════════════════════════════════════

USBHIDKeyboard UsbKeyboard;
BleKeyboard    BleKbd(BLE_DEVICE_NAME, BLE_MANUFACTURER, 100);

// ════════════════════════════════════════════════════════════════
//  ⑦  ESTADO GLOBAL
// ════════════════════════════════════════════════════════════════

bool     keyState[ROWS][COLS]         = {};
bool     rawState[ROWS][COLS]         = {};
uint32_t debounceTimer[ROWS][COLS]    = {};

bool     fnActive       = false;
bool     usbMode        = false;   // true = USB HID activo
bool     pairingMode    = false;
uint8_t  activeSlot     = 0;
uint8_t  savedMAC[NUM_SLOTS][MAC_LEN] = {};
bool     batLow         = false;
bool     batCrit        = false;

uint32_t lastScanUs     = 0;
uint32_t lastBatMs      = 0;
uint32_t lastActivityMs = 0;

// ════════════════════════════════════════════════════════════════
//  ⑧  LED – primitivas y secuencias
// ════════════════════════════════════════════════════════════════

void ledGreen() { digitalWrite(PIN_LED_GREEN, HIGH); digitalWrite(PIN_LED_RED,   LOW);  }
void ledRed()   { digitalWrite(PIN_LED_GREEN, LOW);  digitalWrite(PIN_LED_RED,   HIGH); }
void ledOff()   { digitalWrite(PIN_LED_GREEN, LOW);  digitalWrite(PIN_LED_RED,   LOW);  }

// N parpadeos verdes – confirmación slot (bloqueante, uso puntual)
void ledBlinkGreen(uint8_t n, uint16_t onMs = 120, uint16_t offMs = 120) {
    for (uint8_t i = 0; i < n; i++) {
        ledGreen(); delay(onMs);
        ledOff();   delay(offMs);
    }
}

// N parpadeos rojos – alerta batería (bloqueante, uso puntual)
void ledBlinkRed(uint8_t n, uint16_t onMs = 120, uint16_t offMs = 120) {
    for (uint8_t i = 0; i < n; i++) {
        ledRed();  delay(onMs);
        ledOff();  delay(offMs);
    }
}

// 3 destellos de aviso antes de dormir
void ledSleepWarning() {
    for (uint8_t i = 0; i < 3; i++) {
        batLow ? ledRed() : ledGreen();
        delay(80); ledOff(); delay(80);
    }
}

/**
 * Actualiza el LED en el loop (no bloqueante).
 * Prioridad: pairing > batería crítica > batería baja > normal
 */
void updateLED() {
    static uint32_t lastToggle = 0;
    static bool     toggle     = false;
    uint32_t now = millis();

    if (pairingMode) {
        // Verde parpadeando 4 Hz mientras espera conexión
        if ((now - lastToggle) >= 125) {
            lastToggle = now; toggle = !toggle;
            toggle ? ledGreen() : ledOff();
        }
        return;
    }
    if (batCrit && !usbMode) {
        // Rojo parpadeando 2 Hz en batería crítica
        if ((now - lastToggle) >= 250) {
            lastToggle = now; toggle = !toggle;
            toggle ? ledRed() : ledOff();
        }
        return;
    }
    // Verde en USB (siempre OK) o en BLE con batería buena;
    // rojo en BLE con batería baja
    (batLow && !usbMode) ? ledRed() : ledGreen();
}

// ════════════════════════════════════════════════════════════════
//  ⑨  DETECCIÓN DE MODO USB / BLE
// ════════════════════════════════════════════════════════════════

/**
 * Devuelve true si hay host USB conectado al puerto OTG.
 * En el ESP32-S3, USB.vbusPresent() consulta el estado del
 * pin VBUS interno del periférico USB OTG sin necesitar GPIO.
 */
bool detectUSB() {
    return USB.vbusPresent();
}

// ════════════════════════════════════════════════════════════════
//  ⑩  BATERÍA
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

    batLow  = (mv < BAT_MV_WARN);
    batCrit = (mv < BAT_MV_CRIT);

    // Primera vez que entra en crítico: 5 parpadeos rojos de alerta
    if (batCrit && !prevCrit) {
        ledBlinkRed(5, 80, 80);
        Serial.println("[BAT] *** CRITICA ***");
    }

    // Reportar nivel al host BLE si está conectado
    if (!usbMode && BleKbd.isConnected())
        BleKbd.setBatteryLevel(pct);

    Serial.printf("[BAT] %lu mV  %u%%  [%s]  [%s]\n",
        mv, pct,
        batCrit ? "CRITICA" : batLow ? "BAJA" : "OK",
        usbMode ? "USB" : "BLE");
}

// ════════════════════════════════════════════════════════════════
//  ⑪  EEPROM
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
    for (uint8_t s = 0; s < NUM_SLOTS; s++)
        Serial.printf("  Slot %u: %02X:%02X:%02X:%02X:%02X:%02X%s\n", s,
            savedMAC[s][0], savedMAC[s][1], savedMAC[s][2],
            savedMAC[s][3], savedMAC[s][4], savedMAC[s][5],
            s == activeSlot ? "  <- activo" : "");
}

// ════════════════════════════════════════════════════════════════
//  ⑫  BLE – SLOTS Y PAIRING
// ════════════════════════════════════════════════════════════════

void selectSlot(uint8_t slot) {
    if (slot >= NUM_SLOTS) return;
    activeSlot = slot;
    EEPROM.write(EEPROM_ACTIVE_SLOT, activeSlot);
    EEPROM.commit();
    Serial.printf("[BLE] Slot %u seleccionado\n", activeSlot);
    eepromPrintMACs();
    // N parpadeos verdes = slot+1  (1, 2 o 3 destellos)
    ledBlinkGreen(activeSlot + 1);
    if (!usbMode) { BleKbd.end(); delay(300); BleKbd.begin(); }
}

void enterPairingMode() {
    pairingMode = true;
    Serial.printf("[BLE] Modo pairing → slot %u. Esperando...\n", activeSlot);
    if (!usbMode) { BleKbd.end(); delay(300); BleKbd.begin(); }
}

// Verificar si se ha completado un pairing y guardar la MAC
void checkPairingComplete() {
    if (!pairingMode || !BleKbd.isConnected()) return;
    int count = esp_ble_get_bond_device_num();
    if (count <= 0) return;
    esp_ble_bond_dev_t *list =
        (esp_ble_bond_dev_t *)malloc(count * sizeof(esp_ble_bond_dev_t));
    if (!list) return;
    esp_ble_get_bond_device_list(&count, list);
    memcpy(savedMAC[activeSlot], list[count - 1].bd_addr, MAC_LEN);
    free(list);
    eepromSaveSlot(activeSlot);
    Serial.printf("[BLE] MAC guardada → slot %u: %02X:%02X:%02X:%02X:%02X:%02X\n",
        activeSlot,
        savedMAC[activeSlot][0], savedMAC[activeSlot][1], savedMAC[activeSlot][2],
        savedMAC[activeSlot][3], savedMAC[activeSlot][4], savedMAC[activeSlot][5]);
    pairingMode = false;
    ledBlinkGreen(3, 80, 80);   // 3 parpadeos rápidos = pairing OK
}

// ════════════════════════════════════════════════════════════════
//  ⑬  DEEP SLEEP
// ════════════════════════════════════════════════════════════════

void enterDeepSleep() {
    Serial.printf("[SLEEP] Timeout %lu ms → deep sleep\n", SLEEP_TIMEOUT_MS);
    ledSleepWarning();
    ledOff();

    // Desconectar BLE limpiamente
    if (!usbMode && BleKbd.isConnected()) BleKbd.end();

    // Poner todas las filas en alta impedancia excepto WAKE_ROW (mantener LOW)
    for (uint8_t r = 0; r < ROWS; r++) {
        if (r == WAKE_ROW) {
            pinMode(ROW_PINS[r], OUTPUT);
            digitalWrite(ROW_PINS[r], LOW);
        } else {
            pinMode(ROW_PINS[r], INPUT);
        }
    }

    // Despertar por EXT1 cuando WAKE_COL_PIN baje a LOW (tecla pulsada)
    // GPIO 14 es RTC GPIO en ESP32-S3 (todos los GPIO 0-21 lo son) ✓
    esp_sleep_enable_ext1_wakeup(1ULL << WAKE_COL_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
    delay(100);
    esp_deep_sleep_start();
    // No retorna: al despertar el chip hace reset y ejecuta setup()
}

// ════════════════════════════════════════════════════════════════
//  ⑭  MACROS MULTI-TECLA
// ════════════════════════════════════════════════════════════════

/**
 * Envía una combinación de teclas completa y libera todo.
 * Funciona en modo USB y BLE según usbMode.
 */
void sendMacro(uint8_t macroId) {
    auto press = [](uint8_t k) {
        if (usbMode) UsbKeyboard.press(k);
        else if (BleKbd.isConnected()) BleKbd.press(k);
    };
    auto releaseAll = []() {
        if (usbMode) UsbKeyboard.releaseAll();
        else if (BleKbd.isConnected()) BleKbd.releaseAll();
    };

    switch (macroId) {
        case KC_MACRO_F13:                        // LAlt + Tab
            press(KEY_LEFT_ALT); press(KEY_TAB);
            delay(30); releaseAll(); break;

        case KC_MACRO_F14:                        // Ctrl + C
            press(KEY_LEFT_CTRL); press('c');
            delay(30); releaseAll(); break;

        case KC_MACRO_F15:                        // Ctrl + V
            press(KEY_LEFT_CTRL); press('v');
            delay(30); releaseAll(); break;

        case KC_MACRO_F16:                        // GUI + V
            press(KEY_LEFT_GUI); press('v');
            delay(30); releaseAll(); break;

        case KC_MACRO_F18:                        // Ctrl + Alt + Supr
            press(KEY_LEFT_CTRL);
            press(KEY_LEFT_ALT);
            press(KEY_DELETE);
            delay(30); releaseAll(); break;

        case KC_MEDIA_VUP:                        // Volumen +
            press(KEY_MEDIA_VOLUME_UP);
            delay(30); releaseAll(); break;

        case KC_MEDIA_VDN:                        // Volumen -
            press(KEY_MEDIA_VOLUME_DOWN);
            delay(30); releaseAll(); break;
    }
}

// ════════════════════════════════════════════════════════════════
//  ⑮  ESCANEO DE MATRIZ, DEBOUNCE Y EVENTOS
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

// Envío HID unificado USB/BLE
void hidPress(uint8_t k) {
    if (usbMode)                    UsbKeyboard.press(k);
    else if (BleKbd.isConnected())  BleKbd.press(k);
}

void hidRelease(uint8_t k) {
    if (usbMode)                    UsbKeyboard.release(k);
    else if (BleKbd.isConnected())  BleKbd.release(k);
}

void onKeyEvent(uint8_t row, uint8_t col, bool pressed) {
    uint8_t baseKey = KEY_MAP[row][col];
    uint8_t keycode = fnActive ? FN_MAP[row][col] : baseKey;

    // Tecla Fn: gestionar estado, no genera HID
    if (baseKey == KC_FN) { fnActive = pressed; return; }

    // Tecla MENU
    if (baseKey == KC_MENU_KEY) {
        pressed ? hidPress(KEY_MENU) : hidRelease(KEY_MENU);
        return;
    }

    // Al soltar: liberar teclas HID normales (keycodes < 0xD0)
    if (!pressed) {
        if (keycode != 0x00 && keycode < 0xD0)
            hidRelease(keycode);
        return;
    }

    // Al pulsar: resetear timeout de sleep
    lastActivityMs = millis();

    // Gestión de slots y pairing
    switch (keycode) {
        case KC_SLOT0:    selectSlot(0);       return;
        case KC_SLOT1:    selectSlot(1);       return;
        case KC_SLOT2:    selectSlot(2);       return;
        case KC_PAIRING:  enterPairingMode();  return;
        case 0x00:        return;
        default:          break;
    }

    // Macros multi-tecla (rango 0xD0-0xDF)
    if (keycode >= 0xD0 && keycode <= 0xDF) {
        sendMacro(keycode);
        return;
    }

    // Tecla HID normal
    hidPress(keycode);
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
    // Serial por USB CDC (no ocupa GPIOs físicos en el S3)
    Serial.begin(115200);
    delay(500);   // Dar tiempo al CDC para que el host lo detecte
    Serial.println("\n=== Teclado 5x18 Dual HID – ESP32-S3-DevKitC-1 N16R8 ===");

    // Pines de matriz
    for (uint8_t r = 0; r < ROWS; r++) {
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], HIGH);
    }
    for (uint8_t c = 0; c < COLS; c++)
        pinMode(COL_PINS[c], INPUT_PULLUP);

    // LED bicolor
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED,   OUTPUT);
    ledOff();

    // EEPROM (NVS flash interna)
    EEPROM.begin(EEPROM_SIZE);
    eepromLoad();
    Serial.printf("[EEPROM] Slot activo: %u\n", activeSlot);
    eepromPrintMACs();

    // Inicializar USB HID (siempre activo, el S3 lo gestiona)
    UsbKeyboard.begin();
    USB.begin();

    // Inicializar BLE HID
    BleKbd.begin();

    // Detectar modo inicial
    delay(200);   // Esperar a que USB.vbusPresent() sea estable
    usbMode = detectUSB();
    Serial.printf("[MODO] %s\n", usbMode ? "USB HID" : "BLE HID");

    // Batería inicial
    checkBattery();

    // Inicializar temporizadores
    lastActivityMs = millis();
    lastBatMs      = millis();
    lastScanUs     = micros();

    // Destello de inicio: 1 verde largo
    ledBlinkGreen(1, 300, 0);
    updateLED();

    Serial.printf("[SLEEP] Timeout: %lu ms\n", SLEEP_TIMEOUT_MS);
    Serial.printf("[WAKE]  GPIO %u (ROW %u, COL %u)\n",
                  WAKE_COL_PIN, WAKE_ROW, WAKE_COL);
}

// ════════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════════

void loop() {
    uint32_t nowUs = micros();
    uint32_t nowMs = millis();

    // ── Escaneo de matriz (cada 500 µs) ──────────────────────
    if ((nowUs - lastScanUs) >= SCAN_INTERVAL_US) {
        lastScanUs = nowUs;
        scanMatrix();
        processDebounce();
    }

    // ── Batería + re-detección de modo (cada 30 s) ───────────
    if ((nowMs - lastBatMs) >= BAT_CHECK_MS) {
        lastBatMs = nowMs;

        // Detectar si cambió el estado del USB
        bool wasUsb = usbMode;
        usbMode = detectUSB();
        if (usbMode != wasUsb) {
            Serial.printf("[MODO] Cambio → %s\n", usbMode ? "USB HID" : "BLE HID");
            // Si vuelve a BLE, reiniciar anuncio
            if (!usbMode) { BleKbd.end(); delay(200); BleKbd.begin(); }
        }

        checkBattery();

        // Comprobar si se completó un pairing pendiente
        if (!usbMode) checkPairingComplete();
    }

    // ── Deep sleep por inactividad (solo en BLE) ─────────────
    // En USB no dormimos: el cable alimenta y el host espera eventos
    if (!usbMode && (nowMs - lastActivityMs) >= SLEEP_TIMEOUT_MS)
        enterDeepSleep();   // No retorna

    // ── Actualizar LED ────────────────────────────────────────
    updateLED();
}
