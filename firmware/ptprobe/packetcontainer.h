#ifndef packet_container_h_
#define packet_container_h_

#define HDR_TYPE_DATA (0x1 << 6)
#define HDR_TYPE_RESP (0x2 << 6)
#define RESP_TYPE_ID (0x0 << 4)
#define RESP_TYPE_T (0x1 << 4)
#define RESP_TYPE_P (0x2 << 4)
#define RESP_TYPE_T_REF (0x3 << 4)
#define HDR_TYPE_HALT (0x3 << 6)

#define PACKET_LENGTH 64

// data packets
// b0: HDR_TYPE_DATA | byte_count (excl header)
// b1-b4: timestamp (ms) -- uint32_t
// b5: T ch active (upper 4 bits) | T ch error (lower 4 bits)
// b6-b9: T0 (float)
// ...
// b22: P ch active (upper 4 bits) | P ch error (lower 4 bits)
// b23-b26: P0 (float)
// ... 
// b39: T ref ch active (upper 4 bits) | T ref ch error (lower 4 bits)
// b40-b43: Tref0 
// ...
// b52-b55: Tref3

#define T_GROUP_BYTE 5
#define P_GROUP_BYTE 22
#define T_REF_GROUP_BYTE 39

class PacketContainer 
{
public:
  PacketContainer() : pos(0), count(0) {}
  
  uint8_t buf[PACKET_LENGTH];
  uint8_t pos;
  uint32_t count;

  void reset_for_write() 
  {
    buf[0] = 0;             // clear count
    buf[T_GROUP_BYTE] = 0;  // clear T active
    buf[P_GROUP_BYTE] = 0;  // clear P active
    buf[T_REF_GROUP_BYTE] = 0;  // clear T ref active
  }

  void write_T(uint8_t ch, float val)       { write_val(T_GROUP_BYTE,ch,val); }
  void write_T_err(uint8_t ch, int32_t err) { write_err(T_GROUP_BYTE,ch,err); }

  void write_P(uint8_t ch, float val)       { write_val(P_GROUP_BYTE,ch,val); }
  void write_P_err(uint8_t ch, int32_t err) { write_err(P_GROUP_BYTE,ch,err); }

  void write_Tref(uint8_t ch, float val)       { write_val(T_REF_GROUP_BYTE,ch,val); }
  void write_Tref_err(uint8_t ch, int32_t err) { write_err(T_REF_GROUP_BYTE,ch,err); }

  void finalize_data() 
  {
    buf[0] = 55;
    buf[0] |= HDR_TYPE_DATA;
  }

  void write_T_resp(uint8_t ch, float val, int32_t err=0)     { write_resp(RESP_TYPE_T, ch, val, err); }
  void write_P_resp(uint8_t ch, float val, int32_t err=0)     { write_resp(RESP_TYPE_P, ch, val, err); }
  void write_Tref_resp(uint8_t ch, float val, int32_t err=0)  { write_resp(RESP_TYPE_T_REF, ch, val, err); }

  void write_halt() 
  {
    buf[0] = HDR_TYPE_HALT;
    *((uint32_t*)&buf[1]) = count;
  }
private:
  void write_val(uint8_t start, uint8_t ch, float val)
  {
    *((float*)&buf[start + 1 + sizeof(float)*ch]) = val;
    buf[start] |= (1 << ch+4); // active
    buf[start] &= ~(1 << ch);  // no error
  }
  void write_err(uint8_t start, uint8_t ch, int32_t err)
  {
    *((int32_t*)&buf[start + 1 + sizeof(int32_t)*ch]) = err;
    buf[start] |= (1 << ch+4); // active
    buf[start] |= (1 << ch);   // error flag
  }
  void write_resp(uint8_t resp_type, uint8_t ch, float val, int32_t err)
  {
    buf[0] = HDR_TYPE_RESP | resp_type | (err == 0 ? 0x0 : 0x1) << 2 | (0x03 & ch);
    if (err != 0) {
      *((int32_t*)&buf[1]) = err;
    } else {
      *((float*)&buf[1]) = val;
    }
  }

};

static PacketContainer packet;

#endif // packet_container_h_
