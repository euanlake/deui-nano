#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "board_backlight.h"
#include "board_display.h"
#include "board_haptic.h"
#include "board_leds.h"
#include "board_power.h"
#include "deui_ble_client.h"
#include "deui_scale_client.h"
#include "deui_ui.h"
#include "deui_weight_stop.h"
#include "input.h"
#include "wifi_setup.h"

static const char *TAG = "deui_main";
enum {
  k_de1_minor_preinfuse = 0x04,
};

void app_main(void) {
  QueueHandle_t input_queue = xQueueCreate(32, sizeof(lm_ctrl_input_event_t));
  lv_disp_t *display = NULL;
  lm_ctrl_input_event_t event;
  uint8_t previous_major_state = 0xff;
  bool shot_timer_armed = false;
  int64_t shot_timer_start_us = 0;

  ESP_LOGI(TAG, "DEUI firmware starting (ST77916 + CST816)");

  ESP_ERROR_CHECK(lm_ctrl_display_init(&display));
  ESP_ERROR_CHECK(lm_ctrl_backlight_init());
  ESP_ERROR_CHECK(lm_ctrl_backlight_on());
  ESP_ERROR_CHECK(lm_ctrl_power_init());
  ESP_ERROR_CHECK(deui_wifi_init());
  ESP_ERROR_CHECK(deui_ble_init());
  ESP_ERROR_CHECK(deui_scale_init());
  ESP_ERROR_CHECK(deui_weight_stop_init());
  ESP_ERROR_CHECK(lm_ctrl_input_init(input_queue));
  if (lm_ctrl_leds_init() != ESP_OK) {
    ESP_LOGW(TAG, "LED ring unavailable");
  }
  if (lm_ctrl_haptic_init(lm_ctrl_display_i2c_bus()) != ESP_OK) {
    ESP_LOGW(TAG, "Haptic unavailable");
  }
  ESP_ERROR_CHECK(deui_ui_init(display));

  while (true) {
    /** LVGL ticking is handled by esp_lv_adapter's worker (under esp_lv_adapter_lock). */

    while (xQueueReceive(input_queue, &event, 0) == pdTRUE) {
      if (event.type == LM_CTRL_EVENT_ROTATE) {
        deui_ui_indicate_ring_step(event.delta_steps);
        (void)lm_ctrl_leds_indicate_rotation(event.delta_steps);
        (void)lm_ctrl_haptic_click();
      }
    }

    deui_ble_tick();
    deui_ble_status_t ble_pre_status = {0};
    deui_ble_get_status(&ble_pre_status);

    deui_scale_tick(ble_pre_status.connected);
    deui_scale_status_t scale_status = {0};
    deui_scale_get_status(&scale_status);

    deui_ble_set_scale_weight(scale_status.weight_g, scale_status.has_weight, scale_status.connected);
    deui_weight_stop_tick(ble_pre_status.connected, ble_pre_status.de1_major_state,
                          ble_pre_status.de1_minor_state, scale_status.connected,
                          scale_status.has_weight, scale_status.weight_g);

    deui_ui_tick();
    lm_ctrl_leds_tick();

    deui_ble_status_t ble_status = {0};
    deui_wifi_info_t wifi_info = {0};
    lm_ctrl_power_info_t power_info = {0};
    deui_ui_status_t ui_status = {0};

    deui_ble_get_status(&ble_status);
    deui_wifi_get_info(&wifi_info);
    lm_ctrl_power_get_info(&power_info);

    const bool entered_espresso = (previous_major_state != DE1_MAJOR_STATE_ESPRESSO) &&
                                  (ble_status.de1_major_state == DE1_MAJOR_STATE_ESPRESSO);
    if (entered_espresso && scale_status.connected) {
      esp_err_t tare_rc = deui_scale_send_tare();
      if (tare_rc != ESP_OK) {
        ESP_LOGW(TAG, "Auto-tare on Espresso entry failed: %s", esp_err_to_name(tare_rc));
      } else {
        ESP_LOGI(TAG, "Auto-tare triggered on Espresso entry");
      }
    }
    previous_major_state = ble_status.de1_major_state;

    ui_status.ble_connected = ble_status.connected;
    ui_status.wifi_connected = wifi_info.sta_connected;
    ui_status.scale_connected = scale_status.connected;
    ui_status.scale_scanning = scale_status.scanning;
    ui_status.power = power_info;

    strncpy(ui_status.ble_footer, ble_status.detail_line, sizeof(ui_status.ble_footer) - 1);
    ui_status.ble_footer[sizeof(ui_status.ble_footer) - 1] = '\0';

    strncpy(ui_status.machine_state_center, ble_status.machine_state_label,
            sizeof(ui_status.machine_state_center) - 1);
    ui_status.machine_state_center[sizeof(ui_status.machine_state_center) - 1] = '\0';
    ui_status.de1_state_valid = ble_status.de1_state_valid;
    ui_status.de1_major_state = ble_status.de1_major_state;
    ui_status.show_shot_time = ble_status.show_shot_time;
    ui_status.show_scale_weight = ble_status.show_scale_weight;

    deui_ui_update_status(&ui_status);

    float w_g = ble_status.weight_g;
    float t_s = 0.f;
    float flow = ble_status.flow_ml_s;
    float bar = ble_status.pressure_bar;
    const bool brew_ui = ble_status.connected && ble_status.de1_state_valid &&
                         (ble_status.de1_major_state == DE1_MAJOR_STATE_ESPRESSO);
    const bool preinfuse_or_later =
        brew_ui && (ble_status.de1_minor_state >= k_de1_minor_preinfuse);
    if (!brew_ui) {
      shot_timer_armed = false;
      shot_timer_start_us = 0;
    } else if (preinfuse_or_later && !shot_timer_armed) {
      shot_timer_start_us = esp_timer_get_time();
      shot_timer_armed = true;
    }
    if (shot_timer_armed && shot_timer_start_us > 0) {
      t_s = (float)(esp_timer_get_time() - shot_timer_start_us) / 1000000.0f;
      if (t_s < 0.f) {
        t_s = 0.f;
      }
    }
    bool shot_metrics = brew_ui;
    if (!shot_metrics) {
      w_g = 0.f;
      t_s = 0.f;
      flow = 0.f;
      bar = 0.f;
    }
    deui_ui_update_metrics(w_g, t_s, flow, bar, shot_metrics);

    if (ble_status.connected) {
      (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_CONNECTED);
    } else if (wifi_info.sta_connected) {
      (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_CONNECTING);
    } else {
      (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_SETUP);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
