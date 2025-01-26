#include "drivers/irq.h"

#include <cassert>

namespace recorder::irq
{

constexpr uint32_t kNumVectors = 0xa6;
constexpr uint32_t kAlignment = 1024;

static Vector RAMVectorTable[kNumVectors]
    __attribute__ ((aligned(kAlignment)));

void Init(void)
{
    auto src = reinterpret_cast<const irq::Vector*>(SCB->VTOR);

    for (uint32_t i = 0; i < kNumVectors; i++)
    {
        RAMVectorTable[i] = src[i];
    }

    SCB->VTOR = reinterpret_cast<uint32_t>(RAMVectorTable);
}

void RegisterHandler(IRQn_Type irqn, Vector handler)
{
    assert(irqn >= NonMaskableInt_IRQn);

    uint32_t exception_num = irqn + 16;
    assert(exception_num < kNumVectors);

    RAMVectorTable[exception_num] = handler;
}

void Enable(IRQn_Type irqn)
{
    assert(irqn >= 0);
    NVIC_EnableIRQ(irqn);
}

void Disable(IRQn_Type irqn)
{
    assert(irqn >= 0);
    NVIC_DisableIRQ(irqn);
}

void SetPriority(IRQn_Type irqn, uint32_t priority)
{
    uint32_t group = NVIC_GetPriorityGrouping();
    priority = NVIC_EncodePriority(group, priority, 0);
    NVIC_SetPriority(irqn, priority);
}

}
