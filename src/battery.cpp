/* Self header */
#include "battery.h"

/* Config */
#include "../cfg/config.h"

/* Project code */
#include "global.h"

/* Arduino Libraries */
#include <Arduino.h>
#include <core/MyMessage.h>
#include <core/MySensorsCore.h>

/* Work variables */
static uint32_t m_battery_millis_sent = UINT32_MAX - CONFIG_BATTERY_REPORTING_INTERVAL;

/**
 * @brief
 * @return
 */
int32_t battery_task(void) {

    /* Wait enough time after last report was sent */
    uint32_t millis_since_last_sent = (millis() + g_millis_slept) - m_battery_millis_sent;
    if (millis_since_last_sent < CONFIG_BATTERY_REPORTING_INTERVAL) {
        return CONFIG_BATTERY_REPORTING_INTERVAL - millis_since_last_sent;
    }

    /* Compute battery average ratio */
    double adc_voltage = (1.1 * analogRead(CONFIG_BATTERY_PIN)) / 1023.0;
    double bat_voltage = (adc_voltage * (CONFIG_BATTERY_VOLTAGE_DIVIDER_R1 + CONFIG_BATTERY_VOLTAGE_DIVIDER_R2)) / CONFIG_BATTERY_VOLTAGE_DIVIDER_R2;
    double bat_ratio = (bat_voltage - 3.2) / (4.2 - 3.2);
    if (bat_ratio < 0) {
        bat_ratio = 0;
    } else if (bat_ratio > 1) {
        bat_ratio = 1;
    }

    /* Report battery level */
    uint8_t bat_pct = bat_ratio * 100;
    if (sendBatteryLevel(bat_pct) == true) {
        m_battery_millis_sent = millis() + g_millis_slept;
        return CONFIG_BATTERY_REPORTING_INTERVAL;
    } else {
        return 1000;
    }
}
