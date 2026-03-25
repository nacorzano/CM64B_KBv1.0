/**
 * Teclado Mecanico Custom basado en el teclado de Commodore Vic20 programado con ESP32
 * Autor: NACH Corp
 * Descripción: Teclado Bluetooth con matriz 5x18, monitor de batería y LED indicador
 */

#include "config.h"
#include "matrix.h"
#include "battery.h"

// ========== DEFINICION DE OBJETOS GLOBALES ==========
BleKeyboard bleKeyboard("CM64B Keyboard", "NACH Corp", 100);
Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// ========== VARIABLES GLOBALES ==========
// Variables de estado del teclado
bool fnActivo = false;
unsigned long ultimaPulsacionFN = 0;

// Variables de bateria
float voltajeBateria = 4.0;
bool usbConectado = false;
bool bluetoothActivo = false;
bool ledRojoState = false;
unsigned long ultimoParpadeo = 0;
int modoParpadeo = 0;
unsigned long ultimaLecturaBateria = 0;

// Caracteristicas ADC
esp_adc_cal_characteristics_t adc_caracteristicas;

// ========== FUNCIONES DEL TECLADO ==========
void manejarTeclaEspecial(byte tecla, bool presionada) {
  if (!presionada) return;
  
  switch(tecla) {
    case KEY_F13: // P1 - L_ALT+Tab
      bleKeyboard.press(KEY_LEFT_ALT); bleKeyboard.press(KEY_TAB);
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P1: ALT+Tab");
      break;
    case KEY_F14: // P2 - Control+C
      bleKeyboard.press(KEY_LEFT_CTRL); bleKeyboard.write('c');
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P2: Ctrl+C");
      break;
    case KEY_F15: // P3 - Control+V
      bleKeyboard.press(KEY_LEFT_CTRL); bleKeyboard.write('v');
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P3: Ctrl+V");
      break;
    case KEY_F16: // P4 - Win+V
      bleKeyboard.press(KEY_LEFT_GUI); bleKeyboard.write('v');
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P4: Win+V");
      break;
    case KEY_F17: // // P5 - Ctl+Alt+Sup
      bleKeyboard.press(KEY_LEFT_CTRL); bleKeyboard.press(KEY_LEFT_ALT); bleKeyboard.press(KEY_DELETE);
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P5: Ctl+Alt+Sup");
      break;
    case KEY_F18: // P6 - // P6 - Volume Up
      bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P6: vol up");
      break;
    case KEY_F19: // / P7 - Volume down
      bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
      delay(100); bleKeyboard.releaseAll();
      Serial.println("P7: vol down");
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

// ========== FUNCIONES DE BATERIA ==========
void initBatteryMonitor() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_caracteristicas);
  
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
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
  return (voltaje_mv / 1000.0) * DIVISOR_FACTOR;
}

bool detectarUSBConectado() {
  return (voltajeBateria > VOLTAJE_USB_CONECTADO);
}

void gestionarBluetooth() {
  bool usbAhora = detectarUSBConectado();
  
  if (usbAhora != usbConectado) {
    usbConectado = usbAhora;
    
    if (usbConectado) {
      if (bluetoothActivo) {
        Serial.println("USB detectado - Bluetooth OFF");
        bleKeyboard.end();
        bluetoothActivo = false;
      }
    } else {
      if (!bluetoothActivo) {
        Serial.println("Modo batería - Bluetooth ON");
        bleKeyboard.begin();
        bluetoothActivo = true;
      }
    }
  }
}

