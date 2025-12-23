/* Switch demo implementation using button and RGB LED
   
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sdkconfig.h>

#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h> 

#include <app_reset.h>
#include "app_priv.h"

/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

/* This is the GPIO on which the power will be set */
#define OUTPUT_GPIO_OPEN    2
#define OUTPUT_GPIO_STOP    26
#define OUTPUT_GPIO_CLOSE   27

/* Pulse duration */
#define PULSE_DURATION_MS   500

static bool g_power_state = DEFAULT_POWER;

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

static void push_btn_cb(void *arg)
{
    bool new_state = !g_power_state;
    app_driver_set_state(new_state);
#ifdef CONFIG_EXAMPLE_ENABLE_TEST_NOTIFICATIONS
    /* This snippet has been added just to demonstrate how the APIs esp_rmaker_param_update_and_notify()
     * and esp_rmaker_raise_alert() can be used to trigger push notifications on the phone apps.
     * Normally, there should not be a need to use these APIs for such simple operations. Please check
     * API documentation for details.
     */
    if (new_state) {
        esp_rmaker_param_update_and_notify(
                esp_rmaker_device_get_param_by_name(open_garage_device, ESP_RMAKER_DEF_POWER_NAME),
                esp_rmaker_bool(new_state));
    } else {
        esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_name(open_garage_device, ESP_RMAKER_DEF_POWER_NAME),
                esp_rmaker_bool(new_state));
        esp_rmaker_raise_alert("Switch was turned off");
    }
#else
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(open_garage_device, ESP_RMAKER_DEF_POWER_NAME),
            esp_rmaker_bool(new_state));
            
#endif
}

static void pulse_gpio (int gpio_num)
{
    gpio_set_level(gpio_num, 0);
    vTaskDelay(PULSE_DURATION_MS / portTICK_PERIOD_MS);
    gpio_set_level(gpio_num, 1);
}

void app_driver_pulse_open(void)
{
    pulse_gpio(OUTPUT_GPIO_OPEN);
}

void app_driver_pulse_close(void)
{
    pulse_gpio(OUTPUT_GPIO_CLOSE);
}

void app_driver_pulse_stop(void)
{
    pulse_gpio(OUTPUT_GPIO_STOP);
}

static void set_power_state(bool target)
{
    gpio_set_level(OUTPUT_GPIO_OPEN, target);
}

void app_driver_init()
{
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        /* Register a callback for a button tap (short press) event */
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        /* Register Wi-Fi reset and factory reset functionality on same button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }

    /* Configure power */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
    io_conf.pin_bit_mask = ((uint64_t)1 << OUTPUT_GPIO_OPEN) |
                           ((uint64_t)1 << OUTPUT_GPIO_CLOSE) |
                           ((uint64_t)1 << OUTPUT_GPIO_STOP);
    /* Configure the GPIO */
    gpio_config(&io_conf);

    /* Set initial state */
    gpio_set_level(OUTPUT_GPIO_OPEN, 1);
    gpio_set_level(OUTPUT_GPIO_CLOSE, 1);
    gpio_set_level(OUTPUT_GPIO_STOP, 1);
}

int IRAM_ATTR app_driver_set_state(bool state)
{
    if(g_power_state != state) {
        g_power_state = state;
        set_power_state(g_power_state);
    }
    return ESP_OK;
}

bool app_driver_get_state(void)
{
    return g_power_state;
}
