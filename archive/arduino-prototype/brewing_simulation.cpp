#include "brewing_simulation.h"

BrewingData calculateBrewingValues(float elapsed_seconds) {
  BrewingData data;
  
  // Update time
  data.time = elapsed_seconds;
  
  // Update weight (0 to 36g over 30 seconds) - linear progression
  data.weight = (elapsed_seconds / BREW_TIME_TOTAL) * WEIGHT_MAX;
  
  // Update pressure - espresso machine simulation
  if (elapsed_seconds < PRESSURE_RAMP_UP_TIME) {
    // Smooth ramp up from 0 to 9 bar
    data.pressure = (elapsed_seconds / PRESSURE_RAMP_UP_TIME) * PRESSURE_MAX;
  } else if (elapsed_seconds < PRESSURE_HOLD_END) {
    // Hold at 9 bar
    data.pressure = PRESSURE_MAX;
  } else {
    // Smooth ramp down from 9 to 0 bar
    data.pressure = PRESSURE_MAX - ((elapsed_seconds - PRESSURE_RAMP_DOWN_START) / (BREW_TIME_TOTAL - PRESSURE_RAMP_DOWN_START)) * PRESSURE_MAX;
    if (data.pressure < 0) data.pressure = 0;
  }
  
  // Update flow - espresso machine simulation
  if (elapsed_seconds < FLOW_RAMP_UP_TIME) {
    // Smooth ramp up from 0 to 3.5 ml/s
    data.flow = (elapsed_seconds / FLOW_RAMP_UP_TIME) * FLOW_MAX;
  } else if (elapsed_seconds < FLOW_HOLD_END) {
    // Hold at 3.5 ml/s
    data.flow = FLOW_MAX;
  } else {
    // Smooth ramp down from 3.5 to 0 ml/s
    data.flow = FLOW_MAX - ((elapsed_seconds - FLOW_RAMP_DOWN_START) / (BREW_TIME_TOTAL - FLOW_RAMP_DOWN_START)) * FLOW_MAX;
    if (data.flow < 0) data.flow = 0;
  }
  
  return data;
}

// Calculate pressure clock position (0-9 bar maps to 0-12 hours)
float calculatePressureClockPosition(float pressure) {
  // Map 0-9 bar to 0-12 hours on clock face
  // 0 bar = 0 hours (top), 9 bar = 12 hours (back to top)
  return (pressure / PRESSURE_MAX) * CLOCK_HOURS;
}

// Calculate flow clock position (0-3.5 ml/s maps to 0-6 hours)
float calculateFlowClockPosition(float flow) {
  // Map 0-3.5 ml/s to 0-6 hours on clock face
  // 0 ml/s = 0 hours (top), 3.5 ml/s = 6 hours (bottom)
  return (flow / FLOW_MAX) * 6.0f; // Only use half the clock face for flow
} 