void actualizarLEDs() {
  unsigned long ahora = millis();
  bool cargando = detectarUSBConectado();
  
  if (cargando) {
    modoParpadeo = 0;
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
  }
  else if (voltajeBateria <= VOLTAJE_PELIGRO) {
    modoParpadeo = 3;
    if (ahora - ultimoParpadeo >= INTERVALO_PARPADEO_PELIGRO) {
      ultimoParpadeo = ahora;
      ledRojoState = !ledRojoState;
      digitalWrite(PIN_LED_ROJO, ledRojoState);
      digitalWrite(PIN_LED_VERDE, LOW);
    }
  }
  else if (voltajeBateria <= VOLTAJE_CRITICO) {
    modoParpadeo = 2;
    if (ahora - ultimoParpadeo >= INTERVALO_PARPADEO_CRITICO) {
      ultimoParpadeo = ahora;
      ledRojoState = !ledRojoState;
      digitalWrite(PIN_LED_ROJO, ledRojoState);
      digitalWrite(PIN_LED_VERDE, LOW);
    }
  }
  else if (voltajeBateria <= VOLTAJE_MIN_BATERIA) {
    modoParpadeo = 0;
    digitalWrite(PIN_LED_ROJO, HIGH);
    digitalWrite(PIN_LED_VERDE, LOW);
  }
  else if (voltajeBateria <= VOLTAJE_BAJO) {
    modoParpadeo = 1;
    if (ahora - ultimoParpadeo >= INTERVALO_PARPADEO_NORMAL) {
      ultimoParpadeo = ahora;
      ledRojoState = !ledRojoState;
      digitalWrite(PIN_LED_ROJO, ledRojoState);
      digitalWrite(PIN_LED_VERDE, LOW);
    }
  }
  else {
    modoParpadeo = 0;
    digitalWrite(PIN_LED_ROJO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  Serial.println(F("========================================"));
  Serial.println(F("  Teclado Mecanico CM64B con ESP32"));
  Serial.println(F("  Iniciando sistema..."));
  Serial.println(F("========================================"));

  //Adapted from GitHub, alongside the MAC address code
  //Allows the keyboard to connect to multiple devices, and "remembers" what device it was connected to
  EEPROM.begin(4);                                      //Begin EEPROM, allow us to store
  int deviceChose = EEPROM.read(0);                     //Read selected address from storage
// Validar que el valor leído sea válido
  if (deviceChose >= maxdevice || deviceChose < 0) {
    deviceChose = 0;  // Valor por defecto
    EEPROM.write(0, deviceChose);
    EEPROM.commit();
  }
 /// Configurar MAC address ANTES de iniciar Bluetooth
// Usando la API moderna para ESP32 v3.x
esp_err_t err = esp_iface_mac_addr_set(&MACAddress[deviceChose][0], ESP_MAC_BT);
if (err == ESP_OK) {
    Serial.print("MAC configurada correctamente: ");
    for (int i = 0; i < 6; i++) {
        Serial.print(MACAddress[deviceChose][i], HEX);
        if (i < 5) Serial.print(":");
    }
    Serial.println();
} else {
    Serial.print("Error configurando MAC: ");
    Serial.println(err);
}
  
  // Configuración especial para GPIO0
  //pinMode(0, INPUT_PULLUP);  // Necesita resistencia EXTERNA de 10kΩ a 3.3V

  // Configurar pines de fila con pull-up interno (todos lo tienen)
  for (int i = 0; i < FILAS; i++) {
    pinMode(pinesFilas[i], INPUT_PULLUP);
  }
  
  // Configurar pines de columna como salidas
  for (int i = 0; i < COLUMNAS; i++) {
    pinMode(pinesColumnas[i], OUTPUT);
    digitalWrite(pinesColumnas[i], HIGH);
  }
  
  // Inicializar monitor de bateria
  initBatteryMonitor();
  
  // Lectura inicial de bateria
  voltajeBateria = leerVoltajeBateria();
  Serial.print(F("Voltaje inicial: "));
  Serial.print(voltajeBateria);
  Serial.println(F("V"));
  
  // Detectar USB
  usbConectado = detectarUSBConectado();
  
  if (usbConectado) {
    Serial.println(F("USB detectado - Modo ahorro (Bluetooth OFF)"));
    bluetoothActivo = false;
  } else {
    Serial.println(F("Modo batería - Activando Bluetooth..."));
    bleKeyboard.begin();
    bluetoothActivo = true;
    Serial.println(F("Bluetooth activado. Busca 'Teclado Mecánico Custom'"));
  }
  
  Serial.println(F("¡Teclado listo para usar!"));
  Serial.println(F("========================================"));
}

// ============================================================ LOOP PRINCIPAL ============================================================
void loop() {
  unsigned long tiempoActual = millis();
  
  // ===== LECTURA DE BATERIA (cada 5 segundos) =====
  if (tiempoActual - ultimaLecturaBateria >= INTERVALO_BATERIA) {
    voltajeBateria = leerVoltajeBateria();
    ultimaLecturaBateria = tiempoActual;
    gestionarBluetooth();
    actualizarLEDs();
    
    // Debug cada 30 segundos
    static int contador = 0;
    if (++contador >= 6) {
      contador = 0;
      String estado;
      if (detectarUSBConectado()) estado = "CARGANDO";
      else if (voltajeBateria <= VOLTAJE_PELIGRO) estado = "PELIGRO";
      else if (voltajeBateria <= VOLTAJE_CRITICO) estado = "CRITICO";
      else if (voltajeBateria <= VOLTAJE_MIN_BATERIA) estado = "MUY BAJO";
      else if (voltajeBateria <= VOLTAJE_BAJO) estado = "BAJO";
      else estado = "NORMAL";
      
      Serial.print(F("Batería: "));
      Serial.print(voltajeBateria);
      Serial.print(F("V | "));
      Serial.print(estado);
      Serial.print(F(" | BT: "));
      Serial.println(bluetoothActivo ? "ON" : "OFF");
    }
  }
  
  // ===== PROCESAMIENTO DEL TECLADO =====
  if (bluetoothActivo && bleKeyboard.isConnected()) {
    if (teclado.getKeys()) {
      for (int i = 0; i < LIST_MAX; i++) {
        if (teclado.key[i].stateChanged) {
          byte tecla = teclado.key[i].kchar;
          bool presionada = teclado.key[i].kstate == PRESSED;
          
          // Detectar tecla FN
          if (tecla == KEY_FN) {
            if (presionada) {
              if (tiempoActual - ultimaPulsacionFN < TIEMPO_FN) {
                fnActivo = !fnActivo;
                Serial.println(fnActivo ? "FN Lock ON" : "FN Lock OFF");
              }
              ultimaPulsacionFN = tiempoActual;
            }
            continue;
          }
          
          // Teclas especiales P1-P7
          if (tecla >= KEY_F13 && tecla <= KEY_F19) {
            manejarTeclaEspecial(tecla, presionada);
          }
          // Teclas normales
          else if (presionada) {
            if (fnActivo) {
              enviarCombinacionFN(tecla);
            } else {
              bleKeyboard.write(tecla);
              // Debug: mostrar tecla pulsada
              if (tecla >= 32 && tecla <= 126) {
                Serial.print(F("Tecla: "));
                Serial.println((char)tecla);
              }
            }
          }
        }
      }
    }
  }
  
  // Mantener parpadeo de LEDs
  if (modoParpadeo > 0) {
    actualizarLEDs();
  }
  
  delay(10);
}
