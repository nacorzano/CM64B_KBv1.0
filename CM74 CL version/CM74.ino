/**
 * ================================================================
 *  Teclado Mecánico BLE HID  –  Matriz 5 × 18
 *  Layout  : Commodore-based ANSI (commodore-based-ansi.json)
 *  Hardware: ESP32 DevKitC v4  (chip ESP32, solo BLE)
 *  Framework: Arduino IDE + ESP32 Arduino Core >= 2.0.0
 *
 *  Librerías requeridas:
 *    · BleKeyboard  → https://github.com/T-vK/ESP32-BLE-Keyboard
 *    · EEPROM.h     → incluida en el core
 * ================================================================
 *
 *  MAPA DE TECLAS ESPECIALES
 *  ─────────────────────────
 *  Teclas macro (columna izquierda y bloque derecho fila 0):
 *    F13 → LAlt + Tab          (cambiar ventana)
 *    F14 → Ctrl + C            (copiar)
 *    F15 → Ctrl + V            (pegar)
 *    F16 → GUI  + V            (portapapeles Windows)
 *    F18 → Ctrl + Alt + Supr   (administrador de tareas)
 *    F19 → Tecla media: Vol+
 *    F20 → Tecla media: Vol-
 *
 *  Capa Fn (Fn mantenida):
 *    Fn + 1..0,-,=  → F1..F12  (teclas de función)
 *    Fn + Q         → Slot BLE 0  (1 parpadeo verde)
 *    Fn + W         → Slot BLE 1  (2 parpadeos verdes)
 *    Fn + E         → Slot BLE 2  (3 parpadeos verdes)
 *    Fn + P         → Print Screen
 *    Fn + R         → Modo Pairing (graba próximo dispositivo)
 *    Fn + flechas   → ya son flechas en capa 0 (bloque derecho)
 *
 *  PINES – ESP32 DevKitC v4
 *  ─────────────────────────
 *  Filas  (OUTPUT, activo LOW): GPIO 13,14,26,27,25
 *  Cols   (INPUT_PULLUP)      : GPIO 2,4,5,15,16,17,18,19,21,
 *                                      22,23,32,33,34,35,36,39 (+GPIO 12 col 3)
 *  LED Verde                  : GPIO  1  ─┐ En producción desactivar
 *  LED Rojo                   : GPIO  3  ─┘ Serial para liberar TX/RX
 *  BAT ADC                    : GPIO 35    (pull-up externo 10kΩ en COLs input-only)
 *  VBUS detect                : GPIO  0    (pull-up externo 10kΩ, no pulsar al boot)
 *
 *  ⚠️  GPIOs input-only (sin pull-up interno): 34, 36, 39
 *      → Necesitan resistencia pull-up externa 10kΩ a 3.3V
 *  ⚠️  GPIO 12 → pull-up externo 10kΩ recomendado (strapping pin)
 *  ⚠️  GPIO 35 → input-only, usado para ADC de batería
 *  ⚠️  GPIO 0  → no pulsar ninguna tecla al arrancar (strapping pin)
 *
 *  EEPROM LAYOUT (20 bytes en NVS flash)
 *  ──────────────────────────────────────
 *   0– 5  MAC slot 0
 *   6–11  MAC slot 1
 *  12–17  MAC slot 2
 *  18     Slot activo (0–2)
 *  19     Magic flag (0xAB = inicializado)
 *
 *  GESTIÓN DE SLOTS BLE
 *  ─────────────────────
 *  Fn + Q  → Conectar slot 0  (1 parpadeo verde)
 *  Fn + W  → Conectar slot 1  (2 parpadeos verdes)
 *  Fn + E  → Conectar slot 2  (3 parpadeos verdes)
 *  Fn + R  → Modo pairing: LED parpadea, graba próximo
 *             dispositivo emparejado en el slot activo
 *
 *  DEEP SLEEP
 *  ───────────
 *  Tras SLEEP_TIMEOUT_MS de inactividad entra en deep sleep.
 *  Despertar: GPIO de la tecla WAKE (fila 4, col 5 = Espacio).
 *  La fila WAKE_ROW se mantiene LOW durante el sueño.
 *  WAKE_COL_PIN debe ser RTC GPIO (rango 0-39 en ESP32).
 *
 * ================================================================
 */

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
const uint8_t ROW_PINS[ROWS] = { 13, 14, 26, 27, 25 };

