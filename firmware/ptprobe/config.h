#ifndef ptprobe_config_h_
#define ptprobe_config_h_

#include <Arduino.h>
#include <FlashStorage.h>

#define FW_VERSION_MINOR 5
#define FW_VERSION_MAJOR 0

#ifdef BREADBOARD_PROTO

  // mode select push button 
  #define NEXT_BUTTON_PIN 9
  
  // menu apply push button
  #define APPLY_BUTTON_PIN 10
  
  // status LED 0 
  #define STATUS0_PIN 6
  
  // status LED 1 
  #define STATUS1_PIN 7
  
  // OneWire bus on 
  #define ONE_WIRE_BUS 8

#else

  // mode select push button
  #define NEXT_BUTTON_PIN 6
  
  // menu apply push button on GPIO10
  #define APPLY_BUTTON_PIN 7
  
  // status LED 0 GPIO6
  #define STATUS0_PIN 8
  
  // status LED 1 GPIO7
  #define STATUS1_PIN 9
  
  // OneWire bus on GPIO8
  #define ONE_WIRE_BUS 10

#endif

struct RunConfig
{
  RunConfig() 
    : started(false), led0(0), led1(0), 
      conversion_state(0), debug_level(1), board_id(0) 
  {
  
  }

  void toggle_led0() {
    led0 = led0 ? LOW : HIGH;
    digitalWrite(STATUS0_PIN, led0);
  }
  void set_led0(int8_t state) {
    led0 = state;
    digitalWrite(STATUS0_PIN, led0);
  }

  void toggle_led1() {
    led1 = led1 ? LOW : HIGH;
    digitalWrite(STATUS1_PIN, led1);
  }
  void set_led1(int8_t state) {
    led1 = state;
    digitalWrite(STATUS1_PIN, led1);
  }

  void report_fault(char const* msg) {
    set_led1(HIGH);
    if (debug_level > 0) {
      Serial.println(msg);
    }
  }

  bool started;
  int8_t led0;
  int8_t led1;
  int conversion_state; // 0=idle, 1=conversion, 2=ready, 3=stop

  int8_t debug_level;
  uint32_t board_id;
};

static RunConfig cfg;



#endif  // ptprobe_config_h_
