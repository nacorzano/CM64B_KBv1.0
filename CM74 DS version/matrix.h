#ifndef MATRIX_H
#define MATRIX_H

#include "config.h"

// ========== DEFINICION DE PINES ==========
byte pinesFilas[FILAS] = {12, 14, 27, 32, 33};

byte pinesColumnas[COLUMNAS] = {
  13, 16, 17, 18, 19, 21, 22, 23, 25,
  26, 4, 2, 5, 15, 0, 34, 35, 36
};

// ========== MAPA DE TECLAS ==========
const byte teclas[FILAS][COLUMNAS] PROGMEM = {
  // FILA 0 (superior) - P1, ESC, numeros, simbolos, Backspace, P5, P6, P7
  {KEY_F13, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE, KEY_F17, KEY_F18, KEY_F19},
  
  // FILA 1 - P2, Tab, QWERTYUIOP, [ ], \, DEL, PG UP, PG DWN
  {KEY_F14, KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\', KEY_DELETE, KEY_PAGE_UP, KEY_PAGE_DOWN},
  
  // FILA 2 - P3, Caps, ASDFGHJKL, ;, ', Enter, HOME, ↑, END
  {KEY_F15, KEY_CAPS_LOCK, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', KEY_RETURN, KEY_HOME, KEY_UP_ARROW, KEY_END, 0},
  
  // FILA 3 - P4, Shift, ZXCVBNM, ,, ., /, Shift, ←, ↓, →
  {KEY_F16, KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_RIGHT_SHIFT, KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_ARROW, 0, 0},
  
  // FILA 4 (inferior) - CTR, ALT, espacio (vacío), FN, ALT
  {KEY_LEFT_CTRL, KEY_LEFT_ALT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KEY_RIGHT_ALT, KEY_FN, 0, 0, 0}
};

// ========== DIRECCIONES MAC DE DISPOSITIVOS ==========
uint8_t MACAddress[maxdevice][6] = {
  {0x35, 0xAF, 0xA4, 0x07, 0x0B, 0x66},
  {0x31, 0xAE, 0xAA, 0x47, 0x0D, 0x61},
  {0x31, 0xAE, 0xAC, 0x42, 0x0A, 0x31}
};

// ========== FUNCIONES ==========
void changeID(int DevNum) {
  if (DevNum < maxdevice) {
    EEPROM.write(0, DevNum);
    EEPROM.commit();
    // Reinicio suave para aplicar cambios
    esp_restart();
  }
}

// ========== VARIABLES DE ESTADO DEL TECLADO ==========
extern bool fnActivo;
extern unsigned long ultimaPulsacionFN;

// ========== FUNCIONES DEL TECLADO ==========
void manejarTeclaEspecial(byte tecla, bool presionada);
void enviarCombinacionFN(byte tecla);

#endif
