// victron_records.h
#pragma once
#include <stdint.h>

// Victron manufacturer ID
#define VICTRON_MANUFACTURER_ID 0x02E1

// Maximum encrypted Victron BLE data payload size
#define VICTRON_ENCRYPTED_DATA_MAX_SIZE 21

// ---------------------------------------------------------------------------
// Record Type Enum
// ---------------------------------------------------------------------------
typedef enum {
    VICTRON_BLE_RECORD_TEST              = 0x00,
    VICTRON_BLE_RECORD_SOLAR_CHARGER     = 0x01,
    VICTRON_BLE_RECORD_BATTERY_MONITOR   = 0x02,
    VICTRON_BLE_RECORD_INVERTER          = 0x03,
    VICTRON_BLE_RECORD_DCDC_CONVERTER    = 0x04,
    VICTRON_BLE_RECORD_SMART_LITHIUM     = 0x05,
    VICTRON_BLE_RECORD_INVERTER_RS       = 0x06,
    VICTRON_BLE_RECORD_AC_CHARGER        = 0x08,
    VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT = 0x09,
    VICTRON_BLE_RECORD_LYNX_SMART_BMS    = 0x0A,
    VICTRON_BLE_RECORD_MULTI_RS          = 0x0B,
    VICTRON_BLE_RECORD_VE_BUS            = 0x0C,
    VICTRON_BLE_RECORD_DC_ENERGY_METER   = 0x0D,
    VICTRON_BLE_RECORD_ORION_XS          = 0x0F,
} victron_record_type_t;

// ---------------------------------------------------------------------------
// Common Enums (simplified from ESPHome for C use)
// ---------------------------------------------------------------------------

// Device State
typedef enum {
    VIC_STATE_OFF = 0x00,
    VIC_STATE_LOW_POWER = 0x01,
    VIC_STATE_FAULT = 0x02,
    VIC_STATE_BULK = 0x03,
    VIC_STATE_ABSORPTION = 0x04,
    VIC_STATE_FLOAT = 0x05,
    VIC_STATE_STORAGE = 0x06,
    VIC_STATE_EQUALIZE = 0x07,
    VIC_STATE_POWER_SUPPLY = 0x0B,
    VIC_STATE_NOT_AVAILABLE = 0xFF
} victron_device_state_t;

// Charger Error
typedef enum {
    VIC_ERR_NONE = 0,
    VIC_ERR_BAT_TEMP_HIGH = 1,
    VIC_ERR_BAT_VOLT_HIGH = 2,
    VIC_ERR_REMOTE_TEMP_SENSOR = 3,
    VIC_ERR_REMOTE_BAT_SENSE = 6,
    VIC_ERR_HIGH_RIPPLE = 11,
    VIC_ERR_TEMP_LOW = 14,
    VIC_ERR_TEMP_CHARGER = 17,
    VIC_ERR_OVER_CURRENT = 18,
    VIC_ERR_POLARITY = 19,
    VIC_ERR_OVERHEATED = 26,
    VIC_ERR_SHORT_CIRCUIT = 27,
    VIC_ERR_INPUT_VOLT_HIGH = 33,
    VIC_ERR_INPUT_CURR_HIGH = 34,
    VIC_ERR_INPUT_SHUTDOWN = 38,
    VIC_ERR_CPU_TEMP = 114,
    VIC_ERR_CALIBRATION_LOST = 116,
    VIC_ERR_UNKNOWN = 0xFF
} victron_error_code_t;

// Alarm reasons (bitmask)
typedef enum {
    VIC_ALARM_NONE              = 0x0000,
    VIC_ALARM_LOW_VOLTAGE       = 0x0001,
    VIC_ALARM_HIGH_VOLTAGE      = 0x0002,
    VIC_ALARM_LOW_SOC           = 0x0004,
    VIC_ALARM_LOW_TEMP          = 0x0020,
    VIC_ALARM_HIGH_TEMP         = 0x0040,
    VIC_ALARM_OVERLOAD          = 0x0100,
    VIC_ALARM_DC_RIPPLE         = 0x0200,
    VIC_ALARM_SHORT_CIRCUIT     = 0x1000,
    VIC_ALARM_BMS_LOCKOUT       = 0x2000
} victron_alarm_reason_t;

// ---------------------------------------------------------------------------
// Record Structs (packed)
// ---------------------------------------------------------------------------

// 0x01 - SmartSolar / BlueSolar MPPT
typedef struct __attribute__((packed)) {
    uint8_t  device_state;          // see victron_device_state_t
    uint8_t  charger_error;         // see victron_error_code_t
    int16_t  battery_voltage_centi; // 0.01 V
    int16_t  battery_current_deci;  // 0.1 A
    uint16_t yield_today_centikwh;  // 0.01 kWh
    uint16_t pv_power_w;            // 1 W
    int16_t  load_current_deci;     // 0.1 A
} victron_record_solar_charger_t;

// 0x02 - Battery Monitor (BMV / SmartShunt)
typedef struct __attribute__((packed)) {
    uint16_t time_to_go_minutes;      // 1 min
    uint16_t battery_voltage_centi;   // 0.01 V
    uint16_t alarm_reason;            // bitmask
    uint16_t aux_value;               // depends on aux_input
    uint8_t  aux_input;               // 0=voltage2,1=mid,2=temp
    int32_t  battery_current_milli;   // 0.001 A
    int32_t  consumed_ah_deci;        // 0.1 Ah (negative=discharge)
    uint16_t soc_deci_percent;        // 0.1 %
} victron_record_battery_monitor_t;

