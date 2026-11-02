#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_VEML6040)

#include "VEML6040Sensor.h"
#include <Wire.h>

static constexpr uint8_t VEML6040_ADDR = 0x10;
static constexpr uint8_t REG_CONF = 0x00;
static constexpr uint8_t REG_GREEN = 0x09;
static constexpr uint8_t REG_WHITE = 0x0B;
// Datasheet sensitivity of the green channel at 40 ms; it halves with every doubling of the integration time
static constexpr float LUX_PER_COUNT_40MS = 0.25168f;

// The sensor has no sensor-type of its own in the protobufs, so it is reported as the closest ambient-light sensor
VEML6040Sensor::VEML6040Sensor() : TelemetrySensor(meshtastic_TelemetrySensorType_VEML7700, "VEML6040") {}

bool VEML6040Sensor::writeConfig()
{
    // bit0 SD (0 = on), bit1 AF (0 = auto), bit2 TRIG, bits 6:4 integration time
    uint16_t conf = (uint16_t)(integrationTime & 0x07) << 4;
    _bus->beginTransmission(VEML6040_ADDR);
    _bus->write(REG_CONF);
    _bus->write((uint8_t)(conf & 0xFF));
    _bus->write((uint8_t)(conf >> 8));
    return _bus->endTransmission() == 0;
}

bool VEML6040Sensor::readChannel(uint8_t reg, uint16_t &value)
{
    _bus->beginTransmission(VEML6040_ADDR);
    _bus->write(reg);
    if (_bus->endTransmission(false) != 0)
        return false;
    if (_bus->requestFrom((uint8_t)VEML6040_ADDR, (uint8_t)2) != 2)
        return false;
    uint8_t lo = _bus->read();
    uint8_t hi = _bus->read();
    value = ((uint16_t)hi << 8) | lo;
    return true;
}

bool VEML6040Sensor::initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)
{
    LOG_INFO("Init sensor: %s", sensorName);
    _bus = bus;
    _address = dev->address.address;
    _port = dev->address.port;
    status = writeConfig();
    initI2CSensor();
    return status;
}

bool VEML6040Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
    uint16_t green = 0, white = 0;
    if (!readChannel(REG_GREEN, green) || !readChannel(REG_WHITE, white))
        return false;

    // Simple auto-range: shorter integration when close to saturation, longer when the readings are tiny
    if ((green > 60000 || white > 60000) && integrationTime > 0) {
        integrationTime--;
        writeConfig();
    } else if (green < 500 && white < 1000 && integrationTime < 5) {
        integrationTime++;
        writeConfig();
    }

    float lux = green * LUX_PER_COUNT_40MS / (float)(1 << integrationTime);
    float whiteLux = white * LUX_PER_COUNT_40MS / (float)(1 << integrationTime);
    measurement->variant.environment_metrics.has_lux = true;
    measurement->variant.environment_metrics.lux = lux;
    measurement->variant.environment_metrics.has_white_lux = true;
    measurement->variant.environment_metrics.white_lux = whiteLux;
    LOG_INFO("VEML6040 green %u white %u (IT %u) -> lux %f, white lux %f", green, white, integrationTime, lux, whiteLux);
    return true;
}

#endif
