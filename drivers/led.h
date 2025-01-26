#pragma once

#include <cstdint>

#include "drivers/gpio.h"
#include "drivers/system.h"
#include "common/config.h"
#include "common/io.h"
#include "util/debouncer.h"

namespace recorder
{

class Led
{
public:
  Led(OutputPin<GPIOC_BASE, 13>& pin) : pin_(pin) {
    pin_.Init();
  }

  void TurnOn() {
    pin_.Set();
  }

  void TurnOff() {
    pin_.Clear();
  }

  void Toggle() {
    pin_.Toggle();
  }

private:
  OutputPin<GPIOC_BASE, 13>& pin_;

};

}
