#pragma once

#include <cstdint>
#include <cassert>

#include "libDaisy/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_ll_gpio.h"

namespace recorder
{

class GPIOPin
{
public:
    enum Pull
    {
        PULL_NONE = LL_GPIO_PULL_NO,
        PULL_UP   = LL_GPIO_PULL_UP,
        PULL_DOWN = LL_GPIO_PULL_DOWN,
    };

    enum Speed
    {
        SPEED_LOW    = LL_GPIO_SPEED_FREQ_LOW,
        SPEED_MEDIUM = LL_GPIO_SPEED_FREQ_MEDIUM,
        SPEED_HIGH   = LL_GPIO_SPEED_FREQ_HIGH,
    };

    enum Type
    {
        TYPE_PUSHPULL  = LL_GPIO_OUTPUT_PUSHPULL,
        TYPE_OPENDRAIN = LL_GPIO_OUTPUT_OPENDRAIN,
    };

protected:
    static void ClockEnable(uint32_t gpio_base)
    {
        switch (gpio_base)
        {
            case GPIOA_BASE: __HAL_RCC_GPIOA_CLK_ENABLE(); return;
            case GPIOB_BASE: __HAL_RCC_GPIOB_CLK_ENABLE(); return;
            case GPIOC_BASE: __HAL_RCC_GPIOC_CLK_ENABLE(); return;
            case GPIOD_BASE: __HAL_RCC_GPIOD_CLK_ENABLE(); return;
            case GPIOE_BASE: __HAL_RCC_GPIOE_CLK_ENABLE(); return;
            case GPIOF_BASE: __HAL_RCC_GPIOF_CLK_ENABLE(); return;
            case GPIOG_BASE: __HAL_RCC_GPIOG_CLK_ENABLE(); return;
            case GPIOH_BASE: __HAL_RCC_GPIOH_CLK_ENABLE(); return;
            case GPIOI_BASE: __HAL_RCC_GPIOI_CLK_ENABLE(); return;
            case GPIOJ_BASE: __HAL_RCC_GPIOJ_CLK_ENABLE(); return;
            case GPIOK_BASE: __HAL_RCC_GPIOK_CLK_ENABLE(); return;
        }
    }

    static GPIO_TypeDef* gpio_struct(uint32_t gpio_base)
    {
        return reinterpret_cast<GPIO_TypeDef*>(gpio_base);
    }
};

template <uint32_t gpio_base, uint32_t pin_number, bool invert = false>
class OutputPin : public GPIOPin
{
public:
    static void Init(Speed speed = SPEED_LOW,
              Type  type  = TYPE_PUSHPULL,
              Pull  pull  = PULL_NONE)
    {
        assert(IS_GPIO_ALL_INSTANCE((GPIO_TypeDef*)gpio_base));
        ClockEnable(gpio_base);

        LL_GPIO_SetPinSpeed     (gpio_struct(gpio_base), kPinMask, speed);
        LL_GPIO_SetPinPull      (gpio_struct(gpio_base), kPinMask, pull);
        LL_GPIO_SetPinOutputType(gpio_struct(gpio_base), kPinMask, type);
        LL_GPIO_SetPinMode      (gpio_struct(gpio_base), kPinMask,
            LL_GPIO_MODE_OUTPUT);
    }

    static void Set(void)
    {
        invert ? SetLow() : SetHigh();
    }

    static void Clear(void)
    {
        invert ? SetHigh() : SetLow();
    }

    static void Toggle(void)
    {
        uint32_t bit = gpio_struct(gpio_base)->ODR & kPinMask;
        gpio_struct(gpio_base)->BSRR = (bit << 16) | (bit ^ kPinMask);
    }

    static void Write(bool state)
    {
        state ? Set() : Clear();
    }

protected:
    static constexpr uint32_t kPinMask = 1 << pin_number;

    static void SetHigh(void)
    {
        gpio_struct(gpio_base)->BSRR = kPinMask;
    }

    static void SetLow(void)
    {
        gpio_struct(gpio_base)->BSRR = kPinMask << 16;
    }
};

template <uint32_t gpio_base, uint32_t pin_number, bool invert = false>
class InputPin : public GPIOPin
{
public:
    static void Init(Pull pull = PULL_NONE)
    {
        assert(IS_GPIO_ALL_INSTANCE((GPIO_TypeDef*)gpio_base));
        ClockEnable(gpio_base);

        LL_GPIO_SetPinPull(gpio_struct(gpio_base), kPinMask, pull);
        LL_GPIO_SetPinMode(gpio_struct(gpio_base), kPinMask,
            LL_GPIO_MODE_INPUT);
    }

    static uint32_t Read(void)
    {
        uint32_t pin = gpio_struct(gpio_base)->IDR & kPinMask;

        if (invert)
        {
            pin ^= kPinMask;
        }

        return pin >> pin_number;
    }

protected:
    static constexpr uint32_t kPinMask = 1 << pin_number;
};

class GenericInputPin : public GPIOPin
{
public:
    void Init(uint32_t gpio_base, uint32_t pin_number, bool invert = false,
        Pull pull = PULL_NONE)
    {
        assert(IS_GPIO_ALL_INSTANCE((GPIO_TypeDef*)gpio_base));
        ClockEnable(gpio_base);

        LL_GPIO_SetPinPull(gpio_struct(gpio_base), 1 << pin_number, pull);
        LL_GPIO_SetPinMode(gpio_struct(gpio_base), 1 << pin_number,
            LL_GPIO_MODE_INPUT);

        gpio_base_ = gpio_base;
        pin_number_ = pin_number;
        invert_ = invert;
    }

    uint32_t Read(void)
    {
        uint32_t pin = (gpio_struct(gpio_base_)->IDR >> pin_number_) & 1;

        if (invert_)
        {
            pin ^= 1;
        }

        return pin;
    }

protected:
    uint32_t gpio_base_;
    uint32_t pin_number_;
    bool invert_;
};

}
