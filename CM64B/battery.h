#ifndef BATERIA_H
#define BATERIA_H

#include "config.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>

// ========== VARIABLES GLOBALES ==========
extern float voltajeBateria;
extern bool usbConectado;
extern bool bluetoothActivo;
extern bool ledRojoState;
extern unsigned long ultimoParpadeo;
extern int modoParpadeo;
extern unsigned long ultimaLecturaBateria;

// Características ADC
extern esp_adc_cal_characteristics_t adc_caracteristicas;

// ========== FUNCIONES DE BATERÍA Y LED ==========
void initBatteryMonitor();
float leerVoltajeBateria();
bool detectarUSBConectado();
void gestionarBluetooth();
void actualizarLEDs();

#endif