// Columnas: INPUT_PULLUP (GPIOs 34,36,39 → pull-up externo 10kΩ)
// Col:                 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17
const uint8_t COL_PINS[COLS] = { 2,  4,  5, 12, 15, 16, 17, 18, 19, 21, 22, 23, 32, 33, 34, 36, 39, 0 };
//                                                                                ↑       ↑   ↑   ↑   ↑
//                                          pull-up externo requerido en: ───────────────────────────┘

// LED bicolor (cátodo común, HIGH = encendido)
// ⚠ Comparte TX/RX: desactivar Serial.begin() en producción
#define PIN_LED_GREEN   1   // TX0
#define PIN_LED_RED     3   // RX0

// Batería y VBUS
#define PIN_BAT_ADC    35   // Input-only, ADC
#define PIN_VBUS        0   // Strapping pin – pull-up externo 10kΩ, no pulsar al boot

// Divisor resistivo del ADC de batería (adaptar a tu PCB)
#define BAT_R_HIGH   100000UL   // 100 kΩ
#define BAT_R_LOW     47000UL   //  47 kΩ
#define BAT_MV_MAX     4200UL   // mV carga completa
#define BAT_MV_MIN     3300UL   // mV vacía
#define BAT_MV_WARN    3600UL   // mV → LED rojo
#define BAT_MV_CRIT    3450UL   // mV → alerta crítica

// ════════════════════════════════════════════════════════════════
//  ②  TEMPORIZACIÓN Y SLEEP
// ════════════════════════════════════════════════════════════════

#define DEBOUNCE_MS          10UL
#define SCAN_INTERVAL_US    500UL
#define BAT_CHECK_MS      30000UL

// Tiempo de inactividad antes de deep sleep (ajustar según necesidad)
#define SLEEP_TIMEOUT_MS  300000UL   // 5 minutos

// Tecla de despertar: Espacio → fila 4, col 5 (GP COL_PINS[5] = GPIO 16)
#define WAKE_ROW          4
#define WAKE_COL          5
#define WAKE_COL_PIN      COL_PINS[WAKE_COL]   // GPIO 16 → RTC GPIO ✓

// ════════════════════════════════════════════════════════════════
//  ③  BLE
// ════════════════════════════════════════════════════════════════

#define BLE_DEVICE_NAME   "MiTeclado"
#define BLE_MANUFACTURER  "DIY"
#define NUM_SLOTS          3
#define MAC_LEN            6

// ════════════════════════════════════════════════════════════════
//  ④  EEPROM
// ════════════════════════════════════════════════════════════════

#define EEPROM_SIZE         20
#define EEPROM_SLOT_BASE     0
#define EEPROM_ACTIVE_SLOT  18
#define EEPROM_VALID_FLAG   19
#define EEPROM_MAGIC      0xAB

// ════════════════════════════════════════════════════════════════
//  ⑤  CÓDIGOS INTERNOS (no se envían por HID)
// ════════════════════════════════════════════════════════════════

#define KC_FN        0xF0   // Tecla Fn
#define KC_PAIRING   0xF1   // Fn+R → modo pairing
#define KC_SLOT0     0xE0   // Fn+Q → slot 0
#define KC_SLOT1     0xE1   // Fn+W → slot 1
#define KC_SLOT2     0xE2   // Fn+E → slot 2
// Macros multi-tecla (gestionadas en onKeyEvent)
#define KC_MACRO_F13 0xD0   // LAlt+Tab
#define KC_MACRO_F14 0xD1   // Ctrl+C
#define KC_MACRO_F15 0xD2   // Ctrl+V
#define KC_MACRO_F16 0xD3   // GUI+V
#define KC_MACRO_F18 0xD4   // Ctrl+Alt+Supr
#define KC_MEDIA_VUP 0xC0   // Vol+
#define KC_MEDIA_VDN 0xC1   // Vol-

