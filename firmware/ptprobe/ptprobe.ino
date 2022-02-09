#include <Bounce2.h>
#include "displaymanager.h"
#include "temperaturesensors.h"
#include "packetcontainer.h"
#include "config.h"
#include "sensordata.h"

// objects
Bounce next_button;
Bounce apply_button;
DisplayManager display; // OLED Display
PacketContainer packet;
TemperatureSensors probes_T(ONE_WIRE_BUS);
TSensorData T_sensor[4];
PSensorData P_sensor[4];

// functions
void process_serial_buffer();
void update_T_display(int const ich);
void update_P_display(int const ich);

#define MSG_BUF_LEN 24
char msg_buffer[MSG_BUF_LEN];
void process_serial_buffer(bool force_halt);

void setup() 
{
  // ADC setup
  analogReadResolution(12);

  // LED pins
  pinMode(STATUS0_PIN, OUTPUT);
  pinMode(STATUS1_PIN, OUTPUT);

  // Buttons with debounce
  next_button.attach(NEXT_BUTTON_PIN, INPUT_PULLUP);
  next_button.interval(50);

  apply_button.attach(APPLY_BUTTON_PIN, INPUT_PULLUP);
  apply_button.interval(50);

  // Serial port
  Serial.begin(115200);

  delay(2000);

  // Initialize temperature probes
  probes_T.begin();
  init_T_sensors(4, T_sensor, probes_T);

  // Splash screen
  char buf[16]; // "fw ver #.#"
  sprintf(buf,"fw ver %d.%d",FW_VERSION_MAJOR, FW_VERSION_MINOR);
  if(!display.begin("PT Probe", &buf[0])) {
    cfg.report_fault("SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  }

  // Wink LEDs
  for (int i = 0; i < 3; ++i) {
    digitalWrite(STATUS0_PIN, HIGH);
    digitalWrite(STATUS1_PIN, LOW);
    delay(400);
    digitalWrite(STATUS0_PIN, LOW);
    digitalWrite(STATUS1_PIN, HIGH);
    delay(400);
  }
  digitalWrite(STATUS1_PIN, LOW);

  // Reset data display
  display.clear_all();
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
}

void loop() 
{
  process_serial_buffer(cfg.conversion_state == 3);
  
  if (cfg.started) {
    if (cfg.conversion_state == 0) {
      // activity
      cfg.toggle_led0();
      
      if (probes_T.start_conversion()) {
        cfg.conversion_state = 1;
      } else {
        cfg.report_fault("Failed to start conversion");
        cfg.started = false;
      }
    } else if ((cfg.conversion_state == 1) && (probes_T.conversion_complete())) {
      cfg.conversion_state = 2;
    } else if (cfg.conversion_state == 2) {
      read_all_T(4, T_sensor, probes_T);
      read_all_P(4, P_sensor);

      auto const byte_count = packet.write_data(4,T_sensor,4,P_sensor);
      //Serial.println(byte_count);
      if (cfg.debug_level < 1) {
        Serial.write(packet.buffer(), byte_count);
      }
      
      for (int i = 0; i < 4; ++i) {
        update_T_display(i);
        update_P_display(i);
      }
      display.show();

      if ((packet.max_packets == 0) || (packet.count < packet.max_packets)) {
        cfg.conversion_state = 0;
      } else {
        cfg.conversion_state = 3;
      }
    }
  }

  next_button.update();
  if (next_button.fell()) {
    cfg.set_led1(LOW);
  }

  apply_button.update();
  if (apply_button.fell()) {
    cfg.set_led1(HIGH);
  }

  delay(10);

}


void debug_print_config() 
{
  if (cfg.debug_level > 0) {
    Serial.println("Configuration updated");
    Serial.print("  + Debug level: ");
    Serial.println(cfg.debug_level);
  
    Serial.print("  + Board ID: 0x");
    Serial.println(cfg.board_id, HEX);
    
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
}

void debug_print_status_T(int8_t ch, TSensorData const& sensor) 
{
  Serial.print("T sensor ");
  Serial.println(ch);
  if (sensor.ndx < 0 || sensor.sensor == nullptr) {
    Serial.println("  + disconnected");
  } else {
    Serial.print("  + ID: ");
    Serial.println(sensor.sensor->id_);
    Serial.print("  + Fault: 0x");
    Serial.println((uint8_t)sensor.sensor->fault_status_, HEX);
    Serial.print("  + ROM Addr:");
    for (int i = 0; i < 8; ++i) {
      Serial.print(" ");
      Serial.print(sensor.sensor->addr_[i], HEX);
    }
    Serial.println();
  }
}

void debug_print_status_P(int8_t ch, PSensorData const& sensor) 
{
  Serial.print("P sensor ");
  Serial.println(ch);
  Serial.print("  + ID: ");
  Serial.println(ch);
  Serial.print("  + Coeff:");
  for (int i = 0; i < 3; ++i) {
    Serial.print(" a");
    Serial.print(i);
    Serial.print("=");
    Serial.print(sensor.ai[i]);
  }
  Serial.println();
}


void respond_ask_T(
  uint8_t response_type,
  int8_t const ch) 
{
  if (cfg.debug_level > 0) {
    Serial.print("T"); 
    if (response_type == RESP_TYPE_TREF) {
      Serial.print("ref");
    }
    Serial.print(ch);
    Serial.print(": ");
  }
  if (T_sensor[ch].ndx < 0) {   // not connected
    if (cfg.debug_level < 1) {
      auto const len = packet.write_resp(response_type, ch, 0.0, TemperatureSensors::ERROR_NDX_OUT_OF_RANGE);
      Serial.write(packet.buffer(), len);
    } else {
      Serial.println("not connected");
    }
  } else {
    float const val = response_type == RESP_TYPE_T ? T_sensor[ch].T : T_sensor[ch].Tref;
    if (cfg.debug_level < 1) {
      auto const len = packet.write_resp(response_type, ch, val, T_sensor[ch].fault);
      Serial.write(packet.buffer(), len);
    } else if (T_sensor[ch].fault == 0) {
      Serial.println(val);
    } else {
      Serial.println(TemperatureSensors::error_short_label(T_sensor[ch].fault));
    }
  }
}

void respond_ask_P(
  uint8_t response_type,
  int8_t const ch) 
{
  if (cfg.debug_level > 0) {
    Serial.print("P"); 
    if (response_type == RESP_TYPE_ADC) {
      Serial.print("raw");
    }
    Serial.print(ch);
    Serial.print(": ");
  }

  float const val = response_type == RESP_TYPE_P ? P_sensor[ch].P : P_sensor[ch].raw;
  if (cfg.debug_level < 1) {
    packet.write_resp(response_type, ch, val, 0);
  } else {
    Serial.println(val);
  }
}

// Message format
// H : halt, send last packet then halt packet
// R# : start with max packets count as string (0=no max), streams packets HDR_DATA | timestamp_ms | T .. | P ..
// C : configure
//  Pca## : pressure channel c, coefficient a (0=cte, 1=lin, 2=quad), ## as float string 
//  D# : serial debug level (0-off, 1-on)
//  B# : board ID (with 32 bit ID)
//  W : write configuration to flash 
// A : ask
//  B  : board ID, returns ACK | ID_TYPE | 32bit ID
//  T# : temperature on channel #, returns ACK | PROBE_T_TYPE | CH# | ERR Flag | 32 bit float
//  R# : ref. temperature on channel #, returns ACK | REF_T_TYPE | 32 bit float
//  A# : raw ADC value for pressure channel # , returns ACK | RAW_P_TYPE | 32 bit float 
//  P# : pressure from channel # , returns ACK | PROBE_P_TYPE | 32 bit float 
//  S[T|P]# : status/configuration on sensor
void process_serial_buffer(bool force_halt)
{
  char const data_in = Serial.read();
  if (force_halt || (data_in == 'H')) {
    if (cfg.started) {
      cfg.started = false;

      cfg.set_led0(LOW);
      if (cfg.debug_level > 0) {
        Serial.println("Halting");
      } else {
        auto const pktlen = packet.write_halt();
        Serial.write(packet.buffer(), pktlen);
      }

      display.data_rect(3,0).update_lbl_lo("OFF",false);
      display.show();
    }
  } else if (!cfg.started) {
    if (data_in == 'R') {
      int const rlen = Serial.readBytesUntil('\n', msg_buffer, MSG_BUF_LEN-1);
      if (rlen > 0) {
        int const cfg_val = atoi(&msg_buffer[0]);
        packet.max_packets = cfg_val >= 0 ? cfg_val : 0;
      } else {
        packet.max_packets = 0;
      }
      packet.count = 0;
      
      cfg.started = true;
      cfg.conversion_state = 0;
      
      display.data_rect(3,0).update_lbl_lo("RUN",true);
      // display will update in loop
      
      if (cfg.debug_level > 0) {
        Serial.print("Starting (");
        Serial.print(packet.max_packets);
        Serial.println(" samples)");
      }
    } else if (data_in == 'C') {
      int const rlen = Serial.readBytesUntil('\n', msg_buffer, MSG_BUF_LEN-1);
      msg_buffer[rlen] = '\0'; 
      if ((msg_buffer[0] == 'D') && (rlen > 1)) {  // debug level
        int8_t const dbg = msg_buffer[1] - '0';
        if (dbg >= 0 && dbg < 3) {
          cfg.debug_level = dbg;
        }
      } else if ((msg_buffer[0] == 'P') && (rlen > 4)) {
        int8_t const P_ch = msg_buffer[1] - '0';
        int8_t const icoeff = msg_buffer[2] - '0';
        if ((P_ch >= 0) && (P_ch < 4) && (icoeff >=0) && (icoeff < 3)) {
          P_sensor[P_ch].ai[icoeff] = atof(&msg_buffer[3]);
        }
      }
      debug_print_config();
    } else if (data_in == 'A') {  // ask
      int const rlen = Serial.readBytesUntil('\n', msg_buffer, MSG_BUF_LEN-1);
      msg_buffer[rlen] = '\0'; 
      if (rlen > 2) {
        if (msg_buffer[0] == 'S') {
          int const ch = msg_buffer[2] - '0';
          if (cfg.debug_level == 0) {
            int8_t slen = 0;
            if (msg_buffer[1] == 'T') { // status on T probe
              slen = packet.write_status_T(ch, T_sensor[ch]);
            } else if (msg_buffer[1] == 'P') { // status on P probe
              slen = packet.write_status_P(ch, P_sensor[ch]);
            }
            Serial.write(packet.buffer(), slen);    
          } else {
            if (msg_buffer[1] == 'T') { // status on T probe
              debug_print_status_T(ch, T_sensor[ch]);
            } else if (msg_buffer[1] == 'P') { // status on P probe
              debug_print_status_P(ch, P_sensor[ch]);
            }
          }
        }
      } else if (rlen == 2) { // ask sensor values
        int8_t const ch = msg_buffer[1] - '0';
        if (ch >= 0 && ch < 4) {
          cfg.set_led0(HIGH);
          if ((msg_buffer[0] == 'T') || (msg_buffer[0] == 'R')) {
            one_shot_T(T_sensor[ch], probes_T);
            respond_ask_T(msg_buffer[0] == 'T' ? RESP_TYPE_T : RESP_TYPE_TREF, ch);
            update_T_display(ch);
            display.show();
          } else if ((msg_buffer[0] == 'P') || (msg_buffer[0] == 'A'))  {
            one_shot_P(ch, P_sensor[ch]);
            respond_ask_P(msg_buffer[0] == 'P' ? RESP_TYPE_P : RESP_TYPE_ADC, ch);
            update_P_display(ch);
            display.show();
          }
          cfg.set_led0(LOW);      
        }  
      } else if (rlen == 1) {
        if (msg_buffer[0] == 'B') {
          if (cfg.debug_level == 0) {
            // TODO
          } else {
            Serial.print("Board ID 0x");
            Serial.println(cfg.board_id, HEX);
          }
        }        
      }
    }
  } // not started
}


void update_T_display(int const ich) 
{
  auto const& Tdata = T_sensor[ich];
  if (Tdata.fault != 0) {
    display.data_rect(ich,0).update_data(TemperatureSensors::error_short_label(Tdata.fault),true);
  } else {
    display.data_rect(ich,0).update_data(Tdata.T);
  }
}

void update_P_display(int const ich)
{
  display.data_rect(ich,1).update_data(P_sensor[ich].P);
}
