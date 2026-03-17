#include <Keypad.h>
#include <BleKeyboard.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

// Definición personalizada para la tecla FN (fuera del rango HID estándar)
#define KEY_FN 0xF0

/* ==================================================  CONFIGURACIÓN DE MATRIZ ================================================== */
const byte FILAS = 5;
const byte COLUMNAS = 18;

// Mapeo de teclas
byte teclas[FILAS][COLUMNAS] = {
  {KEY_F13, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE, KEY_F17, KEY_F18, KEY_F19},
  {KEY_F14, KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\', KEY_DELETE, KEY_PAGE_UP, KEY_PAGE_DOWN},
  {KEY_F15, KEY_CAPS_LOCK, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', KEY_RETURN, KEY_HOME, KEY_UP_ARROW, KEY_END, 0},
  {KEY_F16, KEY_LEFT_SHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_RIGHT_SHIFT, KEY_LEFT_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_ARROW, 0, 0},
  {KEY_LEFT_CTRL, KEY_LEFT_ALT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KEY_RIGHT_ALT, KEY_FN, 0, 0, 0}
};

// Pines de la matriz
byte pinesFilas[FILAS] = {36, 39, 34, 35, 32};
byte pinesColumnas[COLUMNAS] = {13, 14, 16, 17, 18, 19, 21, 22, 23, 27,   33, 4, 2, 5, 15, 0, 12, 8};
/* ============================================================================================================================ */




/* =============================================  CONFIGURACIÓN DE BATERÍA Y LED =============================================== */
#define PIN_LED_ROJO     25  // GPIO 25 para rojo
#define PIN_LED_VERDE     26  // GPIO 25 para verde
#define PIN_BATERIA       34  // GPIO 34 para leer voltaje (ADC1_CH6)

// Parámetros de la batería Li-Po / Li-ion 3.7V 2400mAh
const float VOLTAJE_MAX_BATERIA = 4.2;      // Batería completamente cargada
const float VOLTAJE_NOMINAL = 3.7;          // Voltaje nominal de la batería
const float VOLTAJE_MIN_SEGURO = 3.0;       // Mínimo absoluto (nunca bajar de aquí)

// Umbrales de advertencia
const float VOLTAJE_BAJO = 3.5;              // 30-40% de capacidad restante
const float VOLTAJE_MIN_BATERIA = 3.3;        // 10-15% de capacidad restante
const float VOLTAJE_CRITICO = 3.1;            // 5% de capacidad restante - ¡URGENTE!
const float VOLTAJE_PELIGRO = 3.05;           // 2-3% - Apagado inminente
const float VOLTAJE_USB_CONECTADO = 4.05;     // Umbral para detectar USB conectado

// Factor del divisor de tensión (R1=100kΩ, R2=27kΩ)
const float DIVISOR_FACTOR = 4.7;

// Para calibración ADC
const int NUM_MUESTRAS = 64;
esp_adc_cal_characteristics_t adc_caracteristicas;

// Variables para modos de parpadeo
int modoParpadeo = 0; // 0: normal, 1: bajo, 2: crítico, 3: peligro
const unsigned long INTERVALO_PARPADEO_NORMAL = 500;  // 500ms para batería baja
const unsigned long INTERVALO_PARPADEO_CRITICO = 200; // 200ms para batería crítica
const unsigned long INTERVALO_PARPADEO_PELIGRO = 100; // 100ms para peligro extremo
/* ============================================================================================================================ */




/* ==================================================  VARIABLES DE ESTADO ================================================== */
bool fnActivo = false;
unsigned long ultimaPulsacionFN = 0;
const unsigned long TIEMPO_FN = 300;

// Sleep mode
bool enSleep = false;
unsigned long ultimaActividad = 0;
unsigned long tiempoInicioSleep = 0;
bool ignorarTeclas = false;
const unsigned long TIEMPO_SLEEP = 300000; // 5 minutos
const unsigned long TIEMPO_REACTIVACION = 100;

// Monitor de batería
unsigned long ultimaLecturaBateria = 0;
const unsigned long INTERVALO_BATERIA = 5000; // Leer batería cada 5 segundos
float voltajeBateria = 4.0;
bool usbConectado = false;
bool ledRojoState = false;
unsigned long ultimoParpadeo = 0;

// Control de Bluetooth selectivo
bool bluetoothActivo = false;

// Inicializar teclado Bluetooth
BleKeyboard bleKeyboard("Teclado Mecánico Custom", "ESP32", 100);
Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);
/* ============================================================================================================================ */




/* ===========================================  FUNCIONES DE MONITOREO DE BATERÍA ============================================== */

void initBatteryMonitor() {
  // Configurar ADC1
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  
  // Caracterizar ADC
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_caracteristicas);
  
  // Configurar pines del LED
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  
  // Apagar ambos LEDs inicialmente
  digitalWrite(PIN_LED_ROJO, LOW);
  digitalWrite(PIN_LED_VERDE, LOW);
}