// ════════════════════════════════════════════════════════════════
//  ⑥  MAPAS DE TECLAS
// ════════════════════════════════════════════════════════════════
//
//  Índices de columna física:
//  C0  C1   C2  C3  C4  C5  C6  C7  C8  C9  C10 C11 C12   C13 C14   C15   C16   C17
//  Fx| ESC  1   2   3   4   5   6   7   8   9   0   -     =   BKSP  F18   F19   F20   ← Fila 0
//  Fx| TAB  Q   W   E   R   T   Y   U   I   O   P   [     ]   \     DEL   PGUP  PGDN  ← Fila 1
//  Fx| CAP  A   S   D   F   G   H   J   K   L   ;   '     -   ENT   HOME  UP    END   ← Fila 2
//  Fx| LSH  Z   X   C   V   B   N   M   ,   .   /   RSH   -   -     LEFT  DOWN  RIGHT ← Fila 3
//  -   -   CTR  MNU  -  SPC SPC SPC SPC SPC  FN  ALT  -   -   -     -     -     -     ← Fila 4

// Capa 0 – pulsación normal
const uint8_t KEY_MAP[ROWS][COLS] = {
// C0            C1             C2    C3    C4    C5    C6    C7    C8    C9
// C10   C11   C12          C13          C14             C15           C16           C17
 { KC_MACRO_F13, KEY_ESC,       '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',
   '9',  '0',  '-',         '=',         KEY_BACKSPACE,  KC_MACRO_F18, KC_MEDIA_VUP, KC_MEDIA_VDN },

 { KC_MACRO_F14, KEY_TAB,       'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
   'o',  'p',  '[',         ']',         '\\',           KEY_DELETE,   KEY_PAGE_UP,  KEY_PAGE_DOWN },

 { KC_MACRO_F15, KEY_CAPS_LOCK, 'a',  's',  'd',  'f',  'g',  'h',  'j',  'k',
   'l',  ';',  '\'',        0x00,        KEY_RETURN,     KEY_HOME,     KEY_UP_ARROW, KEY_END },

 { KC_MACRO_F16, KEY_LEFT_SHIFT,'z',  'x',  'c',  'v',  'b',  'n',  'm',  ',',
   '.',  '/',  KEY_RIGHT_SHIFT, 0x00,   0x00,           KEY_LEFT_ARROW,KEY_DOWN_ARROW,KEY_RIGHT_ARROW },

 { 0x00,         0x00,          KEY_LEFT_CTRL, 0xFE, 0x00, ' ', ' ', ' ', ' ', ' ',
   KC_FN,        KEY_LEFT_ALT,  0x00,        0x00,        0x00,           0x00,         0x00,         0x00 }
  //                            ↑MENU (0xFE = KEY_MENU ver abajo)
};

// 0xFE lo reasignamos a KEY_MENU en onKeyEvent para no colisionar con internos
#define KC_MENU_KEY  0xFE

// Capa Fn – con Fn mantenida
const uint8_t FN_MAP[ROWS][COLS] = {
// C0    C1    C2    C3    C4    C5    C6    C7    C8    C9    C10   C11   C12   C13   C14   C15   C16   C17
 { 0x00, 0x00, KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_F6,KEY_F7,KEY_F8,
   KEY_F9,KEY_F10,KEY_F11,KEY_F12, 0x00,  0x00,  0x00,  0x00 },
//        ↑ Fn+1=F1 ... Fn+0=F10, Fn+-=F11, Fn+==F12

 { 0x00, 0x00, KC_SLOT0,KC_SLOT1,KC_SLOT2, KC_PAIRING, 0x00, 0x00, 0x00, 0x00,
   0x00, KEY_PRINT_SCREEN, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
//        ↑ Fn+Q=slot0  Fn+W=slot1  Fn+E=slot2  Fn+R=pairing    Fn+P=PrintScr

 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },

 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },

 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

