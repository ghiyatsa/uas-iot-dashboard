/*
 * pins.h - Pin mapping ESP32 DevKitC V1 & Wire Color Code
 */

#ifndef PINS_H
#define PINS_H

// ====================== POWER LINES ======================
// VCC / 5V / 3.3V  --> MERAH (Red)
// GND              --> HITAM (Black)

// ====================== I2C BUS ======================
#define PIN_SDA       21   // HIJAU (Green) - SDA
#define PIN_SCL       22   // KUNING (Yellow) - SCL

// ====================== ANALOG SENSORS ======================
#define PIN_MQ2_AO    32   // UNGU (Purple) - Gas MQ-2
#define PIN_FLAME_AO  33   // PUTIH (White) - Flame Sensor

// ====================== ACTUATORS ======================
#define PIN_BUZZER    23   // COKELAT (Brown) - Buzzer Active
#define PIN_LED_R     25   // ORANYE (Orange) - Red PWM
#define PIN_LED_G     26   // ABU-ABU (Grey) - Green PWM
#define PIN_LED_B     27   // BIRU (Blue) - Blue PWM

// ====================== I2C ADDRESSES ======================
#define I2C_ADDR_AHT20   0x38
#define I2C_ADDR_BMP280  0x77
#define I2C_ADDR_LCD     0x27

#endif