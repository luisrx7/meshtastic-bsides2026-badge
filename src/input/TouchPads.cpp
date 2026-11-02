#include "TouchPads.h"
#include "Throttle.h"

#ifdef INPUTBROKER_TOUCHPADS

static const char inputSourceName[] = "TouchPads";

TouchPads *touchPadsInput;

TouchPads::TouchPads() : concurrency::OSThread(inputSourceName)
{
    inputBroker->registerSource(this);
}

int32_t TouchPads::runOnce()
{
    for (uint8_t i = 0; i < N_PADS; i++) {
        bool touched = touchRead(pins[i]) > TOUCH_THRESHOLD;

        if (touched && !down[i]) {
            down[i] = true;
            longSent[i] = false;
            downSince[i] = millis();
        } else if (touched && down[i]) {
            // Long touch on pads 1 and 3 moves between screens
            if (!longSent[i] && (i == 0 || i == 2) && !Throttle::isWithinTimespanMs(downSince[i], LONG_PRESS_MS)) {
                longSent[i] = true;
                sendKey(i == 0 ? INPUT_BROKER_LEFT : INPUT_BROKER_RIGHT);
            }
        } else if (!touched && down[i]) {
            down[i] = false;
            if (!longSent[i] && !Throttle::isWithinTimespanMs(downSince[i], DEBOUNCE_MS)) {
                static const input_broker_event shortKeys[N_PADS] = {INPUT_BROKER_UP, INPUT_BROKER_CANCEL, INPUT_BROKER_DOWN,
                                                                     INPUT_BROKER_SELECT};
                sendKey(shortKeys[i]);
            }
        }
    }
    return 50;
}

void TouchPads::sendKey(input_broker_event key)
{
    InputEvent e = {};
    e.source = inputSourceName;
    e.inputEvent = key;
    notifyObservers(&e);
}

#endif
