// ========== CONFIGURACIÓN DE MATRIZ ==========
const byte FILAS = 5;
const byte COLUMNAS = 18;

// Pines para FILAS (entradas) 
// GPIO0 necesita resistencia pull-up EXTERNA de 10kΩ a 3.3V
byte pinesFilas[FILAS] = {36, 39, 34, 35, 0};  // 0 con pull-up externo

// Pines para COLUMNAS (salidas) - 18 pines
byte pinesColumnas[COLUMNAS] = {
  13, 14, 16, 17, 18, 19, 21, 22, 23,  // Pines 100% seguros
  25, 26, 27, 32, 33,                   // Más pines seguros
  4, 2, 5, 15                           // Pines de arranque (funcionan en columnas)
};

// ========== CONFIGURACIÓN DE BATERÍA Y LED ==========
#define PIN_LED_ROJO     25  // GPIO 25 para rojo (no usado en matriz)
#define PIN_LED_VERDE     26  // GPIO 26 para verde (no usado en matriz)
#define PIN_BATERIA       34  // GPIO 34 para batería (solo lectura)


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