// ════════════════════════════════════════════════════════════════
//  ⑦  OBJETOS Y ESTADO GLOBAL
// ════════════════════════════════════════════════════════════════

BleKeyboard BleKbd(BLE_DEVICE_NAME, BLE_MANUFACTURER, 100);

// Matriz
bool     keyState[ROWS][COLS]          = {};
bool     rawState[ROWS][COLS]          = {};
uint32_t debounceTimer[ROWS][COLS]     = {};

// Estado
bool     fnActive       = false;
bool     pairingMode    = false;
uint8_t  activeSlot     = 0;
uint8_t  savedMAC[NUM_SLOTS][MAC_LEN]  = {};
bool     batLow         = false;
bool     batCrit        = false;

// Tiempos
uint32_t lastScanUs     = 0;
uint32_t lastBatMs      = 0;
uint32_t lastActivityMs = 0;

// ════════════════════════════════════════════════════════════════
//  ⑧  LED – primitivas y secuencias
// ════════════════════════════════════════════════════════════════

void ledGreen() { digitalWrite(PIN_LED_GREEN, HIGH); digitalWrite(PIN_LED_RED,   LOW);  }
void ledRed()   { digitalWrite(PIN_LED_GREEN, LOW);  digitalWrite(PIN_LED_RED,   HIGH); }
void ledOff()   { digitalWrite(PIN_LED_GREEN, LOW);  digitalWrite(PIN_LED_RED,   LOW);  }

// N parpadeos verdes bloqueantes – confirmación de slot (N = slot+1)
void ledBlinkGreen(uint8_t n, uint16_t onMs = 120, uint16_t offMs = 120) {
    for (uint8_t i = 0; i < n; i++) {
        ledGreen(); delay(onMs);
        ledOff();   delay(offMs);
    }
}

// N parpadeos rojos bloqueantes – alerta de batería crítica
void ledBlinkRed(uint8_t n, uint16_t onMs = 120, uint16_t offMs = 120) {
    for (uint8_t i = 0; i < n; i++) {
        ledRed();  delay(onMs);
        ledOff();  delay(offMs);
    }
}

// 3 destellos rápidos antes de dormir
void ledSleepWarning() {
    for (uint8_t i = 0; i < 3; i++) {
        batLow ? ledRed() : ledGreen();
        delay(80); ledOff(); delay(80);
    }
}

/**
 * Actualiza LED en el loop (no bloqueante).
 * Prioridad: pairing > batería crítica > batería baja > normal
 */
void updateLED() {
    static uint32_t lastToggle = 0;
    static bool     toggle     = false;
    uint32_t now = millis();

    if (pairingMode) {
        // Verde parpadeando 4 Hz
        if ((now - lastToggle) >= 125) {
            lastToggle = now; toggle = !toggle;
            toggle ? ledGreen() : ledOff();
        }
        return;
    }
    if (batCrit) {
        // Rojo parpadeando 2 Hz
        if ((now - lastToggle) >= 250) {
            lastToggle = now; toggle = !toggle;
            toggle ? ledRed() : ledOff();
        }
        return;
    }
    batLow ? ledRed() : ledGreen();
}

// ════════════════════════════════════════════════════════════════
//  ⑨  BATERÍA
// ════════════════════════════════════════════════════════════════

uint32_t readBatteryMv() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 8; i++) { sum += analogReadMilliVolts(PIN_BAT_ADC); delay(1); }
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

    if (batCrit && !prevCrit) {
        ledBlinkRed(5, 80, 80);   // Alerta crítica al entrar por primera vez
        Serial.println("[BAT] *** CRITICA ***");
    }

    if (BleKbd.isConnected()) BleKbd.setBatteryLevel(pct);

    Serial.printf("[BAT] %lu mV  %u%%  [%s]\n",
        mv, pct, batCrit ? "CRITICA" : batLow ? "BAJA" : "OK");
}