float leerVoltajeBateria() {
  uint32_t adc_lectura = 0;
  
  for (int i = 0; i < NUM_MUESTRAS; i++) {
    adc_lectura += adc1_get_raw(ADC1_CHANNEL_6);
    delayMicroseconds(100);
  }
  adc_lectura /= NUM_MUESTRAS;
  
  uint32_t voltaje_mv = esp_adc_cal_raw_to_voltage(adc_lectura, &adc_caracteristicas);
  float voltaje = (voltaje_mv / 1000.0) * DIVISOR_FACTOR;
  
  return voltaje;
}

bool detectarUSBConectado() {
  // Si el voltaje es muy cercano al máximo, probablemente está cargando
  return (voltajeBateria > VOLTAJE_USB_CONECTADO);
}

void gestionarBluetooth() {
  bool usbAhora = detectarUSBConectado();
  
  // Si cambió el estado del USB
  if (usbAhora != usbConectado) {
    usbConectado = usbAhora;
    
    if (usbConectado) {
      // USB conectado - APAGAR Bluetooth (no necesario)
      if (bluetoothActivo) {
        Serial.println("USB detectado - Desactivando Bluetooth para ahorrar energía");
        bleKeyboard.end();  // Apagar Bluetooth
        bluetoothActivo = false;
      }
    } else {
      // USB desconectado - ACTIVAR Bluetooth
      if (!bluetoothActivo) {
        Serial.println("Modo batería - Activando Bluetooth");
        bleKeyboard.begin();
        bluetoothActivo = true;
      }
    }
  }
}

void actualizarLEDs() {
  unsigned long ahora = millis();
  
  // Determinar estado de la batería
  bool cargando = detectarUSBConectado();
  
  if (cargando) {
    // MODO 1: Cargando/USB conectado - Verde fijo
    modoParpadeo = 0;
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
  }
  else if (voltajeBateria <= VOLTAJE_PELIGRO) {
    // MODO 2: Batería en PELIGRO - Parpadeo MUY RÁPIDO en rojo
    modoParpadeo = 3;
    if (ahora - ultimoParpadeo >= INTERVALO_PARPADEO_PELIGRO) {
      ultimoParpadeo = ahora;
      ledRojoState = !ledRojoState;
      digitalWrite(PIN_LED_ROJO, ledRojoState);
      digitalWrite(PIN_LED_VERDE, LOW);
    }
  }
  else if (voltajeBateria <= VOLTAJE_CRITICO) {
    // MODO 3: Batería CRÍTICA - Parpadeo RÁPIDO en rojo
    modoParpadeo = 2;
    if (ahora - ultimoParpadeo >= INTERVALO_PARPADEO_CRITICO) {
      ultimoParpadeo = ahora;
      ledRojoState = !ledRojoState;
      digitalWrite(PIN_LED_ROJO, ledRojoState);
      digitalWrite(PIN_LED_VERDE, LOW);
    }
  }
  else if (voltajeBateria <= VOLTAJE_MIN_BATERIA) {
    // MODO 4: Batería MUY BAJA - Rojo fijo
    modoParpadeo = 0;
    digitalWrite(PIN_LED_ROJO, HIGH);
    digitalWrite(PIN_LED_VERDE, LOW);
  }
  else if (voltajeBateria <= VOLTAJE_BAJO) {
    // MODO 5: Batería BAJA - Parpadeo NORMAL en rojo
    modoParpadeo = 1;
    if (ahora - ultimoParpadeo >= INTERVALO_PARPADEO_NORMAL) {
      ultimoParpadeo = ahora;
      ledRojoState = !ledRojoState;
      digitalWrite(PIN_LED_ROJO, ledRojoState);
      digitalWrite(PIN_LED_VERDE, LOW);
    }
  }
  else {
    // MODO 6: Batería NORMAL - Verde fijo
    modoParpadeo = 0;
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
  }
}
/* ============================================================================================================================ */



