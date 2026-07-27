/*
 * pins.h - Pin mapping ESP32 DevKitC V1 & Wire Color Code
 */

#ifndef PINS_H
#define PINS_H

// ====================== POWER LINES ======================
// VCC / 5V / 3.3V  --> MERAH (Red)
// GND              --> HITAM (Black)

// ====================== I2C BUS ======================
constexpr uint8_t PIN_SDA      = 21;  // HIJAU (Green) - SDA
constexpr uint8_t PIN_SCL      = 22;  // KUNING (Yellow) - SCL

// ====================== ANALOG SENSORS ======================
constexpr uint8_t PIN_MQ2_AO   = 32;  // UNGU (Purple) - Gas MQ-2
constexpr uint8_t PIN_FLAME_AO = 33;  // PUTIH (White) - Flame Sensor

// ====================== ACTUATORS ======================
constexpr uint8_t PIN_BUZZER   = 23;  // COKELAT (Brown) - Buzzer Active
constexpr uint8_t PIN_LED_R    = 25;  // ORANYE (Orange) - Red PWM
constexpr uint8_t PIN_LED_G    = 26;  // ABU-ABU (Grey) - Green PWM
constexpr uint8_t PIN_LED_B    = 27;  // BIRU (Blue) - Blue PWM

// ====================== I2C ADDRESSES ======================
constexpr uint8_t I2C_ADDR_AHT20  = 0x38;
constexpr uint8_t I2C_ADDR_BMP280 = 0x77;
constexpr uint8_t I2C_ADDR_LCD    = 0x27;

#endif