// ════════════════════════════════════════════════════════════════
//  ⑩  EEPROM
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
//  ⑪  BLE – SLOTS Y PAIRING
// ════════════════════════════════════════════════════════════════

void selectSlot(uint8_t slot) {
    activeSlot = slot;
    EEPROM.write(EEPROM_ACTIVE_SLOT, activeSlot);
    EEPROM.commit();
    Serial.printf("[BLE] Slot %u seleccionado\n", activeSlot);
    eepromPrintMACs();
    ledBlinkGreen(activeSlot + 1);   // 1, 2 o 3 parpadeos
    BleKbd.end(); delay(300); BleKbd.begin();
}

void enterPairingMode() {
    pairingMode = true;
    Serial.printf("[BLE] Pairing activo → slot %u\n", activeSlot);
    BleKbd.end(); delay(300); BleKbd.begin();
}

// Llamar periódicamente cuando BLE está conectado
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
    ledBlinkGreen(3, 80, 80);   // Confirmación: 3 parpadeos rápidos
}

// ════════════════════════════════════════════════════════════════
//  ⑫  DEEP SLEEP
// ════════════════════════════════════════════════════════════════

void enterDeepSleep() {
    Serial.printf("[SLEEP] Inactividad > %lu ms → durmiendo\n", SLEEP_TIMEOUT_MS);
    ledSleepWarning();
    ledOff();

    if (BleKbd.isConnected()) BleKbd.end();

    // Todas las filas en alta impedancia excepto WAKE_ROW (mantener LOW)
    for (uint8_t r = 0; r < ROWS; r++) {
        if (r == WAKE_ROW) {
            pinMode(ROW_PINS[r], OUTPUT);
            digitalWrite(ROW_PINS[r], LOW);
        } else {
            pinMode(ROW_PINS[r], INPUT);
        }
    }

    // Despertar cuando WAKE_COL_PIN baje a LOW (tecla pulsada)
    esp_sleep_enable_ext1_wakeup(1ULL << WAKE_COL_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
    delay(100);
    esp_deep_sleep_start();
    // No retorna – al despertar ejecuta setup() de nuevo
}

// ════════════════════════════════════════════════════════════════
//  ⑬  MACROS MULTI-TECLA
//  Envía combinaciones de teclas y libera todo al terminar.
//  Solo se ejecutan en el evento de pulsación (pressed=true).
// ════════════════════════════════════════════════════════════════

void sendMacro(uint8_t macroId) {
    if (!BleKbd.isConnected()) return;

    switch (macroId) {

        case KC_MACRO_F13:          // LAlt + Tab  →  cambiar ventana
            BleKbd.press(KEY_LEFT_ALT);
            BleKbd.press(KEY_TAB);
            delay(30);
            BleKbd.releaseAll();
            break;

        case KC_MACRO_F14:          // Ctrl + C  →  copiar
            BleKbd.press(KEY_LEFT_CTRL);
            BleKbd.press('c');
            delay(30);
            BleKbd.releaseAll();
            break;

        case KC_MACRO_F15:          // Ctrl + V  →  pegar
            BleKbd.press(KEY_LEFT_CTRL);
            BleKbd.press('v');
            delay(30);
            BleKbd.releaseAll();
            break;

        case KC_MACRO_F16:          // GUI + V  →  portapapeles Windows
            BleKbd.press(KEY_LEFT_GUI);
            BleKbd.press('v');
            delay(30);
            BleKbd.releaseAll();
            break;

        case KC_MACRO_F18:          // Ctrl + Alt + Supr  →  administrador
            BleKbd.press(KEY_LEFT_CTRL);
            BleKbd.press(KEY_LEFT_ALT);
            BleKbd.press(KEY_DELETE);
            delay(30);
            BleKbd.releaseAll();
            break;

        case KC_MEDIA_VUP:          // Volumen +
            BleKbd.press(KEY_MEDIA_VOLUME_UP);
            delay(30);
            BleKbd.releaseAll();
            break;

        case KC_MEDIA_VDN:          // Volumen -
            BleKbd.press(KEY_MEDIA_VOLUME_DOWN);
            delay(30);
            BleKbd.releaseAll();
            break;
    }
}

// ════════════════════════════════════════════════════════════════
//  ⑭  ESCANEO DE MATRIZ Y EVENTOS
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

void onKeyEvent(uint8_t row, uint8_t col, bool pressed) {
    uint8_t baseKey = KEY_MAP[row][col];
    uint8_t keycode = fnActive ? FN_MAP[row][col] : baseKey;

    // ── Tecla Fn ──────────────────────────────────────────────
    if (baseKey == KC_FN) { fnActive = pressed; return; }

    // ── Tecla MENU (código especial 0xFE) ─────────────────────
    if (baseKey == KC_MENU_KEY) {
        if (pressed) BleKbd.press(KEY_MENU);
        else         BleKbd.release(KEY_MENU);
        return;
    }

    // ── Al soltar: liberar teclas HID normales ─────────────────
    if (!pressed) {
        if (keycode != 0x00 && keycode < 0xC0)
            BleKbd.release(keycode);
        return;
    }

    // ── Solo al pulsar ─────────────────────────────────────────
    lastActivityMs = millis();   // Resetear timeout de inactividad

    // Códigos internos de gestión
    switch (keycode) {
        case KC_SLOT0:   selectSlot(0);    return;
        case KC_SLOT1:   selectSlot(1);    return;
        case KC_SLOT2:   selectSlot(2);    return;
        case KC_PAIRING: enterPairingMode(); return;
        case 0x00:       return;
        default:         break;
    }

    // Macros multi-tecla (rango 0xC0-0xDF)
    if (keycode >= 0xC0 && keycode <= 0xDF) {
        sendMacro(keycode);
        return;
    }

    // Tecla HID normal
    if (BleKbd.isConnected()) BleKbd.press(keycode);
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
    // Serial – comentar las dos líneas siguientes en producción
    // para liberar GPIO 1 y GPIO 3 para los LEDs
    Serial.begin(115200);
    Serial.println("\n=== Teclado BLE 5x18 Commodore ANSI ===");

    // Pines de matriz
    for (uint8_t r = 0; r < ROWS; r++) {
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], HIGH);
    }
    for (uint8_t c = 0; c < COLS; c++)
        pinMode(COL_PINS[c], INPUT_PULLUP);
    // Nota: GPIOs 34,36,39 ignoran INPUT_PULLUP → requieren pull-up externo 10kΩ

    // LED y VBUS
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED,   OUTPUT);
    pinMode(PIN_VBUS,      INPUT);
    ledOff();

    // EEPROM
    EEPROM.begin(EEPROM_SIZE);
    eepromLoad();
    Serial.printf("[EEPROM] Slot activo: %u\n", activeSlot);
    eepromPrintMACs();

    // BLE
    BleKbd.begin();
    Serial.println("[BLE] Anunciando...");

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
    Serial.printf("[WAKE]  GPIO %u (fila %u, col %u)\n",
                  WAKE_COL_PIN, WAKE_ROW, WAKE_COL);
}

// ════════════════════════════════════════════════════════════════
//  LOOP
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

    // ── Batería cada 30 s ────────────────────────────────────
    if ((nowMs - lastBatMs) >= BAT_CHECK_MS) {
        lastBatMs = nowMs;
        checkBattery();
        checkPairingComplete();
    }

    // ── Timeout de inactividad → deep sleep ──────────────────
    if ((nowMs - lastActivityMs) >= SLEEP_TIMEOUT_MS)
        enterDeepSleep();   // No retorna

    // ── LED ───────────────────────────────────────────────────
    updateLED();
}
