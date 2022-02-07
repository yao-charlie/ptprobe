#include "sensordata.h"


inline void update_sensor_T(
  TSensorData& data, 
  TemperatureSensors::MAX31850 const* probe, 
  int8_t const result)
{
  if (result == 0) {        // data ready
    data.T = probe->probe_T_;
    data.Tref = probe->ref_T_;
    data.fault = 0;
  } else if (result > 0) {  // fault
    data.fault = probe->fault_status_;
    data.Tref = probe->ref_T_;
  } else if (result < 0) {  // error
    data.fault = result;
  }
}


void init_T_sensors(int8_t ndata, TSensorData* data, TemperatureSensors const& probes)
{
  //Serial.print("Init ");
  //Serial.print(probes.sensor_count());
  //Serial.println(" T sensors");
  for (int8_t i = 0; i < probes.sensor_count(); ++i) {
    auto const* probe = probes.get_sensor(i);
    auto const ich = probe->id_;
    if (ich >= 0 && ich < ndata) {
      data[ich].ndx = i;
      data[ich].sensor = probe;
      //Serial.print("  ");
      //Serial.print(ich);
      //Serial.print(" at index ");
      //Serial.println(data[ich].ndx);
    }
  }
}

void read_all_T(int8_t ndata, TSensorData* data, TemperatureSensors& probes) 
{
  for (int i = 0; i < probes.sensor_count(); ++i) {
    int8_t const result = probes.read_scratchpad(i);
    auto const* probe = probes.get_sensor(i);
    auto const ch_id = probe->id_;
    if (ch_id >= 0 && ch_id < ndata) {
      update_sensor_T(data[ch_id], probe, result);
    }
  }
}

void read_all_P(int8_t ndata, PSensorData* data)
{
  for (int i = 0; i < ndata; ++i) {
    one_shot_P(i, data[i]);
  }
}


uint32_t one_shot_T(
  TSensorData& data, 
  TemperatureSensors& probes) 
{
  auto const ndx = data.ndx;
  if (ndx < 0) {
    return 0;
  }
  
  if (!probes.start_conversion(ndx)) {
    digitalWrite(STATUS1_PIN, HIGH);
    return 0;
  }
  uint32_t t_start_one_shot = millis();
  do {
    delay(2);
  } while(!probes.conversion_complete(ndx));
  uint32_t const t_one_shot = millis()-t_start_one_shot;
    
  int8_t const result = probes.read_scratchpad(ndx);
  auto const* probe = probes.get_sensor(ndx);
  update_sensor_T(data, probe, result);
  
  return t_one_shot;
}
