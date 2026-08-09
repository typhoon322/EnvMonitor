#pragma once

#include <Arduino.h>

#include "config.h"

struct AirQualityReading {
  float temperatureC = NAN;
  float humidityPct = NAN;
  uint16_t eco2Ppm = 0;
  uint16_t tvocPpb = 0;
  uint8_t aqiUba = 0;
  bool ens160Ready = false;
};

class AirQualitySensor {
public:
  bool begin();
  bool ready() const { return initialized_; }
  bool read(AirQualityReading &out);
  bool isValidReading(const AirQualityReading &reading) const;
  bool hasFault() const { return faultActive_; }
  const char *stateText() const;
  bool recover();

private:
  bool initDevices_();
  bool readAht20_(float &tempC, float &humPct);
  bool readEns160_(AirQualityReading &out, float tempC, float humPct);
  void pushFilter_(float tempC, float humPct, uint16_t eco2, uint16_t tvoc, uint8_t aqi);
  void applyFilter_(AirQualityReading &out) const;

  bool initialized_ = false;
  bool faultActive_ = false;
  uint8_t ens160Flags_ = 3;

  // Hold last gas sample: ENS160 NEWDAT is ~1 Hz; missing a poll must not
  // zero the UI or flip ens160Ready to "warming up".
  bool hasGasSample_ = false;
  uint16_t lastEco2Ppm_ = 0;
  uint16_t lastTvocPpb_ = 0;
  uint8_t lastAqiUba_ = 0;
  uint32_t lastCompensationMs_ = 0;

  float tempBuf_[SENSOR_FILTER_SAMPLES] = {};
  float humBuf_[SENSOR_FILTER_SAMPLES] = {};
  float eco2Buf_[SENSOR_FILTER_SAMPLES] = {};
  float tvocBuf_[SENSOR_FILTER_SAMPLES] = {};
  uint8_t filterIdx_ = 0;
  uint8_t filterCount_ = 0;
};
