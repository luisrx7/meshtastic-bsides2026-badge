#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && __has_include(<Adafruit_SPA06_003.h>)

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "SPA06Sensor.h"
#include "TelemetrySensor.h"
#include <Adafruit_SPA06_003.h>

static TwoWire *dbgBus = nullptr;
static uint8_t dbgAddr = 0;

SPA06Sensor::SPA06Sensor()
    : TelemetrySensor(meshtastic_TelemetrySensorType_SPA06, "SPA06"), spa_temp(nullptr), spa_pressure(nullptr)
{
}

bool SPA06Sensor::initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)
{
    LOG_INFO("Init sensor: %s", sensorName);
    status = spa06.begin(dev->address.address, bus);
    if (!status) {
        return status;
    }
    // Set moderate precision for faster sampling
    spa06.setPressureOversampling(SPA06_003_OVERSAMPLE_8);    // 8x oversampling
    spa06.setTemperatureOversampling(SPA06_003_OVERSAMPLE_8); // 8x oversampling

    // Set measurement rate. 1 Hz is ample for telemetry (polled every tens of seconds)
    // and draws far less power than 32 Hz, while staying in continuous mode so getMetrics()
    // remains non-blocking (fresh data always available).
    spa06.setPressureMeasureRate(SPA06_003_RATE_1);    // 1 Hz
    spa06.setTemperatureMeasureRate(SPA06_003_RATE_1); // 1 Hz
    spa06.setMeasurementMode(SPA06_003_MEAS_CONTINUOUS_BOTH);

    // The temperature coefficients (c0, c1) belong to the sensor named in COEF_SRCE (0x28, bit 7). TMP_CFG (0x07, bit 7, TMP_EXT)
    // must select the same sensor, otherwise the other one is compensated with the wrong coefficients and reads tens of degrees off.
    // The Adafruit library does not do this.
    {
        auto readReg = [&](uint8_t reg, uint8_t &val) {
            bus->beginTransmission(dev->address.address);
            bus->write(reg);
            if (bus->endTransmission(false) != 0 || bus->requestFrom((int)dev->address.address, 1) != 1)
                return false;
            val = bus->read();
            return true;
        };
        uint8_t coefSrce = 0, tmpCfg = 0;
        if (readReg(0x28, coefSrce) && readReg(0x07, tmpCfg)) {
            uint8_t wanted = (tmpCfg & 0x7F) | (coefSrce & 0x80);
            LOG_INFO("SPA06 COEF_SRCE=0x%02x TMP_CFG=0x%02x -> 0x%02x", coefSrce, tmpCfg, wanted);
            if (wanted != tmpCfg) {
                bus->beginTransmission(dev->address.address);
                bus->write((uint8_t)0x07);
                bus->write(wanted);
                bus->endTransmission();
            }
        }
    }

    dbgBus = bus;
    dbgAddr = dev->address.address;
    spa_temp = spa06.getTemperatureSensor();
    spa_pressure = spa06.getPressureSensor();

    initI2CSensor();
    return status;
}

// Dumps the registers the temperature depends on (bring-up diagnostics)
static void dumpSpa06Regs()
{
    uint8_t r[0x2A] = {0};
    if (!dbgBus)
        return;
    dbgBus->beginTransmission(dbgAddr);
    dbgBus->write((uint8_t)0x00);
    if (dbgBus->endTransmission(false) != 0 || dbgBus->requestFrom((int)dbgAddr, (int)sizeof(r)) != (int)sizeof(r))
        return;
    for (size_t i = 0; i < sizeof(r); i++)
        r[i] = dbgBus->read();
    LOG_INFO("SPA06 TMP=%02x%02x%02x PRS_CFG=%02x TMP_CFG=%02x MEAS=%02x CFG=%02x COEF %02x %02x %02x COEF_SRCE=%02x", r[3], r[4],
             r[5], r[6], r[7], r[8], r[9], r[0x10], r[0x11], r[0x12], r[0x28]);
}

bool SPA06Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
    if (!spa_temp || !spa_pressure) {
        return false;
    }
    dumpSpa06Regs();

    sensors_event_t temp, press;

    if (!spa_temp->getEvent(&temp) || !spa_pressure->getEvent(&press)) {
        LOG_DEBUG("SPA06 getEvents no data");
        return false;
    }

    measurement->variant.environment_metrics.has_temperature = true;
    measurement->variant.environment_metrics.has_barometric_pressure = true;
#ifdef SPA06_TEMP_OFFSET_C
    measurement->variant.environment_metrics.temperature = temp.temperature + SPA06_TEMP_OFFSET_C;
#else
    measurement->variant.environment_metrics.temperature = temp.temperature;
#endif
    LOG_INFO("SPA06 compensated %.2f C", (double)temp.temperature);
    measurement->variant.environment_metrics.barometric_pressure = press.pressure;

    return true;
}
#endif
