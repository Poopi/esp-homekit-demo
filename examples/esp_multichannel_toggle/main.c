/*
 * Example of using esp-homekit library to control
 * a simple $5 Sonoff Basic using HomeKit.
 * The esp-wifi-config library is also used in this
 * example. This means you don't have to specify
 * your network's SSID and password before building.
 *
 * In order to flash the sonoff basic you will have to
 * have a 3,3v (logic level) FTDI adapter.
 *
 * To flash this example connect 3,3v, TX, RX, GND
 * in this order, beginning in the (square) pin header
 * next to the button.
 * Next hold down the button and connect the FTDI adapter
 * to your computer. The sonoff is now in flash mode and
 * you can flash the custom firmware.
 *
 * WARNING: Do not connect the sonoff to AC while it's
 * connected to the FTDI adapter! This may fry your
 * computer and sonoff.
 *
 updated SSID to use full MAC address
 updated HomeKit name to end in last 3 bytes of MAC Address
 updated HomeKit passcode to valid value
 updated HomeKit identify pulses to be heartbeat timing
 added toggle switch via toggle.c based on button.c
 enabled GPIO14 as input and use as toggle
 */

#include <stdio.h>
#include <espressif/esp_misc.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <homekit/../../src/debug.h>
#include <wifi_config.h>
#include "button.h"
#include "toggle.h"

#define NO_LONG_PRESS_RESET_CONFIG  2
#define NO_CHARACTERISTICS          4
#define RELAY_ACTIVE_LOW

const int led_gpio            = 0;
const int config_button_gpio  = 14;

// Callbacks
void toggle_callback(uint8_t gpio);
void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void button_callback(uint8_t gpio, button_event_t event);
void config_button_callback(uint8_t gpio, button_event_t event);

//Definitions
typedef struct {
  bool                      default_val;
  homekit_characteristic_t  switch_on;
  int                       relay_gpio;
  int                       toggle_gpio;
  int                       button_gpio;
} homekit_config_t;

homekit_config_t homekit_config[NO_CHARACTERISTICS] = {
  {
    false,
    HOMEKIT_CHARACTERISTIC_(
      ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
    ),
     0, 4, -1
  },
  {
    false,
    HOMEKIT_CHARACTERISTIC_(
      ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
    ),
     2, 5, -1
  },
  {
    false,
    HOMEKIT_CHARACTERISTIC_(
      ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
    ),
    15,12, -1
  },
  {
    false,
    HOMEKIT_CHARACTERISTIC_(
      ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
    ),
    16,13, -1
  }
};


void relay_write(int gpio, bool on) {
#ifdef RELAY_ACTIVE_LOW
  gpio_write(gpio, on ? 0 : 1);
#else
    gpio_write(gpio, on ? 1 : 0);
#endif
}

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}

void led_blink(unsigned int time, unsigned char no_blink) {
  for (unsigned char i=0; i<no_blink; i++) {
      led_write(true);
      vTaskDelay((time/2) / portTICK_PERIOD_MS);
      led_write(false);
      vTaskDelay((time/2) / portTICK_PERIOD_MS);
  }
}

void reset_configuration_task() {
    //Flash the LED first before we start the reset
    led_blink(200,3);
    INFO("Resetting Wifi Config\n");
    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    INFO("Resetting HomeKit Config\n");
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    INFO("Restarting\n");
    sdk_system_restart();
    vTaskDelete(NULL);
}

void reset_configuration() {
    INFO("Resetting Sonoff configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}


void gpio_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(false);

    for(int i=0;i<NO_CHARACTERISTICS;i++) {
      if(homekit_config[i].relay_gpio!=-1){
        gpio_enable(homekit_config[i].relay_gpio, GPIO_OUTPUT);
        relay_write(homekit_config[i].relay_gpio, homekit_config[i].switch_on.value.bool_value);
      }
      if(homekit_config[i].toggle_gpio!=-1){
        gpio_enable(homekit_config[i].toggle_gpio, GPIO_INPUT);
      }

      if(homekit_config[i].button_gpio!=-1){
        gpio_enable(homekit_config[i].button_gpio, GPIO_INPUT);
      }
    }
    gpio_enable(config_button_gpio, GPIO_INPUT);
}

void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    //uint32_t ctx = (uint32_t) context;
    int idx = -1;
    for(idx=0;idx<NO_CHARACTERISTICS;idx++) {
      if(_ch == &homekit_config[idx].switch_on) {
        break;
      }
    }
    INFO("Switch on for %d = %d",idx,on.bool_value);
    if(idx < NO_CHARACTERISTICS) {
      relay_write(homekit_config[idx].relay_gpio,on.bool_value);
    }
    else {
      DEBUG("Illegal Switch on callback");
    }
}

void led_helper_task(void *pvParam) {
    unsigned int cmd = (uint32_t)pvParam;

    //INFO("Led helper task:%d - %d\n",cmd,cmd>>1);
    if (cmd == 0){
      for (int i=0; i<3; i++) {
          led_blink(400,2);
          vTaskDelay(500 / portTICK_PERIOD_MS);
      }
      led_write(false);
    } else {
      unsigned int no_long_press = cmd >> 1;
      led_blink(2000,no_long_press);
      led_write((cmd&1)?true:false);
    }
    vTaskDelete(NULL);
}

