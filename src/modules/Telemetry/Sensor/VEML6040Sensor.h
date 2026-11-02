#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_VEML6040)

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "TelemetrySensor.h"

// Vishay VEML6040 RGBW colour sensor (I2C 0x10). Reported as ambient light: green channel -> lux, white channel -> white_lux.
class VEML6040Sensor : public TelemetrySensor
{
  private:
    uint8_t integrationTime = 2; // 0..5 = 40, 80, 160, 320, 640, 1280 ms

    bool writeConfig();
    bool readChannel(uint8_t reg, uint16_t &value);

  public:
    VEML6040Sensor();
    virtual bool getMetrics(meshtastic_Telemetry *measurement) override;
    virtual bool initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev) override;
};

#endif
