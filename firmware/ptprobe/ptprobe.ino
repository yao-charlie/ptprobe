#include <FlashStorage.h>
#include <Bounce2.h>
#include "displaymanager.h"
#include "temperaturesensors.h"
#include "packetcontainer.h"

#define FW_VERSION_MINOR 4
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

#define MSG_BUF_LEN 24
char msg_buffer[MSG_BUF_LEN];
void process_serial_buffer();

struct RunConfig
{
  RunConfig() : started(false), max_packets(0) { }
  
  bool started;
  uint32_t max_packets;
} cfg;

struct TSensorData
{
  TSensorData() : active(false), T(0), Tref(0), fault(0) {}
  bool active;
  float T;
  float Tref;
  int8_t fault;
};

struct PSensorData
{
  PSensorData() : active(false), raw(0), P(0) 
  {
    // from that one test -- raw 0-1 to kPa
    ai[0] = -97.35308;
    ai[1] =  920.6867;
    ai[2] = -14.86687;
  }
  bool active;
  float raw;
  float P;
  float ai[3];
};

TSensorData T_sensor[4];
PSensorData P_sensor[4];
int const P_sensor_pin[] = {A0, A1, A2, A3};

// objects
Bounce next_button = Bounce();
Bounce apply_button = Bounce();
TemperatureSensors probes_T(ONE_WIRE_BUS);
DisplayManager display; // OLED Display

// functions
void read_all();
void process_serial_buffer();

inline void update_T_display(int const ich) 
{
  auto const& Tdata = T_sensor[ich];
  switch (Tdata.fault) {
  case 0:
    display.data_rect(ich,0).update_data(Tdata.T);
    break;
  case 1:
    display.data_rect(ich,0).update_data("ENC",true);
    break;
  case 2:
    display.data_rect(ich,0).update_data("EGND", true);
    break;
  case 4:
    display.data_rect(ich,0).update_data("EVDD", true);
    break;
  case -1:
    display.data_rect(ich,0).update_data("ECRC", true);
    break;
  default:
    display.data_rect(ich,0).update_data("EUNK", true);
    break;
  }
}

inline void update_P_display(int const ich)
{
  display.data_rect(ich,1).update_data(P_sensor[ich].P);
}

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
  //Serial.println("Starting demo");

  char buf[16]; // "fw ver #.#"
  sprintf(buf,"fw ver %d.%d",FW_VERSION_MAJOR, FW_VERSION_MINOR);
  if(!display.begin("PT Probe", &buf[0])) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  //Serial.println();
  //Serial.println("Acquired display");

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

  char data_tag[] = "T0";
  for (int icol = 0; icol < 2; ++icol) {
    for (int i = 0; i < 4; ++i) {
      data_tag[1] = '0'+i;
      display.data_rect(i,icol).update_lbl_hi(data_tag);
      display.data_rect(i,icol).update_data("---.-");
    }
    data_tag[0] = 'P';
  }
  display.show();
  delay(2000);
}

int led0 = 0;
int led1 = 0;
int conversion_state = 0;
unsigned long start_ms = 0;


void loop() 
{
  process_serial_buffer();
  
  if (cfg.started) {
    if (conversion_state == 0) {
      // activity
      led0 = led0 == HIGH ? LOW : HIGH;
      digitalWrite(STATUS0_PIN, led0);
      
      if (probes_T.start_conversion()) {
        conversion_state = 1;
        start_ms = millis();
      } else {
        digitalWrite(STATUS1_PIN, HIGH);
        cfg.started = false;
      }
    } else if ((conversion_state == 1) && (probes_T.conversion_complete())) {
      conversion_state = 2;
    } else if (conversion_state == 2) {
      read_all();

      for (int i = 0; i < 4; ++i) {
        update_T_display(i);
        update_P_display(i);
      }
      display.show();
      conversion_state = 0;
    }
  }

  next_button.update();
  if (next_button.fell()) {
      digitalWrite(STATUS1_PIN, LOW);
  }

  apply_button.update();
  if (apply_button.fell()) {
      digitalWrite(STATUS1_PIN, HIGH);
  }

  delay(10);

}

inline void update_sensor_T(
  TemperatureSensors::MAX31850 const* probe, 
  int8_t const result)
{
  auto const ch_id = probe->id_;
  T_sensor[ch_id].active = true;
  if (result == 0) {        // data ready
    T_sensor[ch_id].T = probe->probe_T_;
    T_sensor[ch_id].Tref = probe->ref_T_;
  } else if (result > 0) {  // fault
    T_sensor[ch_id].fault = probe->fault_status_;
    T_sensor[ch_id].Tref = probe->ref_T_;
  } else if (result < 0) {  // error
    T_sensor[ch_id].fault = result;
  }
}

uint32_t one_shot_T(int const ch) 
{
  T_sensor[ch].active = false;
  T_sensor[ch].fault = 0;
  
  int const ndx = probes_T.get_sensor_ndx(ch);
  if (ndx < 0) {
    return;
  }
  
  if (!probes_T.start_conversion(ndx)) {
    digitalWrite(STATUS1_PIN, HIGH);
    return;
  }
  uint32_t t_start_one_shot = millis();
  do {
    delay(2);
  } while(!probes_T.conversion_complete(ndx));
  uint32_t const t_one_shot = millis()-t_start_one_shot;
    
  int8_t const result = probes_T.read_scratchpad(ndx);
  auto const* probe = probes_T.get_sensor(ndx);
  update_sensor_T(probe,result);
  
  return t_one_shot;
}