void config_button_callback(uint8_t gpio, button_event_t event) {
  static int no_long_press = 0;

  switch (event) {
    case button_event_single_press:
        INFO("Getting button at GPIO %2d\n", gpio);
        no_long_press = 0;
        break;
    case button_event_long_press:
        INFO("Long press of button at GPIO %2d\n", gpio);
        no_long_press++;
        xTaskCreate(led_helper_task, "Led blink", 128, (void*)(no_long_press<<1), 2, NULL);
        if (no_long_press >= NO_LONG_PRESS_RESET_CONFIG) {
            reset_configuration();
            no_long_press = 0;
        }
        break;
    default:
        INFO("Unknown button event: %d\n", event);
  }
}

void button_callback(uint8_t gpio, button_event_t event) {
    int idx = -1;
    for(idx=0;idx<NO_CHARACTERISTICS;idx++) {
      if(gpio == homekit_config[idx].button_gpio) {
        break;
      }
    }

    if(idx < NO_CHARACTERISTICS) {
      switch (event) {
        case button_event_single_press:
        case button_event_long_press:
            INFO("Toggling relay due to button at GPIO %2d\n", gpio);
            homekit_config[idx].switch_on.value.bool_value = !homekit_config[idx].switch_on.value.bool_value;
            relay_write(homekit_config[idx].relay_gpio, homekit_config[idx].switch_on.value.bool_value);
            homekit_characteristic_notify(&homekit_config[idx].switch_on, homekit_config[idx].switch_on.value);
            break;
        default:
            INFO("Unknown button event: %d\n", event);
      }
    }
    else {
      INFO("Illegal GPIO in button_t callback");
    }
}

void toggle_callback(uint8_t gpio) {
  int idx = -1;
  for(idx=0;idx<NO_CHARACTERISTICS;idx++) {
    if(gpio == homekit_config[idx].toggle_gpio) {
      break;
    }
  }
  if(idx < NO_CHARACTERISTICS) {
    INFO("Toggling relay due to switch at GPIO %2d ( idx = %d)\n", gpio,idx);
    homekit_config[idx].switch_on.value.bool_value = !homekit_config[idx].switch_on.value.bool_value;
    relay_write(homekit_config[idx].relay_gpio,homekit_config[idx].switch_on.value.bool_value);
    homekit_characteristic_notify(&homekit_config[idx].switch_on, homekit_config[idx].switch_on.value);
  }
  else {
    INFO("Illegal GPIO in toggle callback");
  }
}

void switch_identify(homekit_value_t _value) {
    INFO("Switch identify\n");
    xTaskCreate(led_helper_task, "Switch identify", 128, (void*)0, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Switch");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Poopi"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "POOPI000001"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Poopi 4Ch switch"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Switch[1]"),
            &homekit_config[0].switch_on,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Switch[2]"),
            &homekit_config[1].switch_on,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Switch[3]"),
            &homekit_config[2].switch_on,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Switch[4]"),
            &homekit_config[3].switch_on,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-22-333"    //changed tobe valid
//    .password = "111-11-111"    //default easy
};

void on_wifi_ready() {
    INFO("WiFi is ready: Starting Homekit service\n");
    homekit_server_init(&config);
}

void create_accessory_name() {
    uint8_t macaddr[6];
    const char id_str[] = "Poopi Switch %02X:%02X:%02X";
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, id_str,
            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, id_str,
            macaddr[3], macaddr[4], macaddr[5]);

    name.value = HOMEKIT_STRING(name_value);
}


void user_init(void) {
    uart_set_baud(0, 115200);
#if HOMEKIT_DEBUG
    for(int i=0;i<3000;i++) {
      sdk_os_delay_us(1000);
    }
    DEBUG("Starting after delay\n");
#endif

    for(int i=0;i<NO_CHARACTERISTICS;i++) {
      homekit_config[i].switch_on.value.bool_value = homekit_config[i].default_val;
    }

    gpio_init();
    create_accessory_name();
    wifi_config_init("Poopi Switch", NULL, on_wifi_ready);

    int buttons[NO_CHARACTERISTICS] = {-1,-1,-1,-1};
    int toggles[NO_CHARACTERISTICS] = {-1,-1,-1,-1};

    if (button_create(config_button_gpio, 0, 3000, config_button_callback)) {
      INFO("Failed to initialize button\n");
    }

    for(int i=0;i<NO_CHARACTERISTICS;i++) {
      bool button_found = false;
      bool toggle_found = false;
      for(int j=0;j<i;j++) {
        if (homekit_config[i].button_gpio == buttons[j]) {
          button_found = true;
          break;
        }
      }
      if(!button_found) {
        buttons[i] = homekit_config[i].button_gpio;
        if(buttons[i]!=-1){
          DEBUG("Creating button for GPIO=%d",buttons[i]);
          if (button_create(buttons[i], 0, 3000, button_callback)) {
            INFO("Failed to initialize button\n");
          }
        }
      }

      for(int j=0;j<i;j++) {
        if (homekit_config[i].toggle_gpio == toggles[j]) {
          toggle_found = true;
          break;
        }
      }
      if(!toggle_found) {
        toggles[i] = homekit_config[i].toggle_gpio;
        if(toggles[i]!=-1){
          DEBUG("Creating toggle for GPIO=%d",toggles[i]);
          if (toggle_create(toggles[i], toggle_callback)) {
              INFO("Failed to initialize toggle\n");
          }
        }
      }
    }
}
