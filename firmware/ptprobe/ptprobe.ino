#include <Bounce2.h>
#include "displaymanager.h"
#include "temperaturesensors.h"

#define FW_VERSION_MINOR 1
#define FW_VERSION_MAJOR 0

// mode select push button on GPIO9
#define NEXT_BUTTON_PIN 9

// menu apply push button on GPIO10
#define APPLY_BUTTON_PIN 10

// status LED 0 GPIO6
#define STATUS0_PIN 6

// status LED 1 GPIO7
#define STATUS1_PIN 7

// OneWire bus on GPIO8
#define ONE_WIRE_BUS 8

// objects
Bounce next_button = Bounce();
Bounce apply_button = Bounce();
TemperatureSensors probes_T(ONE_WIRE_BUS);
DisplayManager display; // OLED Display

void setup() 
{
  // ADC setup
  analogReadResolution(12);
  
  pinMode(STATUS0_PIN, OUTPUT);
  pinMode(STATUS1_PIN, OUTPUT);

  next_button.attach(NEXT_BUTTON_PIN, INPUT_PULLUP);
  next_button.interval(50);

  apply_button.attach(APPLY_BUTTON_PIN, INPUT_PULLUP);
  apply_button.interval(50);

  Serial.begin(115200);

  delay(2000);
  Serial.println("Starting demo");

  char buf[16]; // "fw ver #.#"
  sprintf(buf,"fw ver %d.%d",FW_VERSION_MAJOR, FW_VERSION_MINOR);
  if(!display.begin("PT Probe", &buf[0])) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  Serial.println();
  Serial.println("Acquired display");

  for (int i = 0; i < 3; ++i) {
    digitalWrite(STATUS0_PIN, HIGH);
    digitalWrite(STATUS1_PIN, LOW);
    delay(400);
    digitalWrite(STATUS0_PIN, LOW);
    digitalWrite(STATUS1_PIN, HIGH);
    delay(400);
  }
  digitalWrite(STATUS1_PIN, LOW);

  display.clear_all();

  probes_T.begin();
}

int led0 = 0;
int led1 = 0;
int update_adc_count = 0;

void loop() 
{
  if (update_adc_count > 2000/50) {
    //char pval2buf[16];
    //char pval3buf[16];
    int probe_P2 = analogRead(A2);  // 12-bit range: 0..4095
    int probe_P3 = analogRead(A3);

    String pval2buf = "A2: ";
    pval2buf += String((float)probe_P2/4095.0, 3);
    String pval3buf = "A3: ";
    pval3buf += String((float)probe_P3/4095.0, 3);
      
    display.update_title("Pressure");
    display.update_status(pval2buf.c_str(), pval3buf.c_str());
    display.show();
    update_adc_count = 0;
  }
  
  next_button.update();
  if (next_button.fell()) {
      led0 = led0 == HIGH ? LOW : HIGH;
      digitalWrite(STATUS0_PIN, led0);
  }

  apply_button.update();
  if (apply_button.fell()) {
      led1 = led1 == HIGH ? LOW : HIGH;
      digitalWrite(STATUS1_PIN, led1);
  }

  update_adc_count++;
  delay(50);

}
