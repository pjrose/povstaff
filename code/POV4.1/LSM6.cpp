#include "LSM6.h"
#include <math.h>

// ST LSM6DSO datasheet: CTRL1_XL=0x48 (104Hz,+/-4g),
// CTRL2_G=0x4c (104Hz,+/-2000dps), CTRL3_C=0x44 (BDU + IF_INC).
// https://www.st.com/resource/en/datasheet/lsm6dso.pdf
static constexpr float A_SENS = 0.000122f;
static constexpr float G_SENS = 0.070f;
LSM6::LSM6() : a{0,0,0}, g{0,0,0}, last_status(0), bus(&Wire), _device(device_auto), address(0) {}
bool LSM6::init(deviceType device, sa0State sa0) {
  for (uint8_t candidate = 0x6a; candidate <= 0x6b; ++candidate) {
    if ((sa0 == sa0_low && candidate != 0x6a) || (sa0 == sa0_high && candidate != 0x6b)) continue;
    int16_t id = testReg(candidate, WHO_AM_I);
    if ((id == 0x6c && (device == device_auto || device == device_DSO)) ||
        (id == 0x69 && (device == device_auto || device == device_DS33))) {
      address = candidate;
      _device = id == 0x6c ? device_DSO : device_DS33;
      return true;
    }
  }
  _device = device_auto;
  return false;
}
void LSM6::writeReg(uint8_t reg, uint8_t value) {
  bus->beginTransmission(address);
  bus->write(reg); bus->write(value);
  last_status = bus->endTransmission();
}
uint8_t LSM6::readReg(uint8_t reg) {
  const int16_t value = testReg(address, static_cast<regAddr>(reg));
  return value < 0 ? 0 : uint8_t(value);
}
int16_t LSM6::testReg(uint8_t target, regAddr reg) {
  bus->beginTransmission(target); bus->write(uint8_t(reg));
  last_status = bus->endTransmission(false);
  if (last_status) return -1;
  if (bus->requestFrom(target, uint8_t(1)) != 1) { last_status = 5; return -1; }
  last_status = 0;
  return bus->read();
}
bool LSM6::configure() {
  if (_device == device_auto) return false;
  writeReg(CTRL3_C, 0x44); if (last_status) return false;
  writeReg(CTRL1_XL, 0x48); if (last_status) return false;
  writeReg(CTRL2_G, 0x4c); if (last_status) return false;
  return readReg(CTRL3_C) == 0x44 && !last_status &&
         readReg(CTRL1_XL) == 0x48 && !last_status &&
         readReg(CTRL2_G) == 0x4c && !last_status;
}
void LSM6::enableDefault() { configure(); }
bool LSM6::readSample() {
  bus->beginTransmission(address); bus->write(uint8_t(OUTX_L_G));
  last_status = bus->endTransmission(false);
  if (last_status) return false;
  if (bus->requestFrom(address, uint8_t(12)) != 12) {
    while (bus->available()) bus->read();
    last_status = 5;
    return false;
  }
  int16_t values[6];
  for (unsigned i=0; i<6; ++i) {
    const uint8_t lo=bus->read(), hi=bus->read();
    values[i]=int16_t((uint16_t(hi)<<8)|lo);
  }
  g = {values[0],values[1],values[2]};
  a = {values[3],values[4],values[5]};
  last_status=0;
  return true;
}
void LSM6::read() { readSample(); }
void LSM6::readAcc() { readSample(); }
void LSM6::readGyro() { readSample(); }
float LSM6::getSpeed() {
  // Cast before multiply: three int16 squares can overflow signed int32.
  return sqrtf(float(g.x)*g.x + float(g.y)*g.y + float(g.z)*g.z)*G_SENS;
}
bool LSM6::isHorizontal() { return fabsf(float(a.x))*A_SENS < 0.25f; }
bool LSM6::isVertical() { return fabsf(float(a.x))*A_SENS > 0.75f; }
void LSM6::vector_normalize(vector<float>* v) {
  const float mag=sqrtf(vector_dot(v,v));
  if (mag>0) { v->x/=mag; v->y/=mag; v->z/=mag; }
}
