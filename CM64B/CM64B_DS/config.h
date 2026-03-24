#ifndef CONFIG_H
#define CONFIG_H

#include <Keypad.h>
#include <BleKeyboard.h>


// ========== DEFINICIONES GENERALES ==========
#define KEY_FN 0xF0  // Tecla FN (fuera del rango HID estandar)

// ========== CONFIGURACION DE MATRIZ ==========
const byte FILAS = 5;
const byte COLUMNAS = 18;

// Pines para FILAS (INPUTS)
// GPIO0 necesita resistencia pull-up EXTERNA de 10kΩ a 3.3V
extern byte pinesFilas[FILAS];

// Pines para COLUMNAS (OUTPUTS) - 18 pines
extern byte pinesColumnas[COLUMNAS];

// ========== CONFIGURACION DE BATERIA Y LED ==========
#define PIN_LED_ROJO     25  // GPIO 25 LED rojo
#define PIN_LED_VERDE     26  // GPIO 26 LED verde
#define PIN_BATERIA       34  // GPIO 34 para lectura de voltaje (ADC1_CH6)

// Parametros de la bateria Li-Po 3.7V 2400mAh
const float VOLTAJE_MAX_BATERIA = 4.2;
const float VOLTAJE_NOMINAL = 3.7;
const float VOLTAJE_MIN_SEGURO = 3.0;
const float VOLTAJE_BAJO = 3.5;
const float VOLTAJE_MIN_BATERIA = 3.3;
const float VOLTAJE_CRITICO = 3.1;
const float VOLTAJE_PELIGRO = 3.05;
const float VOLTAJE_USB_CONECTADO = 4.05;

// Factor del divisor de tension (R1=100kΩ, R2=27kΩ)
const float DIVISOR_FACTOR = 4.7;

// Para calibración ADC
const int NUM_MUESTRAS = 64;

// ========== TIEMPOS Y CONFIGURACIONES ==========
const unsigned long TIEMPO_FN = 300;                    // ms para doble pulsacion FN
const unsigned long INTERVALO_BATERIA = 5000;           // Leer bateria cada 5 segundos
const unsigned long INTERVALO_PARPADEO_NORMAL = 500;    // 500ms para bateria baja
const unsigned long INTERVALO_PARPADEO_CRITICO = 200;   // 200ms para bateria crítica
const unsigned long INTERVALO_PARPADEO_PELIGRO = 100;   // 100ms para peligro extremo

// ========== OBJETOS GLOBALES ==========
extern BleKeyboard bleKeyboard;
extern Keypad teclado;

#endif



/* 
COLUMNAS -> OUTPUT
FILAS -> INPUT


FILAS -> GPIO34, GPIO35, GPIO36, GPIO39
COLUMNAS -> GPIO4, GPIO5, GPIO12, GPIO13, GPIO14
			GPIO15, GPIO16, GPIO17, GPIO18, GPIO19
			GPIO21, GPIO22, GPIO23, GPIO25, GPIO26
			GPIO27, GPIO32, GPIO33 



Fila 0: GPIO 36 ←─── Conecta todos los cátodos (raya) de los diodos de la fila superior
Fila 1: GPIO 39 ←─── Cátodos de la segunda fila
Fila 2: GPIO 34 ←─── Cátodos de la tercera fila
Fila 3: GPIO 35 ←─── Cátodos de la cuarta fila
Fila 4: GPIO 0  ←─── Cátodos de la fila inferior (ESPACIO, CTRL, ALT, FN)
        │
        └─ [10kΩ] ─── 3.3V  (¡IMPORTANTE! resistencia pull-up externa
*/