inline void one_shot_P(int const ch)
{
  if (ch >= 0 && ch < 4) {
    P_sensor[ch].active = true;
    auto const val = float(analogRead(P_sensor_pin[ch]))/4095.0;
    P_sensor[ch].raw = val;
    P_sensor[ch].P = P_sensor[ch].ai[0] + val*(P_sensor[ch].ai[1] + val*P_sensor[ch].ai[2]);
  }
}

void read_all() 
{
  for (int i = 0; i < 4; ++i) {
    T_sensor[i].active = false;
    T_sensor[i].fault = 0;

    P_sensor[i].active = false;
  }
  
  for (int i = 0; i < probes_T.sensor_count(); ++i) {
    int8_t const result = probes_T.read_scratchpad(i);
    auto const* probe = probes_T.get_sensor(i);
    update_sensor_T(probe,result);
  }

  for (int i = 0; i < 4; ++i) {
    one_shot_P(i);
  }
}

void debug_print_config() 
{
  Serial.println("Configuration updated");
  Serial.print("  + Max packets: ");
  Serial.println(cfg.max_packets);
  
  Serial.println("  + Pressure coefficients:");
  for (int i = 0; i < 4; ++i) {
    Serial.print("    ch");
    Serial.print(i);
    Serial.print(": ");
    for (int j = 0; j < 3; ++j) {
      if (j > 0) {
        Serial.print(", ");
      }
      Serial.print(P_sensor[i].ai[j]);
    }
    Serial.println();
  }
}

//
// Message format
// H : halt, send last packet then halt packet
// R : run (start), streams packets HDR_DATA | timestamp_ms | T .. | P ..
// C : configure
//  M## : max packets, number as int string
//  Pca## : pressure channel c, coefficient a (0=cte, 1=lin, 2=quad), ## as float string 
// A : ask
//  B  : board ID, returns ACK | ID_TYPE | 32bit ID
//  T# : temperature on channel #, returns ACK | PROBE_T_TYPE | CH# | ERR Flag | 32 bit float
//  R# : ref. temperature on channel #, returns ACK | REF_T_TYPE | 32 bit float
//  A# : raw ADC value for pressure channel # , returns ACK | RAW_P_TYPE | 32 bit float 
//  P# : pressure from channel # , returns ACK | PROBE_P_TYPE | 32 bit float 
void process_serial_buffer()
{
  char const data_in = Serial.read();
  if (data_in == 'H') {
    if (cfg.started) {
      cfg.started = false;
      //finalize_transmission();
      display.data_rect(3,0).update_lbl_lo("OFF",false);
      display.show();
      
      led0 = LOW;
      digitalWrite(STATUS0_PIN, led0);
      
      Serial.println("Halting");
    }
  } else if (!cfg.started) {
    if (data_in == 'R') {
      packet.reset_for_write();
      packet.count = 0;
      cfg.started = true;
      display.data_rect(3,0).update_lbl_lo("RUN",true);
      // display will update in loop
      
      Serial.println("Starting");
    } else if (data_in == 'C') {
      int const rlen = Serial.readBytesUntil('\n', msg_buffer, MSG_BUF_LEN-1);
      msg_buffer[rlen] = '\0'; 
      if (rlen > 1) {
        if (msg_buffer[0] == 'M') {      // max packets
          int const cfg_val = atoi(&msg_buffer[1]);
          if (cfg_val >= 0) {
            cfg.max_packets = cfg_val;
          }
        } else if ((msg_buffer[0] == 'P') && (rlen > 4)) {
          int8_t const P_ch = msg_buffer[1] - '0';
          int8_t const icoeff = msg_buffer[2] - '0';
          if ((P_ch >= 0) && (P_ch < 4) && (icoeff >=0) && (icoeff < 3)) {
            P_sensor[P_ch].ai[icoeff] = atof(&msg_buffer[3]);
          }
        }
      }
      debug_print_config();
    } else if (data_in == 'A') {  // ask
      int const rlen = Serial.readBytesUntil('\n', msg_buffer, MSG_BUF_LEN-1);
      msg_buffer[rlen] = '\0'; 
      if (rlen > 0) {
        if (msg_buffer[0] == 'B') {
          Serial.println("Board ID = 12345");   // TODO 
        } else if (rlen > 1) {
          int const ch = msg_buffer[1] - '0';
          
          digitalWrite(STATUS0_PIN, HIGH);
          if (msg_buffer[0] == 'T') {
            one_shot_T(ch);
            update_T_display(ch);
            display.show();
            Serial.print("Probe Temperature (");
            Serial.print(ch);
            Serial.print("): ");
            if (T_sensor[ch].active) {
              Serial.println(T_sensor[ch].T);
            } else {
              Serial.println("inactive");
            }
          } else if (msg_buffer[0] == 'R') {
            one_shot_T(ch);
            Serial.print("Ref. Temperature (");
            Serial.print(ch);
            Serial.print("): ");
            Serial.println(T_sensor[ch].Tref);
          } else if (msg_buffer[0] == 'P') {
            one_shot_P(ch);
            update_P_display(ch);
            display.show();
            Serial.print("Probe Pressure (");
            Serial.print(ch);
            Serial.print("): ");
            Serial.println(P_sensor[ch].P);
          } else if (msg_buffer[0] == 'A') {
            one_shot_P(ch);
            Serial.print("Raw Pressure ADC (");
            Serial.print(ch);
            Serial.print("): ");
            Serial.println(P_sensor[ch].raw);
          }
          digitalWrite(STATUS0_PIN, LOW);
          led0 = LOW;
        }
      }
    }
  } // not started
}
