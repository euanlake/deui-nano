#include "lcd_bsp.h"
#include "cst816.h"
#include "lcd_bl_pwm_bsp.h"
#include "lcd_config.h"
#include "brewing_simulation.h"
#include "NimBLEDevice.h"
#include "de1_ble_client.h"
// #include "fonts.h"
// FreeRTOS queue for cross-task log transport
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Include the font files
#include "../fonts/LabGrotesque_Regular_16.c"
// #include "../fonts/LabGrotesque_Bold_24.c"
#include "../fonts/LabGrotesque_Bold_48.c"

// Global variables for UI elements
lv_obj_t *weight_label;
lv_obj_t *weight_value;
lv_obj_t *time_label;
lv_obj_t *time_value;
lv_obj_t *blue_arc;
lv_obj_t *green_arc;
lv_obj_t *de1_connection_status;
lv_obj_t *device_list_label;
lv_obj_t *log_box;

// Log queue for safely passing log lines from BLE callbacks to UI thread
static QueueHandle_t gLogQueue = nullptr;

// App logging helper: printf-style, mirrored to Serial and queued for on-screen log
extern "C" void appLogf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
  if (gLogQueue) {
    // Send a copy of the buffer to the queue (non-blocking)
    xQueueSend(gLogQueue, buf, 0);
  }
}

// Font handles - using custom fonts
const lv_font_t *font_heading_medium = &lv_font_montserrat_14;
const lv_font_t *font_body_regular = &LabGrotesque_Regular_16;
const lv_font_t *font_body_small = &LabGrotesque_Regular_16;
const lv_font_t *font_large_values = &LabGrotesque_Bold_48; // Large bold font for values

// Animation variables
unsigned long start_time = 0;
bool animation_running = false;
BrewingData current_brewing_data;

// UI callback function for DE1 connection state changes
void onDE1ConnectionStateChanged(bool connected) {
  // This will be called immediately when connection state changes
  // Trigger immediate UI update
  update_de1_connection_indicator();
  update_device_list_display();
  
  if (connected) {
    appLogf("🎉 UI updated for DE1 connection");
  } else {
    appLogf("⚠️ UI updated for DE1 disconnection");
  }
}



// Update timing variables
unsigned long last_ui_update = 0;
unsigned long last_gauge_update = 0;
unsigned long last_de1_scan = 0;
const unsigned long UI_UPDATE_INTERVAL = 100;  // Update UI every 100ms
const unsigned long GAUGE_UPDATE_INTERVAL = 50; // Update gauges every 50ms
const unsigned long DE1_SCAN_INTERVAL = 10000; // Scan for DE1 every 10 seconds

// Color definitions from style guide
#define COLOR_PRIMARY    0xEBE8E8  // #EBE8E8
#define COLOR_SUBTLE     0x757575  // #757575
#define COLOR_BACKGROUND 0x000000  // #000000 (card background)
#define COLOR_OVERLAY    0x000000  // #000000 (main background - black)
#define COLOR_BLUE       0x002E59  // #002E59
#define COLOR_RED        0xB4000B  // #B4000B
#define COLOR_GREEN      0x00B41C  // #00B41C - Verified correct
#define COLOR_ORANGE     0xFF9900  // #FF9900

void setup()
{
  Serial.begin(115200);
  Serial.println("ESP32-S3 Custom LVGL Interface Starting...");
  
  // Initialize NimBLE
  NimBLEDevice::init("DEUI-Nano");
  // Boost TX power for better discovery/connection range
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  Serial.println("NimBLE initialized");
  // Create log queue (20 messages of up to 256 bytes)
  gLogQueue = xQueueCreate(20, 256);
  
  // Register UI callback for connection state changes
  de1Client.setUIConnectionCallback(onDE1ConnectionStateChanged);
  Serial.println("UI connection callback registered");
  
  // Start initial DE1 scan
  Serial.println("Starting initial DE1 device scan...");
  if (de1Client.scanForDE1Devices(30)) {
    Serial.println("DE1 device found during initial scan");
    // Give the scan callback time to complete safely
    delay(500);
    // Try to connect to the found device using safer method
    if (de1Client.hasFoundDevice()) {
      NimBLEAddress foundAddress = de1Client.getFoundDeviceAddress();
      Serial.printf("Connecting to discovered device: %s\n", foundAddress.toString().c_str());
      if (de1Client.connectToDE1(foundAddress)) {
        Serial.println("Successfully connected to DE1 device");
      } else {
        Serial.println("Failed to connect to DE1 device");
      }
    } else {
      Serial.println("Device found but address not available");
    }
  } else {
    Serial.println("No DE1 devices found during initial scan");
  }
  
  Touch_Init();
  Serial.println("Touch initialized");
  
  lcd_lvgl_Init();
  Serial.println("LCD and LVGL initialized");
  
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
  Serial.println("Backlight initialized");
  
  // Create custom UI
  create_custom_interface();
  
  // Start animation
  start_brewing_animation();
  
  Serial.println("Setup complete - brewing animation ready!");
}

