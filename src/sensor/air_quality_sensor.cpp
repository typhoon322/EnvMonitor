#include "sensor/air_quality_sensor.h"

#include <Adafruit_AHTX0.h>
#include <SparkFun_ENS160.h>
#include <Wire.h>

#include "config.h"

namespace {
Adafruit_AHTX0 aht20_;
SparkFun_ENS160 ens160_;
}  // namespace

bool AirQualitySensor::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  Wire.setTimeout(50);

  initialized_ = initDevices_();
  faultActive_ = !initialized_;
  filterIdx_ = 0;
  filterCount_ = 0;
  return initialized_;
}

bool AirQualitySensor::initDevices_() {
  if (!aht20_.begin(&Wire)) {
    Serial.println(F("WARN: AHT20 init failed."));
    return false;
  }

  if (!ens160_.begin()) {
    Serial.println(F("WARN: ENS160 init failed."));
    return false;
  }

  if (!ens160_.setOperatingMode(SFE_ENS160_RESET)) {
    Serial.println(F("WARN: ENS160 reset failed."));
    return false;
  }
  delay(100);

  if (!ens160_.setOperatingMode(SFE_ENS160_IDLE)) {
    Serial.println(F("WARN: ENS160 idle mode failed."));
    return false;
  }

  float tempC = NAN;
  float humPct = NAN;
  if (!readAht20_(tempC, humPct)) {
    tempC = 25.0f;
    humPct = 50.0f;
  }

  ens160_.setTempCompensationCelsius(tempC);
  ens160_.setRHCompensationFloat(humPct);
  delay(100);

  if (!ens160_.setOperatingMode(SFE_ENS160_STANDARD)) {
    Serial.println(F("WARN: ENS160 standard mode failed."));
    return false;
  }

  ens160Flags_ = ens160_.getFlags();
  Serial.print(F("ENS160 flags: "));
  Serial.println(ens160Flags_);
  return true;
}

bool AirQualitySensor::readAht20_(float &tempC, float &humPct) {
  sensors_event_t humidity;
  sensors_event_t temp;
  if (!aht20_.getEvent(&humidity, &temp)) {
    return false;
  }
  tempC = temp.temperature;
  humPct = humidity.relative_humidity;
  return !isnan(tempC) && !isnan(humPct);
}

bool AirQualitySensor::readEns160_(AirQualityReading &out, float tempC, float humPct) {
  ens160_.setTempCompensationCelsius(tempC);
  ens160_.setRHCompensationFloat(humPct);

  ens160Flags_ = ens160_.getFlags();
  out.ens160Ready = (ens160Flags_ == 0);

  if (!ens160_.checkDataStatus()) {
    return false;
  }

  out.aqiUba = static_cast<uint8_t>(ens160_.getAQI());
  out.tvocPpb = static_cast<uint16_t>(ens160_.getTVOC());
  out.eco2Ppm = static_cast<uint16_t>(ens160_.getECO2());
  return true;
}

void AirQualitySensor::pushFilter_(float tempC, float humPct, uint16_t eco2, uint16_t tvoc,
                                   uint8_t aqi) {
  tempBuf_[filterIdx_] = tempC;
  humBuf_[filterIdx_] = humPct;
  eco2Buf_[filterIdx_] = static_cast<float>(eco2);
  tvocBuf_[filterIdx_] = static_cast<float>(tvoc);
  filterIdx_ = (filterIdx_ + 1) % SENSOR_FILTER_SAMPLES;
  if (filterCount_ < SENSOR_FILTER_SAMPLES) {
    filterCount_++;
  }
}

void AirQualitySensor::applyFilter_(AirQualityReading &out) const {
  if (filterCount_ == 0) {
    return;
  }

  float sumTemp = 0.0f;
  float sumHum = 0.0f;
  float sumEco2 = 0.0f;
  float sumTvoc = 0.0f;
  for (uint8_t i = 0; i < filterCount_; ++i) {
    sumTemp += tempBuf_[i];
    sumHum += humBuf_[i];
    sumEco2 += eco2Buf_[i];
    sumTvoc += tvocBuf_[i];
  }
  const float n = static_cast<float>(filterCount_);
  out.temperatureC = sumTemp / n;
  out.humidityPct = sumHum / n;
  out.eco2Ppm = static_cast<uint16_t>(sumEco2 / n + 0.5f);
  out.tvocPpb = static_cast<uint16_t>(sumTvoc / n + 0.5f);
}

bool AirQualitySensor::read(AirQualityReading &out) {
  if (!initialized_) {
    return false;
  }

  float tempC = NAN;
  float humPct = NAN;
  if (!readAht20_(tempC, humPct)) {
    faultActive_ = true;
    return false;
  }

  AirQualityReading raw{};
  raw.temperatureC = tempC;
  raw.humidityPct = humPct;
  const bool ensOk = readEns160_(raw, tempC, humPct);
  if (!ensOk) {
    raw.eco2Ppm = 0;
    raw.tvocPpb = 0;
    raw.aqiUba = 0;
    raw.ens160Ready = false;
  }

  pushFilter_(raw.temperatureC, raw.humidityPct, raw.eco2Ppm, raw.tvocPpb, raw.aqiUba);
  out = raw;
  applyFilter_(out);
  out.ens160Ready = raw.ens160Ready;
  out.aqiUba = raw.aqiUba;

  if (isValidReading(out)) {
    faultActive_ = false;
    return true;
  }

  faultActive_ = true;
  return false;
}

bool AirQualitySensor::isValidReading(const AirQualityReading &reading) const {
  if (isnan(reading.temperatureC) || isnan(reading.humidityPct)) {
    return false;
  }
  if (reading.temperatureC < MIN_VALID_TEMP_C || reading.temperatureC > MAX_VALID_TEMP_C) {
    return false;
  }
  if (reading.humidityPct < MIN_VALID_HUM_PCT || reading.humidityPct > MAX_VALID_HUM_PCT) {
    return false;
  }
  if (!reading.ens160Ready) {
    return true;
  }
  if (reading.eco2Ppm < MIN_VALID_ECO2_PPM || reading.eco2Ppm > MAX_VALID_ECO2_PPM) {
    return false;
  }
  if (reading.tvocPpb < MIN_VALID_TVOC_PPB || reading.tvocPpb > MAX_VALID_TVOC_PPB) {
    return false;
  }
  if (reading.aqiUba < MIN_VALID_AQI || reading.aqiUba > MAX_VALID_AQI) {
    return false;
  }
  return true;
}

const char *AirQualitySensor::stateText() const {
  if (!initialized_) {
    return "INIT";
  }
  if (faultActive_) {
    return "FAULT";
  }
  switch (ens160Flags_) {
    case 0:
      return "OK";
    case 1:
      return "WARMUP";
    case 2:
      return "STARTUP";
    default:
      return "NOTREADY";
  }
}

bool AirQualitySensor::recover() {
  Serial.println(F("WARN: attempting sensor recover..."));
  initialized_ = false;
  filterIdx_ = 0;
  filterCount_ = 0;
  const bool ok = initDevices_();
  initialized_ = ok;
  faultActive_ = !ok;
  return ok;
}
