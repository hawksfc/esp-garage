#include <sdkconfig.h>
#include <esp_log.h> // Necesario para logs
#include <iot_button.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h> 

#include <app_reset.h>
#include "app_priv.h"

static const char *TAG = "app_driver";

/* Botón BOOT para reset */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0

/* --- CONFIGURACIÓN DE PINES --- */
// NOTA: Cambié OPEN al 25 para dejar el 2 libre para tu Pantalla TFT
#define OUTPUT_GPIO_OPEN    25  
#define OUTPUT_GPIO_CLOSE   26
#define OUTPUT_GPIO_STOP    27

// Sensores de Final de Carrera
#define GPIO_SENSOR_OPEN    13
#define GPIO_SENSOR_CLOSE   14

/* Duración del pulso del relé */
#define PULSE_DURATION_MS   500

static bool g_power_state = DEFAULT_POWER;

#define WIFI_RESET_BUTTON_TIMEOUT       3
#define FACTORY_RESET_BUTTON_TIMEOUT    10

/* --- FUNCIONES DE LECTURA DE SENSORES --- */
// Devuelve true si el portón está tocando el sensor de ABIERTO
bool app_driver_is_open(void)
{
    // Como usamos Pull-up, si el sensor cierra circuito a tierra, leemos 0
    return gpio_get_level(GPIO_SENSOR_OPEN) == 0;
}

// Devuelve true si el portón está tocando el sensor de CERRADO
bool app_driver_is_closed(void)
{
    return gpio_get_level(GPIO_SENSOR_CLOSE) == 0;
}

static void push_btn_cb(void *arg)
{
    bool new_state = !g_power_state;
    app_driver_set_state(new_state);
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(open_garage_device, ESP_RMAKER_DEF_POWER_NAME),
            esp_rmaker_bool(new_state));
}

static void pulse_gpio (int gpio_num)
{
    gpio_set_level(gpio_num, 0); // Activa Relé (Asumiendo módulo activo bajo)
    vTaskDelay(PULSE_DURATION_MS / portTICK_PERIOD_MS);
    gpio_set_level(gpio_num, 1); // Apaga Relé
}

/* --- COMANDOS CON SEGURIDAD --- */

void app_driver_pulse_open(void)
{
    if (app_driver_is_open()) {
        ESP_LOGW(TAG, "SEGURIDAD: El porton ya esta ABIERTO. Ignorando comando.");
        return; // ¡NO activamos el relé!
    }
    ESP_LOGI(TAG, "Abriendo porton...");
    pulse_gpio(OUTPUT_GPIO_OPEN);
}

void app_driver_pulse_close(void)
{
    if (app_driver_is_closed()) {
        ESP_LOGW(TAG, "SEGURIDAD: El porton ya esta CERRADO. Ignorando comando.");
        return; // ¡NO activamos el relé!
    }
    ESP_LOGI(TAG, "Cerrando porton...");
    pulse_gpio(OUTPUT_GPIO_CLOSE);
}

void app_driver_pulse_stop(void)
{
    ESP_LOGI(TAG, "Deteniendo porton...");
    pulse_gpio(OUTPUT_GPIO_STOP);
}

static void set_power_state(bool target)
{
    gpio_set_level(OUTPUT_GPIO_OPEN, target);
}

void app_driver_init()
{
    /* 1. Configurar Botón Boot */
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, NULL);
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }

    /* 2. Configurar RELÉS (Salidas) */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1, // Para mantenerlos apagados al inicio si son active low
    };
    io_conf.pin_bit_mask = ((uint64_t)1 << OUTPUT_GPIO_OPEN) |
                           ((uint64_t)1 << OUTPUT_GPIO_CLOSE) |
                           ((uint64_t)1 << OUTPUT_GPIO_STOP);
    gpio_config(&io_conf);

    /* 3. Configurar SENSORES (Entradas con Pull-up) */
    // Esto es vital: Habilita las resistencias internas para conectar directo a GND
    gpio_config_t sensor_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE, // Sin interrupciones por ahora
        .pin_bit_mask = ((uint64_t)1 << GPIO_SENSOR_OPEN) | 
                        ((uint64_t)1 << GPIO_SENSOR_CLOSE)
    };
    gpio_config(&sensor_conf);

    /* Estado Inicial Relés (Apagados/High) */
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