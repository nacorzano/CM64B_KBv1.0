#ifndef MATRIZ_H
#define MATRIZ_H

#include "config.h"

// ========== DEFINICION DE PINES ==========
// Pines para FILAS (INPUT)
byte pinesFilas[FILAS] = {36, 39, 34, 35, 0};

// Pines para COLUMNAS (OUTPUT) - 18 pines
byte pinesColumnas[COLUMNAS] = {
  13, 14, 16, 17, 18, 19, 21, 22, 23,  
  25, 26, 27, 32, 33,                   
  4, 2, 5, 15                           
};

// ========== MAPA DE TECLAS ==========
// Basado en keyboard-layout-editor.com
const byte teclas[FILAS][COLUMNAS] PROGMEM = {
  // FILA 0 (superior) - P1, ESC, números, símbolos, Backspace, P5, P6, P7
  {KEY_F13, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE, KEY_F17, KEY_F18, KEY_F19},
  
  // FILA 1 - P2, Tab, QWERTYUIOP, [ ], \, DEL, PG UP, PG DWN
  {KEY_F14, KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\', KEY_DELETE, KEY_PAGE_UP, KEY_PAGE_DOWN},
  
  // FILA 2 - P3, Caps, ASDFGHJKL, ;, ', Enter, HOME, ↑, END
  {KEY_F15, KEY_CAPS_LOCK, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', KEY_RETURN, KEY_HOME, KEY_UP_ARROW, KEY_END, 0},
  
  // FILA 3 - P4, Shift, ZXCVBNM, ,, ., /, Shift, ←, ↓, →
  {KEY_F16, KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_RIGHTSHIFT, KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_ARROW, 0, 0},
  
  // FILA 4 (inferior) - CTR, ALT, espacio (vacío), FN, ALT
  {KEY_LEFT_CTRL, KEY_LEFT_ALT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KEY_RIGHT_ALT, KEY_FN, 0, 0, 0}
};

// ========== VARIABLES DE ESTADO DEL TECLADO ==========
extern bool fnActivo;
extern unsigned long ultimaPulsacionFN;

// ========== FUNCIONES DEL TECLADO ==========
void manejarTeclaEspecial(byte tecla, bool presionada);
void enviarCombinacionFN(byte tecla);

#endif