/* ==================================================  FUNCIONES DE TECLADO ================================================== */
void manejarTeclaEspecial(byte tecla, bool presionada) {
  if (!presionada) return;
  
  switch(tecla) {
    case KEY_F13: // P1 - Control+C
      bleKeyboard.press(KEY_LEFT_CTRL);
      bleKeyboard.write('c');
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P1: Ctrl+C");
    break;
    case KEY_F14: // P2 - Control+V
      bleKeyboard.press(KEY_LEFT_CTRL);
      bleKeyboard.write('v');
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P2: Ctrl+V");
    break;
    case KEY_F15: // P3 - Control+X
      bleKeyboard.press(KEY_LEFT_CTRL);
      bleKeyboard.write('x');
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P3: Ctrl+X");
    break;
    case KEY_F16: // P4 - WIN + V
      bleKeyboard.press(KEY_LEFT_GUI);
      bleKeyboard.write('v');
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P4: Win+V");
    break;
    case KEY_F17: // P5 - Ctl+Alt+Sup
      bleKeyboard.press(KEY_LEFT_CTRL);
      bleKeyboard.press(KEY_LEFT_ALT);
      bleKeyboard.press(KEY_DELETE);
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P5: Ctl+Alt+Sup");
    break;
    case KEY_F18: // P6 - Volume Up
      bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
      delay(100); Serial.println("P6: vol up");
    break;
    case KEY_F19: // P7 - Volume down
      bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
      delay(100); Serial.println("P7: vol down");
    break;
  }
}

void enviarCombinacionFN(byte tecla) {
  switch(tecla) {
    case '1': bleKeyboard.write(KEY_F1); Serial.println("FN+1: F1"); break;
    case '2': bleKeyboard.write(KEY_F2); Serial.println("FN+2: F2"); break;
    case '3': bleKeyboard.write(KEY_F3); Serial.println("FN+3: F3"); break;
    case '4': bleKeyboard.write(KEY_F4); Serial.println("FN+4: F4"); break;
    case '5': bleKeyboard.write(KEY_F5); Serial.println("FN+5: F5"); break;
    case '6': bleKeyboard.write(KEY_F6); Serial.println("FN+6: F6"); break;
    case '7': bleKeyboard.write(KEY_F7); Serial.println("FN+7: F7"); break;
    case '8': bleKeyboard.write(KEY_F8); Serial.println("FN+8: F8"); break;
    case '9': bleKeyboard.write(KEY_F9); Serial.println("FN+9: F9"); break;
    case '0': bleKeyboard.write(KEY_F10); Serial.println("FN+0: F10"); break;
    case '-': bleKeyboard.write(KEY_F11); Serial.println("FN+-: F11"); break;
    case '=': bleKeyboard.write(KEY_F12); Serial.println("FN+=: F12"); break;
    default: 
      bleKeyboard.write(tecla); 
      break;
  }
  fnActivo = false;
}

void entrarEnSleep() {
  Serial.println("Entrando en modo sleep - 5 minutos sin actividad");
  enSleep = true;
  tiempoInicioSleep = millis();
  
  // Apagar LEDs en sleep
  digitalWrite(PIN_LED_ROJO, LOW);
  digitalWrite(PIN_LED_VERDE, LOW);
  
  // Configurar pines para mínimo consumo
  for (int i = 0; i < COLUMNAS; i++) {
    pinMode(pinesColumnas[i], INPUT_PULLUP);
  }
  for (int i = 0; i < FILAS; i++) {
    pinMode(pinesFilas[i], INPUT_PULLUP);
  }
}

bool hayActividadDuranteSleep() {
  for (int c = 0; c < COLUMNAS; c++) {
    pinMode(pinesColumnas[c], OUTPUT);
    digitalWrite(pinesColumnas[c], LOW);
    delayMicroseconds(10);
    
    for (int f = 0; f < FILAS; f++) {
      if (digitalRead(pinesFilas[f]) == LOW) {
        Serial.println("Actividad detectada - Despertando...");
        for (int i = 0; i < COLUMNAS; i++) {
          pinMode(pinesColumnas[i], OUTPUT);
          digitalWrite(pinesColumnas[i], HIGH);
        }
        return true;
      }
    }
    pinMode(pinesColumnas[c], INPUT_PULLUP);
  }
  return false;
}

void despertar() {
  enSleep = false;
  ignorarTeclas = true;
  ultimaActividad = millis();
  tiempoInicioSleep = millis();
  
  Serial.println("Sistema despertado - Listo para usar");
  
  for (int i = 0; i < COLUMNAS; i++) {
    pinMode(pinesColumnas[i], OUTPUT);
    digitalWrite(pinesColumnas[i], HIGH);
  }
  
  delay(50);
}
/* ============================================================================================================================ */




/* ==================================================  SETUP Y LOOP PRINCIPAL ================================================== */
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando teclado Bluetooth con monitor de batería...");
  
  // Inicializar monitor de batería
  initBatteryMonitor();
  
  // Lectura inicial de batería
  voltajeBateria = leerVoltajeBateria();
  Serial.printf("Voltaje inicial de batería: %.2fV\n", voltajeBateria);
  
  // Detectar estado inicial del USB
  usbConectado = detectarUSBConectado();
  
  if (usbConectado) {
    Serial.println("USB detectado - Bluetooth desactivado (modo ahorro)");
    bluetoothActivo = false;
    // No iniciamos Bluetooth
  } else {
    Serial.println("Modo batería - Activando Bluetooth");
    bleKeyboard.begin();
    bluetoothActivo = true;
  }
  
  ultimaActividad = millis();
}

