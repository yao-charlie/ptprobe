#ifndef packet_container_h_
#define packet_container_h_

#include "sensordata.h"

#define HDR_TYPE_DATA 0x1
#define HDR_TYPE_RESP 0x2
#define HDR_TYPE_HALT 0x3

#define RESP_TYPE_RSVD 0x0
#define RESP_TYPE_ID   0x1
#define RESP_TYPE_T    0x2
#define RESP_TYPE_P    0x3
#define RESP_TYPE_TREF 0x4
#define RESP_TYPE_ADC  0x5
#define RESP_TYPE_STATUS_T  0x6
#define RESP_TYPE_STATUS_P  0x7

#define MAX_PACKET_LENGTH 64


class PacketContainer 
{
public:
  PacketContainer() : count(0), max_packets(0) {}

  uint32_t count;
  uint32_t max_packets;
  uint8_t const* buffer() const { return &buf[0]; }

  int8_t write_data(uint8_t nch_T, TSensorData const* Tdata, uint8_t nch_P, PSensorData const* Pdata);
  int8_t write_resp(uint8_t resp_type, uint8_t ch, float val, int32_t err = 0);
  int8_t write_halt() 
  {
    buf[0] = HDR_TYPE_HALT << 6;
    write_to_buf(count, &buf[1]);
    return 5;
  }
  int8_t write_status_T(int8_t ch, TSensorData const& sensor);
  int8_t write_status_P(int8_t ch, PSensorData const& sensor);
  
private:
  uint8_t buf[MAX_PACKET_LENGTH];

  template<typename T>
  void write_to_buf(T v, uint8_t* start) 
  {
    uint8_t const* bvals = reinterpret_cast<uint8_t const*>(&v);
    for (int8_t i = 0; i < sizeof(T); ++i) {
      *start++ = bvals[sizeof(T)-1-i];   // MSB first
    }
  }

  void write_val(uint8_t start, uint8_t ch, float val)
  {
    write_to_buf(val,&buf[start+1+sizeof(float)*ch]);
    buf[start] |= (1 << ch+4); // active
    buf[start] &= ~(1 << ch);  // no error
  }
  void write_err(uint8_t start, uint8_t ch, int32_t err)
  {
    write_to_buf(err,&buf[start+1+sizeof(int32_t)*ch]);
    buf[start] |= (1 << ch+4); // active
    buf[start] |= (1 << ch);   // error flag
  }

};

#endif // packet_container_h_
