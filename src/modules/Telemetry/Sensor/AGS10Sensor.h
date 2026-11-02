#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_AGS10)

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "TelemetrySensor.h"

// Aosong AGS10 TVOC gas sensor (I2C 0x1A, must be read at <= 15 kHz).
// The telemetry has no TVOC field, so the TVOC reading (ppb) is mapped onto the 0-500 `iaq` scale. This is an approximation
// based on TVOC bands, not a Bosch BSEC index.
class AGS10Sensor : public TelemetrySensor
{
  private:
    static uint8_t crc8(const uint8_t *data, uint8_t len);
    static uint16_t iaqFromTvoc(uint32_t tvocPpb);
    bool readTvoc(uint32_t &tvocPpb);

  public:
    AGS10Sensor();
    void poll(); // reads the sensor if the minimum interval has passed and updates the cached value and badgeIaq
    virtual bool getMetrics(meshtastic_Telemetry *measurement) override;
    virtual bool initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev) override;
};

#endif
