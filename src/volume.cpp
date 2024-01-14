/* Self header */
#include "volume.h"

/* Config */
#include "../cfg/config.h"

/* Project code */
#include "global.h"

/* Arduino Libraries */
#include <Arduino.h>
#include <core/MyMessage.h>
#include <core/MySensorsCore.h>

/* Work variables */
static volatile bool m_volume_received;
static volatile bool m_volume_changed;
static volatile bool m_volume_report_needed_float;
static volatile bool m_volume_report_needed_text;
static volatile float m_volume_base;
static volatile uint32_t m_volume_pulses;
static uint32_t m_volume_millis_sent = UINT32_MAX - CONFIG_VOLUME_REPORTING_INTERVAL;
static enum {
    STATE_0,
    STATE_1,
    STATE_2,
    STATE_3,
    STATE_4,
} m_sm;

/**
 * @brief
 * @return
 */
static bool volume_report_float(void) {

    /* Build float value from base and pulses */
    float value = m_volume_base + (m_volume_pulses * 0.010);

    /* Send volume */
    MyMessage message(SENSOR_1_VOLUME, V_VOLUME);
    if (send(message.set(value, 3)) == true) {
        return true;
    } else {
        Serial.println(" [e] Failed to send message!");
        return false;
    }
}
/**
 * @brief
 * @return
 */
static bool volume_report_text(void) {

    /* Build string value from base and pulses */
    double value = m_volume_base + (m_volume_pulses * 0.010);
    double value_integer_part;
    double value_decimal_part = modf(value, &value_integer_part);
    char string[MAX_PAYLOAD_SIZE + 1] = {0};
    snprintf(string, MAX_PAYLOAD_SIZE, "%d.%03d", (int)value_integer_part, (int)(value_decimal_part * 1000));

    /* Send volume */
    MyMessage message(SENSOR_0_MANUAL, V_TEXT);
    message.set(string);
    if (send(message) == true) {
        return true;
    } else {
        Serial.println(" [e] Failed to send message!");
        return false;
    }
}

/**
 * @brief
 * @param[in] volume
 */
void volume_set(const float volume) {
    m_volume_base = volume;
    m_volume_pulses = 0;
    m_volume_changed = true;
    m_volume_received = true;
    m_volume_report_needed_float = true;
}

/**
 * @brief
 * @param[in] volume
 */
void volume_set(const char* const volume) {
    m_volume_base = strtod(volume, NULL);
    m_volume_pulses = 0;
    m_volume_changed = true;
    m_volume_received = true;
    m_volume_report_needed_float = true;
    m_volume_report_needed_text = true;
}

/**
 * @brief
 * @return
 */
bool volume_increase(void) {

    /* Increment number of pulses */
    m_volume_pulses++;

    /* Flag the value as changed */
    m_volume_changed = true;

    /* Return true if the new value would justify going out of sleep */
    uint32_t millis_since_last_sent = (millis() + g_millis_slept) - m_volume_millis_sent;
    if (millis_since_last_sent > CONFIG_VOLUME_REPORTING_INTERVAL) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief
 * @return
 */
int32_t volume_task(void) {

    /* If the controller needs an update */
    if (m_volume_report_needed_float == true) {
        if (volume_report_float() == true) {
            m_volume_report_needed_float = false;
            m_volume_millis_sent = (millis() + g_millis_slept);
            m_volume_changed = false;
        }
    }
    if (m_volume_report_needed_text == true) {
        if (volume_report_text() == true) {
            m_volume_report_needed_text = false;
        }
    }

    /* State machine for reporting */
    switch (m_sm) {

        case STATE_0: {

            /* Present user with a text input to manually set the initial value */
            MyMessage message(SENSOR_0_MANUAL, V_TEXT);
            if (send(message.set("")) != true) {
                return 1000;
            }

            /* Move on */
            m_sm = STATE_1;
            break;
        }

        case STATE_1: {

            /* Request last value
             * this is done to keep the value incrementing across reboots
             * the response will be processed in the receive() function */
            if (request(SENSOR_1_VOLUME, V_VOLUME) != true) {
                return 1000;
            }

            /* Move on */
            m_sm = STATE_2;
            break;
        }

        case STATE_2: {

            /* Wait for initial value to be retrieved */
            if (m_volume_received != true) {
                return 1000;
            }

            /* Log */
            Serial.println(" [i] Retrieved value from controller.");

            /* Move on */
            m_sm = STATE_3;
            break;
        }

        case STATE_3: {

            /* Wait for volume to have changed */
            if (m_volume_changed != true) {
                return INT32_MAX;
            }

            /* Wait enough time after last report was sent */
            uint32_t millis_since_last_sent = (millis() + g_millis_slept) - m_volume_millis_sent;
            if (millis_since_last_sent < CONFIG_VOLUME_REPORTING_INTERVAL) {
                return CONFIG_VOLUME_REPORTING_INTERVAL - millis_since_last_sent;
            }

            /* Send report */
            if (volume_report_float() == true) {
                m_volume_millis_sent = (millis() + g_millis_slept);
                m_volume_changed = false;
            } else {
                break;
            }

            /* Move on */
            m_sm = STATE_4;
            break;
        }

        case STATE_4: {

            /* If value has changed, go back to the previous state */
            if (m_volume_changed == true) {
                m_sm = STATE_3;
                break;
            }

            /* Wait enough time after last report was sent */
            uint32_t millis_since_last_sent = (millis() + g_millis_slept) - m_volume_millis_sent;
            if (millis_since_last_sent < CONFIG_VOLUME_REPORTING_INTERVAL) {
                return CONFIG_VOLUME_REPORTING_INTERVAL - millis_since_last_sent;
            }

            /* Send report */
            if (volume_report_float() == true) {
                m_volume_millis_sent = (millis() + g_millis_slept);
                m_volume_changed = false;
            } else {
                break;
            }

            /* Move on */
            m_sm = STATE_3;
            break;
        }
    }

    /* Return no sleep */
    return 0;
}
