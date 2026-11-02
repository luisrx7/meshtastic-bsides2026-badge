#include "Arduino.h"
#include "configuration.h"
#include "variant.h"
#include "driver/i2s_pdm.h"
#include <math.h>

// Own driver for the two daisy-chained WS2812B: the Meshtastic ambient lighting did not give a usable brightness range.
// LED1 = air quality from the AGS10 (green good, yellow moderate, red poor, blue until the first reading), LED2 = noise level.

extern volatile int16_t badgeIaq; // set by AGS10Sensor.cpp, -1 until the first valid reading

// PDM microphone (CLK GPIO17, DATA GPIO18): sound level in dBFS, updated about 8 times a second
static volatile float noiseDb = -99;

// millis() of the last received text message (set in TextMessageModule.cpp), 0 = none yet
volatile uint32_t badgeMsgMs = 0;

static void micTask(void *)
{
    i2s_chan_handle_t h = nullptr;
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    if (i2s_new_channel(&cc, nullptr, &h) != ESP_OK) {
        LOG_WARN("BadgeMic: i2s_new_channel failed");
        vTaskDelete(NULL);
    }
    i2s_pdm_rx_config_t cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {.clk = (gpio_num_t)BADGE_MIC_CLK, .din = (gpio_num_t)BADGE_MIC_DATA, .invert_flags = {false}},
    };
    if (i2s_channel_init_pdm_rx_mode(h, &cfg) != ESP_OK || i2s_channel_enable(h) != ESP_OK) {
        LOG_WARN("BadgeMic: PDM init failed");
        i2s_del_channel(h);
        vTaskDelete(NULL);
    }
    LOG_INFO("BadgeMic: PDM running");
    static int16_t pcm[1024];
    uint32_t blocks = 0;
    for (;;) {
        size_t got = 0;
        if (i2s_channel_read(h, pcm, sizeof(pcm), &got, 500) != ESP_OK || got < 64) {
            LOG_WARN("BadgeMic: read failed");
            continue;
        }
        if (++blocks % 20 == 0)
            LOG_INFO("BadgeMic: %.1f dBFS", (double)noiseDb);
        int n = got / 2;
        double sum = 0, sum2 = 0;
        for (int i = 0; i < n; i++) {
            sum += pcm[i];
            sum2 += (double)pcm[i] * pcm[i];
        }
        double mean = sum / n, var = sum2 / n - mean * mean;
        noiseDb = var > 0 ? 20.0f * log10f(sqrtf((float)var) / 32768.0f) : -99;
    }
}

static void ledFrame(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2)
{
    rmt_data_t d[48];
    const uint8_t px[6] = {g1, r1, b1, g2, r2, b2}; // WS2812 colour order is GRB
    int n = 0;
    for (uint8_t i = 0; i < 6; i++)
        for (int8_t b = 7; b >= 0; b--) {
            bool one = (px[i] >> b) & 1;
            d[n].level0 = 1;
            d[n].duration0 = one ? 8 : 3; // 10 MHz tick: 0.8 / 0.3 us high (WS2812B-V6 wants T0H <= 0.38 us)
            d[n].level1 = 0;
            d[n].duration1 = one ? 4 : 9;
            n++;
        }
    rmtWrite(BADGE_LED_PIN, d, 48, RMT_WAIT_FOR_EVER);
}

static void ledTask(void *)
{
    const uint8_t L = BADGE_LED_LEVEL;
    rmtInit(BADGE_LED_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
    uint32_t tick = 0;
    uint64_t lastCode = ~0ULL;
    float noiseFloor = 0, hold = 0; // 0 = not known yet (dBFS is always negative)
    for (;;) {
        int16_t iaq = badgeIaq;
        uint8_t r1 = 0, g1 = 0, b1 = 0;
        if (iaq < 0) {
            b1 = L;
        } else if (iaq <= 50) {
            g1 = L;
        } else if (iaq <= 100) {
            r1 = L;
            g1 = L;
        } else if (iaq <= 150) {
            r1 = L;
        } else if ((tick / 5) & 1) { // poor air: red, blinking
            r1 = L;
        }
        // LED2 is the noise indicator, relative to the quiet level of the room (slow minimum tracker): off when quiet,
        // green / yellow / red for +10 / +20 / +30 dB above it. Sampled fast so a clap is seen, and held for about a second.
        float db = noiseDb;
        if (db > -98 && noiseFloor == 0)
            noiseFloor = db; // first valid sample
        else if (db > -98 && db < noiseFloor)
            noiseFloor = db;
        else if (noiseFloor != 0)
            noiseFloor += 0.01f; // creeps up so the floor follows a room that gets noisier
        float over = noiseFloor != 0 ? db - noiseFloor : 0;
        if (over > hold)
            hold = over;
        else
            hold -= 0.4f; // 100 ms steps: roughly 25 dB per second
        uint8_t r2 = 0, g2 = 0, b2 = 0;
        if (hold > 30) {
            r2 = L;
        } else if (hold > 20) {
            r2 = L;
            g2 = L;
        } else if (hold > 10) {
            g2 = L;
        }
        // A text message arrived: both LEDs blink cyan for 6 s, at twice the normal level so it stands out
        uint32_t msgAt = badgeMsgMs;
        if (msgAt != 0 && millis() - msgAt < 6000) {
            bool on = ((millis() - msgAt) / 300) % 2 == 0;
            r1 = r2 = 0;
            g1 = g2 = b1 = b2 = on ? L * 2 : 0;
        }
        // Only send when something changed (and every 2 s to heal a glitched frame), so the LEDs do not flicker
        uint64_t code = ((uint64_t)r1 << 40) | ((uint64_t)g1 << 32) | ((uint64_t)b1 << 24) | (r2 << 16) | (g2 << 8) | b2;
        if (code != lastCode || tick % 20 == 0) {
            ledFrame(r1, g1, b1, r2, g2, b2);
            lastCode = code;
        }
        tick++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void earlyInitVariant()
{
    xTaskCreate(ledTask, "badgeLeds", 3072, NULL, 1, NULL);
    xTaskCreate(micTask, "badgeMic", 4096, NULL, 1, NULL);
}
