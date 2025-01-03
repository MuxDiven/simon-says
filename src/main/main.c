#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <stdlib.h>

static const char *TAG = "debug";
static const char *FAIL = "GAMEOVER";
#define BUFFER 512
#define ONE_SECOND_DELAY 1000

// game logic variables
int index_vector[BUFFER];
int vector_buffer;
int sequence_length;
#define COMPONENT_PAIRS                                                        \
  4 // NOTE:-> you would have to change code if you change this, this macro will
    // only save you time on declerations and loops
#define STARTING_SEQUENCE COMPONENT_PAIRS

// define (button,led) pair, pin selection may be lazy

#define ONBOARD_LED 2

#define LED0 21
#define LED1 33
#define LED2 13
#define LED3 15

#define BUTTON0 22
#define BUTTON1 25
#define BUTTON2 23
#define BUTTON3 18

// define timer variables
#define TIMER_GROUP TIMER_GROUP_0
#define TIMER_IDX TIMER_0
#define TIMER_DIV 80 // 80Mhz clock divided by 80 = 1Mhz (1 tick = 1us)
#define ALARM_SECONDS 2
#define TIMER_ALARM_TRIGGER ALARM_SECONDS * 1000000 // 2 seconds (us->s)

// config structs -> used in config() function

void configure_timer() {
  timer_config_t TIMER_conf = {.divider = TIMER_DIV,
                               .counter_dir = TIMER_COUNT_UP,
                               .counter_en = TIMER_PAUSE,
                               .alarm_en = TIMER_ALARM_EN,
                               .auto_reload = false};

  timer_init(TIMER_GROUP, TIMER_IDX, &TIMER_conf);
  timer_set_counter_value(TIMER_GROUP, TIMER_IDX, 0);
  timer_set_alarm_value(TIMER_GROUP, TIMER_IDX, TIMER_ALARM_TRIGGER);
  timer_enable_intr(TIMER_GROUP, TIMER_IDX);
}

gpio_config_t LED_conf = {
    .pin_bit_mask = (1ULL << LED0 | 1ULL << LED1 | 1ULL << LED2 | 1ULL << LED3),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type = GPIO_INTR_DISABLE};

gpio_config_t BUTTON_conf = {
    .pin_bit_mask =
        (1ULL << BUTTON0 | 1ULL << BUTTON1 | 1ULL << BUTTON2 | 1ULL << BUTTON3),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE,
    .intr_type = GPIO_INTR_DISABLE};

uint8_t LED_VECTOR[] = {LED0, LED1, LED2, LED3};
uint8_t BUTTON_VECTOR[] = {BUTTON0, BUTTON1, BUTTON2, BUTTON3};

static int random_index() { return arc4random_uniform(COMPONENT_PAIRS); }

// flash light on and off with delay
static void blink_led(int index) {
  gpio_set_level(LED_VECTOR[index], true);
  vTaskDelay((ONE_SECOND_DELAY >> 1) / portTICK_PERIOD_MS);
  gpio_set_level(LED_VECTOR[index], false);
}

static int input_listener() {
  /*-------------------------DEBUG INSTRUCTIONS-----------------------*/
  // NOTE:code:
  // ESP_LOGI(TAG, "BUTTON0: %d, BUTTON1: %d, BUTTON2: %d, BUTTON3: %d",
  //          gpio_get_level(BUTTON0), gpio_get_level(BUTTON1),
  //          gpio_get_level(BUTTON2), gpio_get_level(BUTTON3));

  // 0 is active for pull low
  // 1 is active for pull high
  // depends if resistor is ground or not
  // use the debug print above, labeled code

  // currently grounding with resistors
  // LED INDEX: -1 for NULL input since NULL => "#define NULL 0"
  /*-----------------------------------------------------------------*/
  int8_t LED_i = -1;

  if (gpio_get_level(BUTTON0) == 0) {
    LED_i = 0;
  } else if (gpio_get_level(BUTTON1) == 0) {
    LED_i = 1;
  } else if (gpio_get_level(BUTTON2) == 0) {
    LED_i = 2;
  } else if (gpio_get_level(BUTTON3) == 0) {
    LED_i = 3;
  }

  return LED_i;
}

