#pragma once

#include <cstdint>
#include "libDaisy/Drivers/CMSIS/Device/ST/STM32H7xx/Include/stm32h750xx.h"

namespace recorder::irq
{

using Vector = void (*)(void);

void Init(void);
void RegisterHandler(IRQn_Type irqn, Vector handler);

void Enable(IRQn_Type irqn);
void Disable(IRQn_Type irqn);
void SetPriority(IRQn_Type irqn, uint32_t priority);

}