// 0x03 - Inverter
typedef struct __attribute__((packed)) {
    uint8_t  device_state;
    uint16_t alarm_reason;
    int16_t  battery_voltage_centi;
    uint16_t ac_apparent_power_va;
    uint16_t ac_voltage_centi;    // 0.01 V
    uint16_t ac_current_deci;     // 0.1 A
} victron_record_inverter_t;

// 0x04 - DC/DC Converter (Orion)
typedef struct __attribute__((packed)) {
    uint8_t  device_state;
    uint8_t  charger_error;
    uint16_t input_voltage_centi;
    uint16_t output_voltage_centi;
    uint32_t off_reason;
} victron_record_dcdc_converter_t;

// 0x05 - SmartLithium Battery
typedef struct __attribute__((packed)) {
    uint32_t bms_flags;
    uint16_t error_flags;
    uint8_t  cell1_centi; // 0.01 V encoded (7-bit)
    uint8_t  cell2_centi;
    uint8_t  cell3_centi;
    uint8_t  cell4_centi;
    uint8_t  cell5_centi;
    uint8_t  cell6_centi;
    uint8_t  cell7_centi;
    uint8_t  cell8_centi;
    uint16_t battery_voltage_centi : 12;
    uint8_t  balancer_status : 4;
    uint8_t  temperature_c : 7; // raw + offset -40°C
} victron_record_smart_lithium_t;

// 0x08 - AC Charger (Phoenix IP43)
typedef struct __attribute__((packed)) {
    uint8_t  device_state;
    uint8_t  charger_error;
    uint16_t battery_voltage_1_centi : 13;
    uint16_t battery_current_1_deci : 11;
    uint16_t battery_voltage_2_centi : 13;
    uint16_t battery_current_2_deci : 11;
    uint16_t battery_voltage_3_centi : 13;
    uint16_t battery_current_3_deci : 11;
    int8_t   temperature_c;
    uint16_t ac_current_deci : 9;
} victron_record_ac_charger_t;

// 0x09 - Smart Battery Protect
typedef struct __attribute__((packed)) {
    uint8_t  device_state;
    uint8_t  output_state;
    uint8_t  error_code;
    uint16_t alarm_reason;
    uint16_t warning_reason;
    uint16_t input_voltage_centi;
    uint16_t output_voltage_centi;
    uint32_t off_reason;
} victron_record_smart_battery_protect_t;

// 0x0A - Lynx Smart BMS
typedef struct __attribute__((packed)) {
    uint8_t  error;
    uint16_t time_to_go_min;
    uint16_t battery_voltage_centi;
    int16_t  battery_current_deci;
    uint16_t io_status;
    uint32_t warnings_alarms : 18;
    uint16_t soc_deci_percent : 10;
    int32_t  consumed_ah_deci : 20;
    int8_t   temperature_c;
} victron_record_lynx_smart_bms_t;


// 0x0B - Multi RS
typedef struct __attribute__((packed)) {
    uint8_t  device_state;
    uint8_t  charger_error;
    int16_t  battery_current_deci;
    uint16_t battery_voltage_centi : 14;
    uint8_t  active_ac_in : 2;
    uint16_t active_ac_in_power_w;
    uint16_t active_ac_out_power_w;
    uint16_t pv_power_w;
    uint16_t yield_today_centikwh;
} victron_record_multi_rs_t;

// 0x0C - VE.Bus Inverter/Charger
typedef struct __attribute__((packed)) {
    uint8_t  device_state;
    uint8_t  ve_bus_error;
    int16_t  battery_current_deci;
    uint16_t battery_voltage_centi : 14;
    uint8_t  active_ac_in : 2;
    uint32_t active_ac_in_power_w : 19;
    uint32_t ac_out_power_w : 19;
    uint8_t  alarm_state : 2;
    int8_t   battery_temp_c;
    uint8_t  soc_percent;
} victron_record_ve_bus_t;

// 0x0D - DC Energy Meter
typedef struct __attribute__((packed)) {
    int16_t  monitor_mode;
    int16_t  battery_voltage_centi;
    uint16_t alarm_reason;
    uint16_t aux_value;
    uint8_t  aux_input;
    int32_t  battery_current_milli;
} victron_record_dc_energy_meter_t;

// 0x0F - Orion XS DC/DC Converter
typedef struct __attribute__((packed)) {
    uint8_t  device_state;
    uint8_t  charger_error;
    uint16_t output_voltage_centi;
    uint16_t output_current_deci;
    uint16_t input_voltage_centi;
    uint16_t input_current_deci;
    uint32_t off_reason;
} victron_record_orion_xs_t;

// ---------------------------------------------------------------------------
// Unified container (optional convenience)
// ---------------------------------------------------------------------------
typedef struct {
    victron_record_type_t type;
    union {
        victron_record_solar_charger_t       solar;
        victron_record_battery_monitor_t     battery;
        victron_record_inverter_t            inverter;
        victron_record_dcdc_converter_t      dcdc;
        victron_record_smart_lithium_t       lithium;
        victron_record_ac_charger_t          ac_charger;
        victron_record_smart_battery_protect_t sbp;
        victron_record_lynx_smart_bms_t      lynx;
        victron_record_multi_rs_t            multi;
        victron_record_ve_bus_t              vebus;
        victron_record_dc_energy_meter_t     dcem;
        victron_record_orion_xs_t            orion;
        uint8_t raw[VICTRON_ENCRYPTED_DATA_MAX_SIZE];
    };
} victron_record_t;

