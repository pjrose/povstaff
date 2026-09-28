#pragma once
#include <math.h>
#include <stdint.h>

// Hardware-independent safety math, shared by firmware and host tests.
namespace EngineMath {
enum class PowerSource { Invalid, Battery, USB };
inline PowerSource classifySupply(float volts, bool usbPresent, float batteryMax=4.30f,
                                 float usbMin=4.45f, float usbMax=5.50f) {
  if (!isfinite(volts)) return PowerSource::Invalid;
  if (volts>=2.0f && volts<=batteryMax) return PowerSource::Battery;
  if (usbPresent && volts>=usbMin && volts<=usbMax) return PowerSource::USB;
  return PowerSource::Invalid;
}
inline float clamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float stateOfCharge(float voltage) {
  static const float volts[] = {3.20f,3.30f,3.40f,3.50f,3.60f,3.70f,3.80f,3.90f,4.00f,4.10f,4.20f};
  static const float percent[] = {0,2,7,15,25,40,60,75,88,96,100};
  if (!isfinite(voltage) || voltage <= volts[0]) return 0;
  for (unsigned i = 1; i < 11; ++i)
    if (voltage <= volts[i]) return percent[i-1] + (percent[i]-percent[i-1]) *
      (voltage-volts[i-1]) / (volts[i]-volts[i-1]);
  return 100;
}
inline unsigned batteryPixels(float percent, unsigned count) {
  return unsigned(clamp(percent, 0, 100) * count / 100.0f + 0.5f);
}
inline float limiter(float demandMa, float budgetMa, float previous, float elapsedSeconds) {
  const float target = demandMa > budgetMa && demandMa > 0 ? budgetMa / demandMa : 1;
  // Never smooth the attack: the very first over-budget frame must be limited.
  if (target < previous) return target;
  return previous + (target - previous) * (1 - expf(-clamp(elapsedSeconds,0,1) / 0.6f));
}
inline uint32_t advanceLines(float speed, uint32_t elapsedUs, uint16_t width, float& angle) {
  if (!width || !isfinite(speed) || speed <= 0) return 0;
  // A stalled web request cannot create an unbounded catch-up workload.
  angle += speed * (elapsedUs > 50000 ? 50000 : elapsedUs) * 0.000001f;
  const float degreesPerLine = 360.0f / width;
  const uint32_t steps = uint32_t(angle / degreesPerLine);
  angle -= steps * degreesPerLine;
  return steps;
}
}
