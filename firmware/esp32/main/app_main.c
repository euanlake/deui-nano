#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "board_backlight.h"
#include "board_config.h"
#include "board_display.h"
#include "board_haptic.h"
#include "board_leds.h"
#include "board_power.h"
#include "deui_ble_client.h"
#include "deui_power_policy.h"
#include "deui_scale_client.h"
#include "deui_ui.h"
#include "deui_weight_stop.h"
#include "input.h"
#include "wifi_setup.h"

static const char *TAG = "deui_main";
enum {
  k_de1_minor_preinfuse = 0x04,
};

static void suspend_radios_for_battery(void) {
  if (deui_scale_suspend() != ESP_OK) {
    ESP_LOGW(TAG, "Failed to suspend scale client");
  }
  if (deui_ble_suspend() != ESP_OK) {
    ESP_LOGW(TAG, "Failed to suspend BLE client");
  }
  if (deui_wifi_suspend() != ESP_OK) {
    ESP_LOGW(TAG, "Failed to suspend Wi-Fi");
  }
}

static void log_wakeup_reason(void) {
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    ESP_LOGI(TAG, "Boot reason: cold/power-on reset");
    return;
  }

  ESP_LOGI(TAG, "Boot reason: wakeup cause=%d", (int)cause);
  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    ESP_LOGI(TAG, "EXT1 wake mask=0x%llx", (unsigned long long)esp_sleep_get_ext1_wakeup_status());
  }
}

static void enter_battery_deep_sleep(void) {
  const uint64_t wake_mask = (1ULL << LM_CTRL_KNOB_A) | (1ULL << LM_CTRL_KNOB_B);
  if (!esp_sleep_is_valid_wakeup_gpio(LM_CTRL_KNOB_A) ||
      !esp_sleep_is_valid_wakeup_gpio(LM_CTRL_KNOB_B)) {
    ESP_LOGE(TAG, "Encoder GPIOs are not valid deep-sleep wake sources (A=%d, B=%d)",
             LM_CTRL_KNOB_A, LM_CTRL_KNOB_B);
    suspend_radios_for_battery();
    (void)lm_ctrl_backlight_off();
    (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_IDLE);
    return;
  }

  suspend_radios_for_battery();
  (void)lm_ctrl_backlight_off();
  (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_IDLE);
  vTaskDelay(pdMS_TO_TICKS(20));

  esp_err_t wake_disable_rc = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (wake_disable_rc != ESP_OK && wake_disable_rc != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to clear old wake sources: %s", esp_err_to_name(wake_disable_rc));
  }
  ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW));
  ESP_LOGI(TAG, "Entering deep sleep (battery). Rotary wake mask=0x%llx", (unsigned long long)wake_mask);
  esp_deep_sleep_start();
}

void app_main(void) {
  QueueHandle_t input_queue = xQueueCreate(32, sizeof(lm_ctrl_input_event_t));
  lv_disp_t *display = NULL;
  lm_ctrl_input_event_t event;
  lm_ctrl_power_info_t power_info = {0};
  deui_power_policy_t power_policy = {0};
  deui_power_policy_state_t power_state = DEUI_POWER_POLICY_AWAKE;
  uint8_t previous_major_state = 0xff;
  bool shot_timer_armed = false;
  int64_t shot_timer_start_us = 0;
  uint32_t previous_power_version = 0;

  ESP_LOGI(TAG, "DEUI firmware starting (ST77916 + CST816)");
  log_wakeup_reason();

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
  lm_ctrl_power_get_info(&power_info);
  deui_power_policy_init(&power_policy, DEUI_POWER_IDLE_TIMEOUT_MS);
  previous_power_version = lm_ctrl_power_status_version();
  (void)lm_ctrl_backlight_set(DEUI_POWER_ACTIVE_BRIGHTNESS_PERCENT);
  if (LM_CTRL_BATTERY_CHARGE_GPIO == GPIO_NUM_NC) {
    ESP_LOGW(TAG, "No charge/VBUS GPIO configured; line-power mode depends on usb_serial_jtag link");
  }

  while (true) {
    /** LVGL ticking is handled by esp_lv_adapter's worker (under esp_lv_adapter_lock). */
    bool had_activity = false;
    const int64_t now_us = esp_timer_get_time();

    while (xQueueReceive(input_queue, &event, 0) == pdTRUE) {
      if (event.type == LM_CTRL_EVENT_ROTATE) {
        had_activity = true;
        deui_ui_indicate_ring_step(event.delta_steps);
        (void)lm_ctrl_leds_indicate_rotation(event.delta_steps);
        (void)lm_ctrl_haptic_click();
      }
    }

    if (had_activity) {
      deui_power_policy_note_activity(&power_policy, now_us);
    }

    lm_ctrl_power_get_info(&power_info);
    const uint32_t power_version = lm_ctrl_power_status_version();
    if (power_version != previous_power_version) {
      previous_power_version = power_version;
      ESP_LOGI(TAG, "Power source update: line_power=%d battery=%u%% charging=%d",
               (int)deui_power_line_power(&power_info),
               (unsigned)power_info.level_percent,
               (int)power_info.charging);
    }

    const deui_power_policy_state_t next_power_state =
        deui_power_policy_step(&power_policy, &power_info, now_us);
    if (next_power_state != power_state) {
      if (next_power_state == DEUI_POWER_POLICY_BAT_SLEEP) {
        enter_battery_deep_sleep();
      } else {
        if (next_power_state == DEUI_POWER_POLICY_AC_DIMMED) {
          (void)lm_ctrl_backlight_set(DEUI_POWER_DIM_BRIGHTNESS_PERCENT);
        } else {
          (void)lm_ctrl_backlight_set(DEUI_POWER_ACTIVE_BRIGHTNESS_PERCENT);
        }
      }
      power_state = next_power_state;
    }

    if (power_state != DEUI_POWER_POLICY_BAT_SLEEP) {
      deui_ble_tick();
    }
    deui_ble_status_t ble_pre_status = {0};
    deui_ble_get_status(&ble_pre_status);

    if (power_state != DEUI_POWER_POLICY_BAT_SLEEP) {
      deui_scale_tick(ble_pre_status.connected);
    }
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
    /** Match `deui_ui_update_status`: shot metrics only in DE1 major Espresso (0x04), regardless of state_valid. */
    const bool brew_ui =
        ble_status.connected && (ble_status.de1_major_state == DE1_MAJOR_STATE_ESPRESSO);
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
    deui_ui_update_metrics(w_g, t_s, bar, flow, shot_metrics);

    if (power_state == DEUI_POWER_POLICY_BAT_SLEEP) {
      (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_IDLE);
    } else if (ble_status.connected) {
      (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_CONNECTED);
    } else if (wifi_info.sta_connected) {
      (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_CONNECTING);
    } else {
      (void)lm_ctrl_leds_set_status(LM_CTRL_LED_STATUS_SETUP);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
