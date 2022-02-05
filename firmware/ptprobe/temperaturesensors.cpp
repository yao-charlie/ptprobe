#include "temperaturesensors.h"

OneWire* TemperatureSensors::bus_ = nullptr;  // init static

TemperatureSensors::TemperatureSensors(int const bus_pin) 
  : sensor_count_(0)
{
  if (!bus_) {
    bus_ = new OneWire(bus_pin);
  }
}

bool TemperatureSensors::start_conversion(int device_ndx /*= -1*/) 
{
  if (bus_->reset()) {
    if (device_ndx < 0) {
      bus_->skip();       // address all
    } else if (device_ndx < sensor_count_) {
      bus_->select(device_table_[device_ndx].addr_);
    } else {
      return false;
    }
    bus_->write(0x44);  // start conversion
    return true;
  }
  return false;
}

int8_t TemperatureSensors::read_scratchpad(int device_ndx) 
{
  if ((device_ndx < 0) || (device_ndx >= sensor_count_)) {
    return -3;
  }
  if (bus_->reset() == 0) {
    return -2;
  }
  bus_->select(device_table_[device_ndx].addr_);
  bus_->write(0xBE);

  uint8_t data[12];
  bus_->read_bytes(&data[0], 9);
  
  if (OneWire::crc8(data, 8) != data[8]) {
    return -1;
  }

  int16_t raw_probe_T = (data[1] << 8) | data[0];
  int16_t raw_ref_T = (data[3] << 8) | data[2];

  if (raw_probe_T & 0x01) { // fault
    device_table_[device_ndx].fault_status_ = (MAX31850::Fault)(0x07 & raw_ref_T);
  } 

  raw_probe_T >>= 2;
  raw_ref_T >>=4;

  device_table_[device_ndx].probe_T_ = (float)raw_probe_T/4.0;
  device_table_[device_ndx].ref_T_ = (float)raw_ref_T/16.0;

  device_table_[device_ndx].id_ = data[4] & 0x0F;

  return (uint8_t)device_table_[device_ndx].fault_status_;
}

bool TemperatureSensors::conversion_complete(int device_ndx /*= -1*/) 
{
  return static_cast<bool>(bus_->read_bit());
}

int TemperatureSensors::get_sensor_ndx(int const device_id) const
{
  for (int i = 0; i < sensor_count_; ++i) {
    if (device_table_[i].id_ == device_id) {
      return i;
    }
  }
  return -1;
}

TemperatureSensors::MAX31850 const* TemperatureSensors::get_sensor(int const device_ndx) const
{
  if (device_ndx < 0 || device_ndx >= sensor_count_) {
    return nullptr;
  }
  return &device_table_[device_ndx];
}

TemperatureSensors::MAX31850 const* TemperatureSensors::get_sensor_by_addr(int const device_id) const
{
  for (int i = 0; i < sensor_count_; ++i) {
    if (device_table_[i].id_ == device_id) {
      return &device_table_[i];
    }
  }
  return nullptr;
}

void TemperatureSensors::begin() 
{
  uint8_t addr[8];

  sensor_count_ = 0;
  while ((sensor_count_ < MAX_SENSOR_COUNT) && bus_->search(addr)) {
    /*
    Serial.print("ROM =");
    for(int i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);
    }
    */
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println(" CRC is not valid!");
      continue;
    }
    if (addr[0] != 0x3B) {
      Serial.println(" Invalid sensor type (expecting 0x3B)");
      continue;
    }
    //Serial.println();

    // add to address table
    for(int i = 0; i < 8; i++) {
      device_table_[sensor_count_].addr_[i] = addr[i];
    }
    ++sensor_count_;
  }
  bus_->reset_search();

  // recover device IDs
  for (int device_ndx = 0; device_ndx < sensor_count_; ++device_ndx) {
    if (bus_->reset() == 0) {
      Serial.println(" Failed bus reset reading scratchpad");
      continue;
    }
    bus_->select(device_table_[device_ndx].addr_);
    bus_->write(0xBE);

    uint8_t data[12];
    bus_->read_bytes(&data[0], 9);
  
    if (OneWire::crc8(data, 8) != data[8]) {
      Serial.println(" CRC is not valid reading scratchpad");
      continue;
    }
    device_table_[device_ndx].id_ = data[4] & 0x0F;
  }
}
