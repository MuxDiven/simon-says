#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "debug";

#define COMPONENT_PAIRS 4

// define button led pair
#define LED0 21
#define LED1 33
#define LED2 13
#define LED3 15

#define BUTTON0 22
#define BUTTON1 25
#define BUTTON2 23
#define BUTTON3 19

gpio_config_t LED_conf = {
    .pin_bit_mask = (1ULL << LED0 | 1ULL << LED1 | 1ULL << LED2 | 1ULL << LED3),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type = GPIO_INTR_DISABLE // no interupts for now
};

gpio_config_t BUTTON_conf = {
    .pin_bit_mask =
        (1ULL << BUTTON0 | 1ULL << BUTTON1 | 1ULL << BUTTON2 | 1ULL << BUTTON3),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type = GPIO_INTR_DISABLE // no interupts for now
};

uint8_t LED_VECTOR[] = {LED0, LED1, LED2, LED3};
uint8_t BUTTON_VECTOR[] = {BUTTON0, BUTTON1, BUTTON2, BUTTON3};

#ifdef CONFIG_BLINK_LED_GPIO

// flash light on and off with delay
static void blink_led(int index) {
  ESP_LOGI(TAG, );
  gpio_set_level(LED_VECTOR[index], true);

  vTaskDelay((CONFIG_BLINK_PERIOD >> 1) /
             portTICK_PERIOD_MS); // roughly 1/3 seconds

  ESP_LOGI(TAG, "LED OFF");
  gpio_set_level(LED_VECTOR[index], false);
}

static void configure_io(void) {
  ESP_LOGI(TAG, "CONSTRUCTOR");
  gpio_config(&LED_conf);
  gpio_config(&BUTTON_conf);

  for (int i = 0; i < COMPONENT_PAIRS; i++) {
    gpio_reset_pin(LED_VECTOR[i]);
    gpio_reset_pin(BUTTON_VECTOR[i]);
    gpio_set_direction(LED_VECTOR[i], GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_VECTOR[i], GPIO_MODE_INPUT);
  }
}

static void input_listener() {
  short LED_i = -1; // LED INDEX: -1 for NULL input since NULL ismacro(0)
  //
  ESP_LOGI(TAG, "BUTTON0: %d, BUTTON1: %d, BUTTON2: %d, BUTTON3: %d",
           gpio_get_level(BUTTON0), gpio_get_level(BUTTON1),
           gpio_get_level(BUTTON2), gpio_get_level(BUTTON3));

  // 0 is pull low
  // 1 is pull high
  // depends on the resistor
  // might need a debug print

  // currently grounding with resistors
  if (gpio_get_level(BUTTON0) == 0) {
    LED_i = 0;
  } else if (gpio_get_level(BUTTON1) == 0) {
    LED_i = 1;
  } else if (gpio_get_level(BUTTON2) == 0) {
    LED_i = 2;
  } else if (gpio_get_level(BUTTON3) == 0) {
    LED_i = 3;
    ESP_LOGI(TAG, "bot right button hit");
  }
  ESP_LOGI(TAG, "LED_i: %d", LED_i);

  if (LED_i > -1) {
    blink_led(LED_i);
  }
}

#else
#error "unsupported LED type"
#endif

void app_main(void) {
  /* Configure the peripheral according to the LED type */
  configure_io();

  while (1) {
    input_listener();
    vTaskDelay((CONFIG_BLINK_PERIOD >> 2) /
               portTICK_PERIOD_MS); // roughly 1/3 seconds
  }
}
