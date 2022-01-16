#include "temperaturesensors.h"

OneWire* TemperatureSensors::bus_ = nullptr;  // init static

TemperatureSensors::TemperatureSensors(int const bus_pin) 
  : sensor_count_(0)
{
  if (!bus_) {
    bus_ = new OneWire(bus_pin);
  }
}

void TemperatureSensors::start_conversion(int device_id /*= -1*/) 
{
  bus_->reset();
  if (device_id < 0) {
    bus_->skip();       // address all
  } else if (device_id < sensor_count_) {
    bus_->select(device_table_[device_id].addr_);
  } else {
    return;
  }
  bus_->write(0x44);  // start conversion
}

int8_t TemperatureSensors::read_scratchpad(int device_id) 
{
  if ((device_id < 0) || (device_id >= sensor_count_)) {
    return -2;
  }
  bus_->reset();
  bus_->select(device_table_[device_id].addr_);
  bus_->write(0xBE);

  uint8_t data[12];
  bus_->read_bytes(&data[0], 9);
  
  if (OneWire::crc8(data, 8) != data[8]) {
    return -1;
  }

  int16_t raw_probe_T = (data[1] << 8) | data[0];
  int16_t raw_ref_T = (data[3] << 8) | data[2];

  if (raw_probe_T & 0x01) { // fault
    device_table_[device_id].fault_status_ = (MAX31850::Fault)(0x07 & raw_ref_T);
  } 

  raw_probe_T >>= 2;
  raw_ref_T >>=4;

  device_table_[device_id].probe_T_ = (float)raw_probe_T/4.0;
  device_table_[device_id].ref_T_ = (float)raw_probe_T/16.0;

  device_table_[device_id].id_ = data[4] & 0x0F;

  return (uint8_t)device_table_[device_id].fault_status_;
}

bool TemperatureSensors::conversion_complete(int device_id /*= -1*/) 
{
  if (device_id < 0) {
    for (int i = 0; i < sensor_count_; ++i) {
      uint8_t present = bus_->reset();  
      bus_->select(device_table_[i].addr_);
      if (bus_->read_bit() == 0) { // A device is pulling down the wire, indicating conv in progress
        return false;
      }
    }
  } else if (device_id < sensor_count_) {
     uint8_t present = bus_->reset();  
     bus_->select(device_table_[device_id].addr_);
     return static_cast<bool>(bus_->read_bit());
  }
  return true;
}

TemperatureSensors::MAX31850 const* TemperatureSensors::get_sensor(int device_id) const
{
  if (device_id < 0 || device_id >= sensor_count_) {
    return nullptr;
  }
  return &device_table_[device_id];
}

void TemperatureSensors::begin() 
{
  uint8_t addr[8];

  sensor_count_ = 0;
  while ((sensor_count_ < MAX_SENSOR_COUNT) && bus_->search(addr)) {
    Serial.print("ROM =");
    for(int i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);
    }
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println(" CRC is not valid!");
      continue;
    }
    if (addr[0] != 0x3B) {
      Serial.println(" Invalid sensor type (expecting 0x3B)");
      continue;
    }
    Serial.println();

    // add to address table
    for(int i = 0; i < 8; i++) {
      device_table_[sensor_count_].addr_[i] = addr[i];
    }
    ++sensor_count_;
  }
  bus_->reset_search();
}
