#ifndef BREWING_SIMULATION_H
#define BREWING_SIMULATION_H

// Brewing simulation data structure
struct BrewingData {
  float pressure;  // in bar
  float flow;      // in ml/s
  float weight;    // in grams
  float time;      // in seconds
};

// Brewing simulation parameters
#define BREW_TIME_TOTAL 30.0f
#define PRESSURE_MAX 9.0f
#define FLOW_MAX 3.5f
#define WEIGHT_MAX 36.0f

// Clock face scaling for gauges
#define CLOCK_HOURS 12.0f
#define CLOCK_DEGREES_PER_HOUR 30.0f

// Pressure simulation phases
#define PRESSURE_RAMP_UP_TIME 3.0f
#define PRESSURE_HOLD_START 3.0f
#define PRESSURE_HOLD_END 27.0f
#define PRESSURE_RAMP_DOWN_START 27.0f

// Flow simulation phases
#define FLOW_RAMP_UP_TIME 5.0f
#define FLOW_HOLD_START 5.0f
#define FLOW_HOLD_END 25.0f
#define FLOW_RAMP_DOWN_START 25.0f

// Function to calculate brewing values at a given time
BrewingData calculateBrewingValues(float elapsed_seconds);

// Functions to calculate clock face positions for gauges
float calculatePressureClockPosition(float pressure);
float calculateFlowClockPosition(float flow);

#endif // BREWING_SIMULATION_H 