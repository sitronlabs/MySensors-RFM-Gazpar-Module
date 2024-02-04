/* Config */
#include "../cfg/config.h"

/* Project code */
#include "battery.h"
#include "global.h"
#include "volume.h"

/* Arduino Libraries */
#include <Arduino.h>
#include <MySensors.h>

/* C/C++ libraries */
#include <ctype.h>
#include <stdlib.h>

/* Work variables */
static volatile bool m_received;

/**
 * Setup function.
 * Called once MySensors has successfully initialized.
 */
void setup(void) {

    /* Setup serial */
    Serial.begin(115200);
    Serial.println(" [i] Hello world.");

    /* Setup battery reading */
    analogReference(INTERNAL);
    analogRead(CONFIG_BATTERY_PIN);

    /* Setup pulse input */
    pinMode(CONFIG_VOLUME_PULSE_PIN, INPUT);
}

/**
 * MySensors function called to describe this sensor and its capabilites.
 */
void presentation(void) {

    /* Because messages might be lost,
     * we're not doing the presentation in one block, but rather step by step,
     * making sure each step is sucessful before advancing to the next */
    for (int8_t step = -1;;) {

        /* Send out presentation information corresponding to the current step,
         * and advance one step if successful */
        switch (step) {
            case -1: {
                if (sendSketchInfo(F("SLHA00028 Gazpar"), F("0.4.0")) == true) {
                    step++;
                }
                break;
            }
            case SENSOR_0_MANUAL: {
                if (present(SENSOR_0_MANUAL, S_INFO, F("Relevé manuel")) == true) {
                    step++;
                }
                break;
            }
            case SENSOR_1_VOLUME: {
                if (present(SENSOR_1_VOLUME, S_GAS, F("Volume")) == true) {
                    step++;
                }
                break;
            }
            default: {
                return;
            }
        }

        /* Sleep a little bit after each presentation, otherwise the next fails
         * @see https://forum.mysensors.org/topic/4450/sensor-presentation-failure */
        sleep(50);
    }
}

/**
 * MySensors function called when a message is received.
 */
void receive(const MyMessage &message) {

    /* Set flag */
    m_received = true;

    /* Handle user manual input initial value */
    if (message.sensor == SENSOR_0_MANUAL && message.getType() == V_TEXT) {
        char input[MAX_PAYLOAD_SIZE + 1] = {0};
        strncpy(input, message.getString(), MAX_PAYLOAD_SIZE);
        for (uint8_t i = 0; i < MAX_PAYLOAD_SIZE + 1; i++) {
            if (input[i] == ',') {
                input[i] = '.';
            }
        }
        volume_set(input);
    }

    /* Handle value sent by controller */
    else if (message.sensor == SENSOR_1_VOLUME && message.getType() == V_VOLUME) {
        volume_set(message.getFloat());
    }
}

/**
 * Main loop.
 */
void loop(void) {
    int32_t time_sleep_initial = INT32_MAX;
    int32_t time_sleep_left = INT32_MAX;

    /* Call volume reporting task */
    int32_t task_res = volume_task();
    if (task_res < time_sleep_initial) {
        time_sleep_initial = task_res;
    }

    /* Call battery reporting task */
    task_res = battery_task();
    if (task_res < time_sleep_initial) {
        time_sleep_initial = task_res;
    }

    /* Sleep if possible
     * Note, smartsleep is implemented manually because otherwise the pusle interrupt would cause mysensors to send two messages systematically */
    if (time_sleep_initial > 0) {

        /* Notify gateway of our intention to sleep */
        _msgTmp = build(_msgTmp, GATEWAY_ADDRESS, NODE_SENSOR_ID, C_INTERNAL, I_PRE_SLEEP_NOTIFICATION);
        _msgTmp.set((uint32_t)MY_SMART_SLEEP_WAIT_DURATION_MS);
        _sendRoute(_msgTmp);

        /* Listen for incoming messages before sleeping
         * and cancel sleep if we receiv a message */
        wait(MY_SMART_SLEEP_WAIT_DURATION_MS);
        if (m_received == true) {
            time_sleep_initial = 0;
            m_received = false;
        }

        /* Sleep */
        for (time_sleep_left = time_sleep_initial; time_sleep_left > 0;) {
            uint8_t sleep_interrupt = digitalPinToInterrupt(CONFIG_VOLUME_PULSE_PIN);
            int8_t sleep_res = sleep(sleep_interrupt, FALLING, time_sleep_left, false);

            /* If woken up by a pulse */
            if (sleep_res == sleep_interrupt) {

                /* Retrieve an estimate of how much time we have slept before the pin interrupt triggered,
                 * but because getSleepRemaining will often think we have slept more than we actually did,
                 * by an order of 8192ms, we substract half that amount */
                uint32_t time_slept = time_sleep_left - getSleepRemaining();
                if (time_slept > 4096) {
                    time_slept -= 4096;
                } else {
                    time_slept = 0;
                }
                g_millis_slept += time_slept;
                time_sleep_left -= time_slept;

                /* Increase volume value, and go out of sleep if the new value justifies it */
                if (volume_increase() == true) {
                    break;
                }
            }

            /* If woken up by the timer */
            else if (sleep_res == MY_WAKE_UP_BY_TIMER) {
                g_millis_slept += time_sleep_left;
                time_sleep_left = 0;
            }
        };

        /* Notify controller about waking up, payload indicates sleeping time in ms */
        _msgTmp = build(_msgTmp, GATEWAY_ADDRESS, NODE_SENSOR_ID, C_INTERNAL, I_POST_SLEEP_NOTIFICATION);
        _msgTmp.set(time_sleep_initial - time_sleep_left);
        _sendRoute(_msgTmp);
    }
}