void create_custom_interface()
{
  // Set screen background to gray (Overlay from style guide)
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(COLOR_OVERLAY), LV_PART_MAIN);
  
  // Create main metrics card (black background)
  lv_obj_t *metrics_card = lv_obj_create(lv_scr_act());
  lv_obj_set_size(metrics_card, 320, 120);
  lv_obj_align(metrics_card, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(metrics_card, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN);
  lv_obj_set_style_radius(metrics_card, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_all(metrics_card, 16, LV_PART_MAIN);
  lv_obj_set_style_border_width(metrics_card, 0, LV_PART_MAIN);
  
  // Create weight section (left side)
  lv_obj_t *weight_container = lv_obj_create(metrics_card);
  lv_obj_set_size(weight_container, 140, 88);
  lv_obj_align(weight_container, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_color(weight_container, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN);
  lv_obj_set_style_border_width(weight_container, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(weight_container, 0, LV_PART_MAIN);
  
  // Weight label (small, all caps) - moved closer to value
  weight_label = lv_label_create(weight_container);
  lv_label_set_text(weight_label, "WEIGHT (G)");
  lv_obj_set_style_text_color(weight_label, lv_color_hex(COLOR_SUBTLE), LV_PART_MAIN);
  lv_obj_set_style_text_font(weight_label, font_body_small, LV_PART_MAIN);
  lv_obj_align(weight_label, LV_ALIGN_TOP_MID, 0, 8); // Reduced spacing
  
  // Weight value (large, prominent) - moved closer to label
  weight_value = lv_label_create(weight_container);
  lv_label_set_text(weight_value, "0g");
  lv_obj_set_style_text_color(weight_value, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
  lv_obj_set_style_text_font(weight_value, font_large_values, LV_PART_MAIN);
  lv_obj_align(weight_value, LV_ALIGN_BOTTOM_MID, 0, -8); // Reduced spacing
  

  
  // Create time section (right side)
  lv_obj_t *time_container = lv_obj_create(metrics_card);
  lv_obj_set_size(time_container, 140, 88);
  lv_obj_align(time_container, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(time_container, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN);
  lv_obj_set_style_border_width(time_container, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(time_container, 0, LV_PART_MAIN);
  
  // Time label (small, all caps) - moved closer to value
  time_label = lv_label_create(time_container);
  lv_label_set_text(time_label, "TIME (S)");
  lv_obj_set_style_text_color(time_label, lv_color_hex(COLOR_SUBTLE), LV_PART_MAIN);
  lv_obj_set_style_text_font(time_label, font_body_small, LV_PART_MAIN);
  lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 8); // Reduced spacing
  
  // Time value (large, prominent) - moved closer to label
  time_value = lv_label_create(time_container);
  lv_label_set_text(time_value, "0s");
  lv_obj_set_style_text_color(time_value, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
  lv_obj_set_style_text_font(time_value, font_large_values, LV_PART_MAIN);
  lv_obj_align(time_value, LV_ALIGN_BOTTOM_MID, 0, -8); // Reduced spacing
  

  
  // Create green arc (pressure gauge) - outer ring
  green_arc = lv_arc_create(lv_scr_act());
  lv_obj_set_size(green_arc, 340, 340);
  lv_obj_align(green_arc, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_rotation(green_arc, 270); // Start at 12 o'clock (top)
  lv_arc_set_bg_angles(green_arc, 0, 30); // Initial small span
  lv_arc_set_range(green_arc, 0, 360); // 0-360 degrees for clock face
  lv_arc_set_value(green_arc, 0); // Start at 0
  lv_obj_set_style_arc_color(green_arc, lv_color_hex(COLOR_GREEN), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(green_arc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(green_arc, lv_color_hex(0x000000), LV_PART_MAIN); // Transparent background
  lv_obj_set_style_arc_width(green_arc, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(green_arc, lv_color_hex(0x000000), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(green_arc, LV_OPA_TRANSP, LV_PART_KNOB);
  
  // Create blue arc (flow gauge) - also on outer ring, overlapping with pressure
  blue_arc = lv_arc_create(lv_scr_act());
  lv_obj_set_size(blue_arc, 340, 340); // Same size as green arc
  lv_obj_align(blue_arc, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_rotation(blue_arc, 270); // Start at 12 o'clock (top)
  lv_arc_set_bg_angles(blue_arc, 0, 30); // Initial small span
  lv_arc_set_range(blue_arc, 0, 360); // 0-360 degrees for clock face
  lv_arc_set_value(blue_arc, 0); // Start at 0
  lv_obj_set_style_arc_color(blue_arc, lv_color_hex(COLOR_BLUE), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(blue_arc, 12, LV_PART_INDICATOR); // Same width as green
  lv_obj_set_style_arc_color(blue_arc, lv_color_hex(0x000000), LV_PART_MAIN); // Transparent background
  lv_obj_set_style_arc_width(blue_arc, 12, LV_PART_MAIN); // Same width as green
  lv_obj_set_style_bg_color(blue_arc, lv_color_hex(0x000000), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(blue_arc, LV_OPA_TRANSP, LV_PART_KNOB);
  
  // Create DE1 connection status text (above the labels)
  de1_connection_status = lv_label_create(lv_scr_act());
  lv_label_set_text(de1_connection_status, "Not Connected");
  lv_obj_set_style_text_color(de1_connection_status, lv_color_hex(COLOR_RED), LV_PART_MAIN);
  lv_obj_set_style_text_font(de1_connection_status, font_body_small, LV_PART_MAIN); // Same font as labels
  lv_obj_align(de1_connection_status, LV_ALIGN_TOP_MID, 0, 40); // Centered above the metrics card
  
  // Create device list label (below the metrics card)
  device_list_label = lv_label_create(lv_scr_act());
  lv_label_set_text(device_list_label, "Scanning for devices...");
  lv_obj_set_style_text_color(device_list_label, lv_color_hex(COLOR_SUBTLE), LV_PART_MAIN);
  lv_obj_set_style_text_font(device_list_label, font_body_small, LV_PART_MAIN);
  lv_obj_align(device_list_label, LV_ALIGN_BOTTOM_MID, 0, -20); // Below the metrics card
  lv_obj_set_style_text_align(device_list_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  // Create an on-screen log box for BLE diagnostics
  log_box = lv_textarea_create(lv_scr_act());
  lv_obj_set_size(log_box, 320, 140);
  lv_obj_align(log_box, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_textarea_set_max_length(log_box, 4096);
  lv_textarea_set_one_line(log_box, false);
  lv_textarea_set_text(log_box, "Logs:\n");
  lv_obj_set_style_text_font(log_box, font_body_small, LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(log_box, LV_SCROLLBAR_MODE_AUTO);
}

void start_brewing_animation()
{
  start_time = millis();
  animation_running = true;
  current_brewing_data = {0, 0, 0, 0};
  last_ui_update = 0;
  last_gauge_update = 0;
  Serial.println("Starting espresso brewing animation...");
}

void update_brewing_animation()
{
  if (!animation_running) return;
  
  unsigned long current_millis = millis();
  unsigned long elapsed = current_millis - start_time;
  float seconds = elapsed / 1000.0;
  
  // Reset animation after 30 seconds
  if (seconds >= BREW_TIME_TOTAL) {
    start_brewing_animation();
    return;
  }
  
  // Get brewing data from simulation
  current_brewing_data = calculateBrewingValues(seconds);
  
  // Update UI elements with timing control
  if (current_millis - last_ui_update >= UI_UPDATE_INTERVAL) {
    update_ui_values();
    last_ui_update = current_millis;
  }
  
  if (current_millis - last_gauge_update >= GAUGE_UPDATE_INTERVAL) {
    update_gauges();
    last_gauge_update = current_millis;
  }
}

void update_ui_values()
{
  // Update weight display (number only, unit is in label)
  char weight_str[16];
  snprintf(weight_str, sizeof(weight_str), "%.1f", current_brewing_data.weight);
  lv_label_set_text(weight_value, weight_str);
  
  // Update time display (number only, unit is in label)
  char time_str[16];
  snprintf(time_str, sizeof(time_str), "%d", (int)round(current_brewing_data.time));
  lv_label_set_text(time_value, time_str);
}

void update_gauges()
{
  // Clock face style gauge updates using proper scaling
  
  // Update blue arc (flow) - clock face style (0-3.5 range)
  // Flow uses only half the clock face: 0=top, 1.75=right, 3.5=bottom
  float flow_clock = calculateFlowClockPosition(current_brewing_data.flow);
  
  // Calculate clock position: 0=top(270°), 3=right(0°), 6=bottom(90°)
  int flow_hour = (int)flow_clock;
  float flow_fraction = flow_clock - flow_hour; // Decimal part (0.0 to 1.0)
  
  // Convert to degrees: 0=top(270°), 3=right(0°), 6=bottom(90°)
  int flow_start_angle = 270 + (flow_hour * CLOCK_DEGREES_PER_HOUR); // Start at the hour position
  int flow_end_angle = flow_start_angle + (int)(CLOCK_DEGREES_PER_HOUR * flow_fraction); // Smooth interpolation
  
  // Set the arc to show the smooth span
  lv_arc_set_bg_angles(blue_arc, flow_start_angle, flow_start_angle + CLOCK_DEGREES_PER_HOUR);
  lv_arc_set_value(blue_arc, flow_end_angle);
  
  // Update green arc (pressure) - clock face style (0-9 range)
  // Pressure uses full clock face: 0=top, 4.5=right, 9=bottom, back to top
  float pressure_clock = calculatePressureClockPosition(current_brewing_data.pressure);
  
  // Calculate clock position: 0=top, 3=right, 6=bottom, 9=left
  int pressure_hour = (int)pressure_clock;
  float pressure_fraction = pressure_clock - pressure_hour; // Decimal part (0.0 to 1.0)
  
  // Convert to degrees: 0=top(270°), 3=right(0°), 6=bottom(90°), 9=left(180°)
  int pressure_start_angle = 270 + (pressure_hour * CLOCK_DEGREES_PER_HOUR); // Start at the hour position
  int pressure_end_angle = pressure_start_angle + (int)(CLOCK_DEGREES_PER_HOUR * pressure_fraction); // Smooth interpolation
  
  // Set the arc to show the smooth span
  lv_arc_set_bg_angles(green_arc, pressure_start_angle, pressure_start_angle + CLOCK_DEGREES_PER_HOUR);
  lv_arc_set_value(green_arc, pressure_end_angle);
}

void update_de1_connection_indicator()
{
  static bool was_connected = false;
  bool is_connected = de1Client.isDeviceConnected();
  
  if (is_connected) {
    // Connected: show device info in green text
    DE1Device device = de1Client.getConnectedDevice();
    char connection_text[128];
    snprintf(connection_text, sizeof(connection_text), "✅ Connected: %s", device.name.c_str());
    lv_label_set_text(de1_connection_status, connection_text);
    lv_obj_set_style_text_color(de1_connection_status, lv_color_hex(COLOR_GREEN), LV_PART_MAIN);
    
    // Hide log box when connection is established (first time only)
    if (!was_connected && log_box) {
      lv_obj_add_flag(log_box, LV_OBJ_FLAG_HIDDEN);
      Serial.println("🎉 DE1 Connected - Hiding log display");
      
      // Add a small success message to the log before hiding
      if (gLogQueue) {
        const char* success_msg = "🎉 DE1 Connection Established - Switching to operational view";
        xQueueSend(gLogQueue, success_msg, 0);
      }
    }
  } else {
    // Disconnected: red text
    lv_label_set_text(de1_connection_status, "❌ Not Connected");
    lv_obj_set_style_text_color(de1_connection_status, lv_color_hex(COLOR_RED), LV_PART_MAIN);
    
    // Show log box when disconnected (to help with troubleshooting)
    if (was_connected && log_box) {
      lv_obj_clear_flag(log_box, LV_OBJ_FLAG_HIDDEN);
      Serial.println("⚠️ DE1 Disconnected - Showing log display");
    }
  }
  
  was_connected = is_connected;
}

void update_device_list_display()
{
  if (de1Client.isDeviceConnected()) {
    // When connected, show device details
    DE1Device device = de1Client.getConnectedDevice();
    char device_info[200];
    snprintf(device_info, sizeof(device_info), 
             "Device: %s | Address: %s | RSSI: %d dBm", 
             device.name.c_str(), 
             device.address.toString().c_str(), 
             device.rssi);
    lv_label_set_text(device_list_label, device_info);
    lv_obj_set_style_text_color(device_list_label, lv_color_hex(COLOR_PRIMARY), LV_PART_MAIN);
  } else {
    // When not connected, show scanning status or device list
    std::string deviceList = de1Client.getDeviceListAsString();
    if (deviceList.empty() || deviceList == "No devices discovered") {
      lv_label_set_text(device_list_label, "Scanning for DE1 devices...");
      lv_obj_set_style_text_color(device_list_label, lv_color_hex(COLOR_SUBTLE), LV_PART_MAIN);
    } else {
      lv_label_set_text(device_list_label, deviceList.c_str());
      lv_obj_set_style_text_color(device_list_label, lv_color_hex(COLOR_SUBTLE), LV_PART_MAIN);
    }
  }
}



void loop()
{
  // Update brewing animation
  update_brewing_animation();
  
  // Touch debugging - reduced frequency
  static unsigned long last_touch_check = 0;
  unsigned long current_millis = millis();
  
  if (current_millis - last_touch_check >= 200) { // Check touch every 200ms
    uint16_t x, y;
    uint8_t touch_detected = getTouch(&x, &y);
    
    if (touch_detected) {
      Serial.printf("Touch detected at: (%d, %d)\n", x, y);
    }
    last_touch_check = current_millis;
  }
  
  // Update DE1 connection indicator
  update_de1_connection_indicator();
  
  // Update device list display
  update_device_list_display();

  // Flush log messages from queue to on-screen log (UI thread safe)
  if (gLogQueue && log_box) {
    char msg[256];
    while (xQueueReceive(gLogQueue, msg, 0) == pdTRUE) {
      lv_textarea_add_text(log_box, msg);
      lv_textarea_add_text(log_box, "\n");
    }
  }
  
  // Periodic DE1 scanning and connection management
  if (current_millis - last_de1_scan >= DE1_SCAN_INTERVAL) {
    if (!de1Client.isDeviceConnected()) {
      Serial.println("DE1 not connected, scanning for devices...");
      if (de1Client.scanForDE1Devices(5)) {
        Serial.println("DE1 device found, attempting connection...");
        // Give the scan callback time to complete safely
        delay(200);
        if (de1Client.hasFoundDevice()) {
          NimBLEAddress foundAddress = de1Client.getFoundDeviceAddress();
          Serial.printf("Connecting to discovered device: %s\n", foundAddress.toString().c_str());
          if (de1Client.connectToDE1(foundAddress)) {
            Serial.println("Successfully connected to DE1 device");
          } else {
            Serial.println("Failed to connect to DE1 device");
          }
        } else {
          Serial.println("Device found but address not available");
        }
      } else {
        Serial.println("No DE1 devices found during scan");
      }
    } else {
      Serial.println("DE1 device is connected");
      // Print current state and temperatures
      DE1MachineState state = de1Client.getCurrentState();
      DE1Temperatures temps = de1Client.getCurrentTemperatures();
      Serial.printf("DE1 State: %s\n", de1Client.stateToString(state).c_str());
      Serial.printf("DE1 Water Heater Temp: %.1f°C\n", temps.waterHeaterTemp);
    }
    last_de1_scan = current_millis;
  }

#ifdef Backlight_Testing
  setUpdutySubdivide(LCD_PWM_MODE_255);
  delay(1000);
  setUpdutySubdivide(LCD_PWM_MODE_200);
  delay(1000);
  setUpdutySubdivide(LCD_PWM_MODE_150);
  delay(1000);
  setUpdutySubdivide(LCD_PWM_MODE_100);
  delay(1000);
  setUpdutySubdivide(LCD_PWM_MODE_50);
  delay(1000);
  setUpdutySubdivide(LCD_PWM_MODE_0);
  delay(1000);
#endif

  // Increased delay to reduce CPU load and match LVGL refresh rate
  delay(30); // 33 FPS update rate to match LVGL's 30ms refresh period
}
