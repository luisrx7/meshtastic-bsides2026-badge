#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_AGS10)

#include "AGS10Sensor.h"
#include "concurrency/OSThread.h"
#include <Wire.h>

static constexpr uint8_t AGS10_ADDR = 0x1A;
static constexpr uint8_t REG_TVOC = 0x00;
static constexpr uint32_t AGS10_CLOCK_HZ = 15000;
static constexpr uint32_t DEFAULT_CLOCK_HZ = 100000;

// Last IAQ for the badge LEDs (variant.cpp); -1 until the first valid reading
volatile int16_t badgeIaq = -1;

// The sensor answers "pre-heating" when it is read again within about two seconds, so keep the last good value
static constexpr uint32_t MIN_READ_INTERVAL_MS = 2500;
static constexpr uint32_t CACHE_VALID_MS = 5 * 60 * 1000;
static uint32_t lastReadMs = 0;
static uint32_t lastGoodMs = 0;
static uint32_t lastTvoc = 0;
static bool haveGood = false;

// There is no AGS10 in the protobuf sensor list; SFA30 (a formaldehyde sensor) is the unused air-quality entry
AGS10Sensor::AGS10Sensor() : TelemetrySensor(meshtastic_TelemetrySensorType_SFA30, "AGS10") {}

uint8_t AGS10Sensor::crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

// Piecewise-linear map of TVOC bands (ppb) onto the 0-500 IAQ scale
uint16_t AGS10Sensor::iaqFromTvoc(uint32_t tvocPpb)
{
    static const uint32_t ppb[] = {0, 65, 220, 660, 2200, 5500, 20000};
    static const uint16_t iaq[] = {0, 50, 100, 150, 200, 300, 500};
    if (tvocPpb >= ppb[6])
        return iaq[6];
    for (uint8_t i = 1; i < 7; i++) {
        if (tvocPpb <= ppb[i])
            return iaq[i - 1] + (uint32_t)(iaq[i] - iaq[i - 1]) * (tvocPpb - ppb[i - 1]) / (ppb[i] - ppb[i - 1]);
    }
    return iaq[6];
}

bool AGS10Sensor::readTvoc(uint32_t &tvocPpb)
{
    // Frame: status, TVOC (24 bit big endian, ppb), CRC8 (poly 0x31, init 0xFF) over the first four bytes
    uint8_t buf[5];
    _bus->setClock(AGS10_CLOCK_HZ);
    _bus->beginTransmission(AGS10_ADDR);
    _bus->write(REG_TVOC);
    bool ok = _bus->endTransmission() == 0;
    delay(30);
    ok = ok && _bus->requestFrom((uint8_t)AGS10_ADDR, (uint8_t)5) == 5;
    if (ok)
        for (uint8_t i = 0; i < 5; i++)
            buf[i] = _bus->read();
    _bus->setClock(DEFAULT_CLOCK_HZ);
    if (!ok)
        return false;
    if (crc8(buf, 4) != buf[4]) {
        LOG_WARN("AGS10 CRC error");
        return false;
    }
    if (buf[0] & 0x01) { // bit 0 set: sensor is still pre-heating
        LOG_DEBUG("AGS10 pre-heating");
        return false;
    }
    tvocPpb = ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
    return true;
}

static void startPoller(AGS10Sensor *sensor); // defined after AGS10Poller

bool AGS10Sensor::initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)
{
    LOG_INFO("Init sensor: %s", sensorName);
    _bus = bus;
    _address = dev->address.address;
    _port = dev->address.port;
    // The first frames are rejected while the sensor pre-heats, so only the presence of the device is checked here
    _bus->beginTransmission(AGS10_ADDR);
    status = _bus->endTransmission() == 0;
    initI2CSensor();
    if (status)
        startPoller(this);
    return status;
}

void AGS10Sensor::poll()
{
    uint32_t now = millis();
    if (haveGood && now - lastReadMs < MIN_READ_INTERVAL_MS)
        return;
    lastReadMs = now;
    uint32_t tvoc = 0;
    if (readTvoc(tvoc)) {
        lastTvoc = tvoc;
        lastGoodMs = now;
        haveGood = true;
        badgeIaq = iaqFromTvoc(tvoc);
    }
}

// Reads the sensor every 10 s so the badge LED follows the air quality; runs in the main scheduler, so the I2C bus is not shared
class AGS10Poller : public concurrency::OSThread
{
  public:
    explicit AGS10Poller(AGS10Sensor *s) : OSThread("AGS10Poller"), sensor(s) {}

  protected:
    int32_t runOnce() override
    {
        sensor->poll();
        return POLL_INTERVAL_MS;
    }

  private:
    static constexpr int32_t POLL_INTERVAL_MS = 10000;
    AGS10Sensor *sensor;
};

static void startPoller(AGS10Sensor *sensor)
{
    new AGS10Poller(sensor);
}

bool AGS10Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
    poll();
    uint32_t now = millis();
    if (!haveGood || now - lastGoodMs > CACHE_VALID_MS)
        return false;
    uint16_t iaq = iaqFromTvoc(lastTvoc);
    badgeIaq = iaq;
    measurement->variant.environment_metrics.has_iaq = true;
    measurement->variant.environment_metrics.iaq = iaq;
    LOG_INFO("AGS10 TVOC %u ppb -> IAQ %u", (unsigned)lastTvoc, (unsigned)iaq);
    return true;
}

#endif
