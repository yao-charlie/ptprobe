#ifndef sensordata_h_
#define sensordata_h_

#include "config.h"
#include "temperaturesensors.h"

struct TSensorData
{
  TSensorData() : T(0), Tref(0), fault(0), ndx(-1), sensor(nullptr) {}
  float T;
  float Tref;
  int8_t fault;
  int8_t ndx;
  TemperatureSensors::MAX31850 const* sensor;
};

struct PSensorData
{
  PSensorData() : raw(0), P(0) 
  {
    // from that one test -- raw 0-1 to kPa
    ai[0] = -97.35308;
    ai[1] =  920.6867;
    ai[2] = -14.86687;
  }
  float raw;
  float P;
  float ai[3];
};


void init_T_sensors(int8_t ndata, TSensorData* data, TemperatureSensors const& probes);
void read_all_T(int8_t ndata, TSensorData* data, TemperatureSensors& probes); 
uint32_t one_shot_T(TSensorData& data, TemperatureSensors& probes); 
void read_all_P(int8_t ndata, PSensorData* data);
inline void one_shot_P(int const ch, PSensorData& data)
{
  static int const P_sensor_pin[] = {A0, A1, A2, A3};

  if (ch >= 0 && ch < 4) {
    auto const val = float(analogRead(P_sensor_pin[ch]))/4095.0;
    data.raw = val;
    data.P = data.ai[0] + val*(data.ai[1] + val*data.ai[2]);
  }
}


#endif // sensordata_h_
