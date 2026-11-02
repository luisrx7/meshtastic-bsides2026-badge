/*
    Input source for boards with four capacitive touch pads read with touchRead().
    Pads are polled; a short touch sends UP / CANCEL / DOWN / SELECT, a long touch on pads 1 and 3 sends LEFT / RIGHT.
*/

#pragma once

#include "configuration.h"

#ifdef INPUTBROKER_TOUCHPADS

#include "InputBroker.h"
#include "concurrency/OSThread.h"

class TouchPads : public Observable<const InputEvent *>, public concurrency::OSThread
{
  public:
    TouchPads();

  protected:
    int32_t runOnce() override;

  private:
    static constexpr uint8_t N_PADS = 4;
    static constexpr uint32_t DEBOUNCE_MS = 40;
    static constexpr uint32_t LONG_PRESS_MS = 600;
#ifdef TOUCHPAD_THRESHOLD
    static constexpr uint32_t TOUCH_THRESHOLD = TOUCHPAD_THRESHOLD;
#else
    static constexpr uint32_t TOUCH_THRESHOLD = 100000; // raw touchRead() above this counts as touched
#endif

    const uint8_t pins[N_PADS] = {TOUCHPAD_PINS};
    uint32_t downSince[N_PADS] = {};
    bool down[N_PADS] = {};
    bool longSent[N_PADS] = {};

    void sendKey(input_broker_event key);
};

extern TouchPads *touchPadsInput;

#endif
