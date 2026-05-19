set(DEUI_REPO_FONTS "${CMAKE_CURRENT_LIST_DIR}/../../../fonts")
set(PORTAL_FONT_GEN "${CMAKE_CURRENT_BINARY_DIR}/portal_fonts")
file(MAKE_DIRECTORY "${PORTAL_FONT_GEN}")
configure_file("${DEUI_REPO_FONTS}/LabGrotesque-Regular.woff2" "${PORTAL_FONT_GEN}/lab-regular.woff2" COPYONLY)
configure_file("${DEUI_REPO_FONTS}/LabGrotesque-Medium.woff2" "${PORTAL_FONT_GEN}/lab-medium.woff2" COPYONLY)
configure_file("${DEUI_REPO_FONTS}/LabGrotesque-Bold.woff2" "${PORTAL_FONT_GEN}/lab-bold.woff2" COPYONLY)

set(PORTAL_IMG_SRC "${CMAKE_CURRENT_LIST_DIR}/portal_assets")
set(PORTAL_IMG_GEN "${CMAKE_CURRENT_BINARY_DIR}/portal_img")
file(MAKE_DIRECTORY "${PORTAL_IMG_GEN}")
configure_file("${PORTAL_IMG_SRC}/PlusLight.png" "${PORTAL_IMG_GEN}/plus-light.png" COPYONLY)
configure_file("${PORTAL_IMG_SRC}/PlusDark.png" "${PORTAL_IMG_GEN}/plus-dark.png" COPYONLY)
configure_file("${PORTAL_IMG_SRC}/MinusLight.png" "${PORTAL_IMG_GEN}/minus-light.png" COPYONLY)
configure_file("${PORTAL_IMG_SRC}/MinusDark.png" "${PORTAL_IMG_GEN}/minus-dark.png" COPYONLY)
configure_file("${PORTAL_IMG_SRC}/deui-logo.png" "${PORTAL_IMG_GEN}/deui-logo.png" COPYONLY)

idf_component_register(
  SRCS
    "app_main.c"
    "board/board_backlight.c"
    "board/board_display.c"
    "board/board_haptic.c"
    "board/board_leds.c"
    "board/board_power.c"
    "ble/deui_ble_client.c"
    "ble/deui_ble_gap.c"
    "ble/deui_ble_gatt.c"
    "ble/deui_ble_parse.c"
    "ble/deui_ble_state.c"
    "power/deui_power_policy.c"
    "ble/deui_scale_client.c"
    "ble/deui_weight_stop.c"
    "brew/deui_shot_metrics.c"
    "ble/deui_ble_hci_compat.c"
    "ui/deui_ui.c"
    "ui/deui_ui_metrics.c"
    "ui/deui_ui_widgets.c"
    "ui/screens/deui_ui_screen_searching.c"
    "ui/screens/deui_ui_screen_idle.c"
    "ui/screens/deui_ui_screen_brewing.c"
    "../../../fonts/LabGrotesque_Regular_16.c"
    "../../../fonts/LabGrotesque_Bold_24.c"
    "../../../fonts/LabGrotesque_Bold_48.c"
    "input/input.c"
    "net/wifi/deui_wifi_ap.c"
    "net/wifi/deui_wifi_captive_dns.c"
    "net/wifi/deui_wifi_events.c"
    "net/wifi/deui_wifi_log.c"
    "net/wifi/deui_wifi_mdns.c"
    "net/wifi/deui_wifi_portal.c"
    "net/wifi/deui_wifi_portal_http.c"
    "net/wifi/deui_wifi_portal_html.c"
    "net/wifi/deui_wifi_portal_fonts.c"
    "net/wifi/deui_wifi_portal_assets.c"
    "net/wifi/deui_wifi_coex.c"
    "net/wifi/deui_wifi_reconnect.c"
    "net/wifi/deui_wifi_service.c"
    "net/wifi/deui_wifi_settings.c"
    "net/wifi/deui_wifi_sta.c"
    "net/wifi/deui_wifi_provision.c"
  INCLUDE_DIRS
    "."
    "board"
    "input"
    "net"
    "net/wifi"
    "ble"
    "brew"
    "power"
    "ui"
    "ui/screens"
  REQUIRES
    bt
    deui_nimble_cent
    espressif__esp_lvgl_adapter
    lvgl__lvgl
    mdns
    led_strip
    esp_adc
    esp_driver_usb_serial_jtag
    esp_driver_ledc
    esp_driver_rmt
    esp_coex
    esp_event
    esp_http_server
    esp_netif
    esp_wifi
    nvs_flash
  EMBED_FILES
    "${PORTAL_FONT_GEN}/lab-regular.woff2"
    "${PORTAL_FONT_GEN}/lab-medium.woff2"
    "${PORTAL_FONT_GEN}/lab-bold.woff2"
    "${PORTAL_IMG_GEN}/plus-light.png"
    "${PORTAL_IMG_GEN}/plus-dark.png"
    "${PORTAL_IMG_GEN}/minus-light.png"
    "${PORTAL_IMG_GEN}/minus-dark.png"
    "${PORTAL_IMG_GEN}/deui-logo.png"
  )

# NimBLE always negotiates LE Data Length on connect; DE1 returns UNSUPP_REM_FEATURE.
target_link_options(
  ${COMPONENT_LIB}
  INTERFACE
  "-Wl,--wrap=ble_hs_hci_util_set_data_len"
  "-u"
  "__wrap_ble_hs_hci_util_set_data_len")
