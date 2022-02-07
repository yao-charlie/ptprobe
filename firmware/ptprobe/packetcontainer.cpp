#include "packetcontainer.h"
#include <Arduino.h>

#define T_GROUP_BYTE 5
#define P_GROUP_BYTE 22
#define T_REF_GROUP_BYTE 39

/*!
 \brief write data packet to buffer, increment count
 
 b0: HDR_TYPE_DATA | byte_count (excl header)
 b1-b4: timestamp (ms) -- uint32_t
 b5: T ch active (upper 4 bits) | T ch error (lower 4 bits)
 b6-b9: T0 (float)
 ...
 b22: P ch active (upper 4 bits) | P ch error (lower 4 bits)
 b23-b26: P0 (float)
 ... 
 b39: T ref ch active (upper 4 bits) | T ref ch error (lower 4 bits)
 b40-b43: Tref0 
 ...
 b52-b55: Tref3

 \returns number of bytes in buffer
*/
int8_t PacketContainer::write_data(uint8_t nch_T, TSensorData const* Tdata, uint8_t nch_P, PSensorData const* Pdata)
{
  buf[0] = 0;             // clear count
  buf[T_GROUP_BYTE] = 0;  // clear T active
  buf[P_GROUP_BYTE] = 0;  // clear P active
  buf[T_REF_GROUP_BYTE] = 0;  // clear T ref active

  uint32_t const timestamp = millis();
  write_to_buf(timestamp, &buf[1]);
  for (uint8_t ich = 0; ich < nch_T; ++ich) {
    if (Tdata[ich].ndx > 0) { 
      if (Tdata[ich].fault == 0) {
        write_val(T_GROUP_BYTE,ich,Tdata[ich].T);
      } else {
        write_err(T_GROUP_BYTE,ich,Tdata[ich].fault);
      }
      write_val(T_REF_GROUP_BYTE,ich,Tdata[ich].Tref);
    }
  }
  for (uint8_t ich = 0; ich < nch_P; ++ich) {
    write_val(P_GROUP_BYTE,ich,Pdata[ich].P);
  }
  buf[0] = 55;
  buf[0] |= (HDR_TYPE_DATA << 6);

  count++;
  
  return 55;
}

/*!
    \brief value response packet

    Header byte: HHTTTCCE
    - HH: header type, HDR_TYPE_RESP 
    - TTT: response type RESP_TYPE_*
    - CC: channel
    - E: error flag

    \returns byte count
 */
int8_t PacketContainer::write_resp(uint8_t resp_type, uint8_t ch, float val, int32_t err)
{
  buf[0] = (HDR_TYPE_RESP << 6) | (resp_type << 3) | (0x03 & ch) << 1 | (err == 0 ? 0x0 : 0x1);
  if (err != 0) {
    *((int32_t*)&buf[1]) = err;
  } else {
    *((float*)&buf[1]) = val;
  }
  return 5;
}


int8_t PacketContainer::write_status_T(int8_t const ch, TSensorData const& sensor)
{
  buf[0] = (HDR_TYPE_RESP << 6) | (RESP_TYPE_STATUS_T << 3) | (0x03 & ch) << 1 | (sensor.ndx < 0 ? 0x1 : 0x0);
  int8_t pos = 1;

  if (sensor.ndx < 0 || sensor.sensor == nullptr) {
    buf[pos++] = 0xFF;
  } else {
    buf[pos++] = sensor.sensor->id_;
    buf[pos++] = (uint8_t)sensor.sensor->fault_status_;
    for (int i = 0; i < 8; ++i) {
      buf[pos++] = sensor.sensor->addr_[i];
    }
  }
  return pos;
}
  
int8_t PacketContainer::write_status_P(int8_t const ch, PSensorData const& sensor)
{
  buf[0] = (HDR_TYPE_RESP << 6) | (RESP_TYPE_STATUS_P << 3) | (0x03 & ch) << 1;
  int8_t pos = 1;
  buf[pos++] = ch;
  for (int i = 0; i < 3; ++i) {
    *((float*)buf[pos]) = sensor.ai[i];
    pos += sizeof(float); 
  }
  return pos;
}