static void output_sequence() {
  for (int i = 0; i < sequence_length; i++) {
    blink_led(index_vector[i]);
    vTaskDelay((ONE_SECOND_DELAY >> 2) / portTICK_PERIOD_MS);
  }
}

// NOTE:I don't think anyone will reach this but there will be no NULL POINTER
// EXCEPTIONS in this house!!!
static void win_condition() {
  gpio_set_level(ONBOARD_LED, true);
  ESP_LOGI("?????",
           "how long did this take you?\t***you win I guess!***\tFINAL "
           "SCORE:\t%d",
           BUFFER);
  ESP_LOGI("!!!!!", "game will restart in 2 minutes or press EN button on "
                    "the micro controller to restart");
  vTaskDelay((ONE_SECOND_DELAY * 120) / portTICK_PERIOD_MS);
}

static void config(void) {
  ESP_LOGI(TAG, "CONSTRUCTOR");

  // hardware configs
  configure_timer();
  gpio_config(&LED_conf);
  gpio_config(&BUTTON_conf);
  sequence_length = STARTING_SEQUENCE;

  gpio_reset_pin(ONBOARD_LED);
  gpio_set_direction(ONBOARD_LED, GPIO_MODE_OUTPUT);
  gpio_set_level(ONBOARD_LED, true);

  for (int i = 0; i < COMPONENT_PAIRS; i++) {
    gpio_reset_pin(LED_VECTOR[i]);
    gpio_reset_pin(BUTTON_VECTOR[i]);
    gpio_set_direction(LED_VECTOR[i], GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_VECTOR[i], GPIO_MODE_INPUT);
    index_vector[i] = random_index();
  }
}

void app_main(void) {
game:
  int score = 0;
  config();

  while (1) {
    // dynamic memory has issues with interrupts caused by timer, guess this is
    // the win condition
    if (sequence_length == BUFFER) {
      win_condition();
      goto game;
    }
    output_sequence();

    for (int i = 0; i < sequence_length; i++) { // check against all nums
      timer_set_counter_value(TIMER_GROUP, TIMER_IDX,
                              0); // resest the timer if input is successful
      timer_start(TIMER_GROUP, TIMER_IDX);

      bool input_flag = false;
      int input_value;

      while (!input_flag) { // input read state
        uint64_t time_stamp;
        timer_get_counter_value(TIMER_GROUP, TIMER_IDX, &time_stamp);
        input_value = input_listener();

        if (time_stamp >= TIMER_ALARM_TRIGGER) { // timeout condition
          ESP_LOGI(FAIL, "Time out, took to long to input");
          goto fail_cond;
        } else if (input_value > -1) { // input condition
          vTaskDelay((ONE_SECOND_DELAY >> 2) / portTICK_PERIOD_MS);

          input_flag = true;

          if (input_value == index_vector[i]) { // good input
            vTaskDelay((ONE_SECOND_DELAY >> 2) / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "successful input");
            blink_led(input_value);
            timer_pause(TIMER_GROUP, TIMER_IDX);
          } else {
            ESP_LOGI(FAIL, "incorrect input");
            goto fail_cond;
          }
        }
      }
    }

    sequence_length++;
    score++;
    index_vector[sequence_length] = random_index();
    vTaskDelay((ONE_SECOND_DELAY >> 2) / portTICK_PERIOD_MS);
  }

fail_cond:
  while (1) { // NOTE:->limbo
    gpio_set_level(ONBOARD_LED, false);
    ESP_LOGI("FINAL SCORE", "%d", score);
    ESP_LOGI(FAIL, "press the EN button on the micro controller to restart");
    vTaskDelay((ONE_SECOND_DELAY * 60) / portTICK_PERIOD_MS);
  }
}