void loop() {
  unsigned long tiempoActual = millis();
  
  // ===== LECTURA PERIÓDICA DE BATERÍA =====
  if (tiempoActual - ultimaLecturaBateria >= INTERVALO_BATERIA) {
    voltajeBateria = leerVoltajeBateria();
    ultimaLecturaBateria = tiempoActual;
    
    // GESTIONAR BLUETOOTH SEGÚN USB
    gestionarBluetooth();
    
    // Debug cada 30 segundos
    static int contador = 0;
    if (++contador >= 6) {
      contador = 0;
      String estado;
      if (detectarUSBConectado()) estado = "CARGANDO";
      else if (voltajeBateria <= VOLTAJE_PELIGRO) estado = "PELIGRO!";
      else if (voltajeBateria <= VOLTAJE_CRITICO) estado = "CRÍTICO!";
      else if (voltajeBateria <= VOLTAJE_MIN_BATERIA) estado = "MUY BAJO";
      else if (voltajeBateria <= VOLTAJE_BAJO) estado = "BAJO";
      else estado = "NORMAL";
      
      Serial.printf("Batería: %.2fV | Estado: %s | BT: %s\n", 
                    voltajeBateria, 
                    estado.c_str(),
                    bluetoothActivo ? "ACTIVO" : "OFF");
    }
    
    // Actualizar LEDs según estado de batería
    actualizarLEDs();
  }
  
  // ===== GESTIÓN DE SLEEP =====
  if (!enSleep) {
    if (tiempoActual - ultimaActividad >= TIEMPO_SLEEP) {
      entrarEnSleep();
    }
  } else {
    if (hayActividadDuranteSleep()) {
      despertar();
    }
    delay(50);
    return;
  }
  /* ============================================================================================================================ */




  
 /* ==================================================  PROCESAMIENTO DEL TECLADO ================================================ */
  // Solo procesamos teclado si Bluetooth está activo (modo batería)
  // O si estamos en modo USB (no necesitamos Bluetooth para funcionar)
  if (!enSleep) {
    if (ignorarTeclas && tiempoActual - tiempoInicioSleep < TIEMPO_REACTIVACION) {
      teclado.getKeys();
      return;
    } else {
      ignorarTeclas = false;
    }
    
    if (teclado.getKeys()) {
      bool teclaPresionada = false;
      
      for (int i = 0; i < LIST_MAX; i++) {
        if (teclado.key[i].stateChanged) {
          byte tecla = teclado.key[i].kchar;
          bool presionada = teclado.key[i].kstate == PRESSED;
          
          if (presionada) {
            teclaPresionada = true;
            ultimaActividad = tiempoActual;
          }
          
          // Detectar tecla FN (ahora usando KEY_FN = 0xF0)
          if (tecla == KEY_FN) {
            if (presionada) {
              // Detectar doble pulsación para FN lock
              if (tiempoActual - ultimaPulsacionFN < TIEMPO_FN) {
                fnActivo = !fnActivo;
                Serial.println(fnActivo ? "FN Lock ON" : "FN Lock OFF");
              }
              ultimaPulsacionFN = tiempoActual;
            }
            continue; // No enviar la tecla FN como tal
          }
          
          // Manejar teclas de función especiales (P1-P7)
          if (tecla >= KEY_F13 && tecla <= KEY_F19) {
            manejarTeclaEspecial(tecla, presionada);
          }
          // Manejar teclas normales con/sin FN
          else {
            if (presionada) {
              // Si estamos en modo USB, enviamos por USB (Serial)
              // Si estamos en modo Bluetooth, enviamos por BLE
              if (usbConectado) {
                // Modo USB - enviar por Serial (como teclado USB)
                // Nota: Esto requiere que el ESP32 esté configurado como teclado USB
                // Por ahora, como no tenemos esa librería, solo mostramos por Serial
                Serial.write(tecla);
              } else {
                // Modo Bluetooth
                if (fnActivo) {
                  enviarCombinacionFN(tecla);
                } else {
                  bleKeyboard.write(tecla);
                }
              }
            }
          }
        }
      }
      
      if (teclaPresionada) {
        ultimaActividad = tiempoActual;
      }
    }
  }
  
  // Asegurar que los LEDs siguen parpadeando si es necesario
  if (!enSleep && modoParpadeo > 0) {
    actualizarLEDs();
  }
  
  delay(10);
}
/* ============================================================================================================================ */
