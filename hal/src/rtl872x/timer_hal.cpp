/*
 * Copyright (c) 2021 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

// This code is partially based on OpenThread nRF52840-based alarm implementation

/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdint>
#include "timer_hal.h"
#include "service_debug.h"
#include "system_error.h"
#include "check.h"
#include "hw_ticks.h"
extern "C" {
#include "rtl8721d.h"
#include "rtl8721d_delay.h"
}
#include "interrupts_hal.h"

namespace {

constexpr int TIMER_INDEX = 4;                              // Use TIM4
constexpr int TIMER_PRESCALAR = 39;                         // Input clock = 40MHz_XTAL / (39 + 1) = 1MHz
constexpr int TIMER_COUNTER_US_THRESHOLD = 1;               // 1us = 1tick
constexpr int US_PER_OVERFLOW = 65536;                      // 65536us
constexpr int TIMER_IRQ_PRIORITY = 0;                       // Default priority in the close-source timer driver per FAE
constexpr IRQn_Type TIMER_IRQ[] = {
    TIMER0_IRQ,
    TIMER1_IRQ,
    TIMER2_IRQ,
    TIMER3_IRQ,
    TIMER4_IRQ,
    TIMER5_IRQ
};

inline uint64_t calibrationTickToTimeUs(uint64_t tick) {
    return tick * 1000000 / 32768;
}

class RtlTimer {
public:
    RtlTimer() = default;
    ~RtlTimer() = default;

    int init(const hal_timer_init_config_t* conf) {
        if (enabled_) {
            deinit();
        }

        if (conf) {
            usSystemTimeMicrosBase_ = conf->base_clock_offset;
        } else {
            usSystemTimeMicrosBase_ = 0;
        }

        overflowCounter_ = 0;
        mutex_ = 0;

        // Enable timer clock
        RCC_PeriphClockCmd(APBPeriph_GTIMER, APBPeriph_GTIMER_CLOCK, ENABLE);

        // Configure NVIC
        NVIC_SetPriority(TIMER_IRQ[TIMER_INDEX], TIMER_IRQ_PRIORITY);
        NVIC_ClearPendingIRQ(TIMER_IRQ[TIMER_INDEX]);
        NVIC_EnableIRQ(TIMER_IRQ[TIMER_INDEX]);

        // Configure timer
        RTIM_TimeBaseInitTypeDef initStruct = {};
        RTIM_TimeBaseStructInit(&initStruct);
        initStruct.TIM_Idx = TIMER_INDEX;
        initStruct.TIM_Prescaler = TIMER_PRESCALAR;
        initStruct.TIM_UpdateEvent = ENABLE;
        initStruct.TIM_UpdateSource = TIM_UpdateSource_Overflow;
        initStruct.TIM_ARRProtection = ENABLE;
        RTIM_TimeBaseInit(TIMx[TIMER_INDEX], &initStruct, TIMx_irq[TIMER_INDEX], interruptHandler, (uint32_t)this);
        enableOverflowInterrupt();
        enabled_ = true;

        return SYSTEM_ERROR_NONE;
    }

    int deinit() {
        CHECK_TRUE(enabled_, SYSTEM_ERROR_INVALID_STATE);
        disableOverflowInterrupt();
        RTIM_Cmd(TIMx[TIMER_INDEX], DISABLE);
        RTIM_DeInit(TIMx[TIMER_INDEX]);
        NVIC_DisableIRQ(TIMER_IRQ[TIMER_INDEX]);
        NVIC_ClearPendingIRQ(TIMER_IRQ[TIMER_INDEX]);
        enabled_ = false;
        return SYSTEM_ERROR_NONE;
    }

    int start() {
        CHECK_TRUE(enabled_, SYSTEM_ERROR_INVALID_STATE);
        lastOverflowSysTimer_ = 0;
        lastOverflowTimer_ = 0;
        overflowCounter_ = 0;
        int s = HAL_disable_irq();
        stop();
        auto t0 = SYSTIMER_TickGet();
        resetCounter();
        __DMB();
        // Wait for timer to actually get reset
        while (SYSTIMER_TickGet() >= t0) {
            ;
        }
        RTIM_Cmd(TIMx[TIMER_INDEX], ENABLE);
        RTIM_Cmd(TIMx[TIMER_INDEX], ENABLE);
        HAL_enable_irq(s);
        return SYSTEM_ERROR_NONE;
    }

    int stop() {
        CHECK_TRUE(enabled_, SYSTEM_ERROR_INVALID_STATE);
        RTIM_Cmd(TIMx[TIMER_INDEX], DISABLE);
        return SYSTEM_ERROR_NONE;
    }

    int setTime(uint64_t timeUs) {
        CHECK_TRUE(enabled_, SYSTEM_ERROR_INVALID_STATE);
        int s = HAL_disable_irq();
        usSystemTimeMicrosBase_ = timeUs;
        start();
        HAL_enable_irq(s);
    }

    uint64_t getTime() {
        uint32_t offset1 = getOverflowCounter();
        __DMB();

        uint32_t counterValue = getCounter();
        __DMB();

        bool uncertain = false;
        uint32_t offset2 = getOverflowCounter(&uncertain);

        if (offset1 != offset2) {
            // Overflow occured between the calls
            counterValue = getCounter();
        }

        if (uncertain) {
            int s = HAL_disable_irq();
            uint64_t curSysTimerUs = sysTimerUs();
            if (curSysTimerUs < lastOverflowSysTimer_) {
                // Wraparound
                curSysTimerUs += 0xffffffff;
            }
            uint64_t t = usSystemTimeMicrosBase_ + lastOverflowTimer_ + (curSysTimerUs - lastOverflowSysTimer_);
            HAL_enable_irq(s);
            return t;
        }

        return usSystemTimeMicrosBase_ + offset2 * US_PER_OVERFLOW + ticksToTime(counterValue);
    }

    void interruptHandlerImpl() {
        if (getOverflowPendingEvent()) {
            // Disable OVERFLOW interrupt to prevent lock-up in interrupt context while mutex is locked from lower priority
            // context and OVERFLOW event flag is stil up. OVERFLOW interrupt will be re-enabled when mutex is released -
            // either from this handler, or from lower priority context, that locked the mutex.
            // Note: RTIM_INTConfig(DISABLE) will clear TIM_IT_Update interrupt status, use disableOverflowInterrupt() instead
            disableOverflowInterrupt();

            // Handle OVERFLOW event by reading current value of overflow counter.
            (void)getOverflowCounter();

        } else {
            RTIM_INTClear(TIMx[TIMER_INDEX]);
        }
    }

    static uint32_t interruptHandler(void* context) {
        RtlTimer* timerInstance = (RtlTimer*)context;
        timerInstance->interruptHandlerImpl();
        return SYSTEM_ERROR_NONE;
    }

    inline uint64_t sysTimerUs() {
        return (((uint64_t)SYSTIMER_TickGet()) * TIMER_TICK_US_X4) / 4;
    }

private:
    inline void resetCounter() {
        RTIM_Reset(TIMx[TIMER_INDEX]);
        // Also reset SYSTIMER
        RTIM_Reset(TIMM00);
    }

    inline uint32_t getCounter() {
        return RTIM_GetCount(TIMx[TIMER_INDEX]);
    }

    inline void enableOverflowInterrupt() {
        TIMx[TIMER_INDEX]->DIER |= TIM_IT_Update;
    }

    inline void disableOverflowInterrupt() {
        TIMx[TIMER_INDEX]->DIER &= ~TIM_IT_Update;
    }

    inline bool getOverflowPendingEvent() {
        return TIMx[TIMER_INDEX]->SR & TIM_IT_Update;
    }

    inline void clearOverflowPendingEvent() {
        TIMx[TIMER_INDEX]->SR |= TIM_IT_Update;
        NVIC_ClearPendingIRQ(TIMER_IRQ[TIMER_INDEX]);
    }

    inline bool getMutex() {
        do {
            volatile uint8_t mutexValue = __LDREXB(&mutex_);

            if (mutexValue) {
                __CLREX();
                return false;
            }
        } while (__STREXB(1, &mutex_));

        // Disable OVERFLOW interrupt to prevent lock-up in interrupt context while mutex is locked from lower priority
        // context and OVERFLOW event flag is stil up.
        disableOverflowInterrupt();

        __DMB();

        return true;
    }

    inline void releaseMutex() {
        // Re-enable OVERFLOW interrupt.
        enableOverflowInterrupt();

        __DMB();
        mutex_ = 0;
    }

    constexpr uint64_t ticksToTime(uint64_t ticks) {
        return ticks / TIMER_COUNTER_US_THRESHOLD;
    }

    uint64_t rtcCounterAndTicksToUs(uint32_t offset, uint32_t counter) {
        return (uint64_t)offset * US_PER_OVERFLOW + ticksToTime(counter);
    }

    uint32_t getOverflowCounter(bool* uncertain = nullptr) {
        uint32_t overflowCounter;

        // Get mutual access for writing to overflowCounter_ variable.
        if (getMutex()) {
            bool increasing = false;

            // Check if interrupt was handled already.
            if (getOverflowPendingEvent()) {
                int s = HAL_disable_irq();
                uint64_t curSysTimerUs = sysTimerUs();
                if (curSysTimerUs < lastOverflowSysTimer_) {
                    // Wraparound
                    curSysTimerUs += 0xffffffff;
                }
                uint32_t curTimerTicks = getCounter();
                uint64_t curTimerUs = rtcCounterAndTicksToUs((overflowCounter_ + 2) / 2, curTimerTicks);
                uint64_t timeElapsedSysTimer = curSysTimerUs - lastOverflowSysTimer_;
                uint64_t timeElapsedTimer = curTimerUs - lastOverflowTimer_;
                if (timeElapsedSysTimer > timeElapsedTimer) {
                    uint32_t diff = timeElapsedSysTimer - timeElapsedTimer;
                    if (diff > US_PER_OVERFLOW) {
                        // Adjust overflow counter
                        overflowCounter_ += (diff / US_PER_OVERFLOW + 1) * 2;
                    }
                }
                overflowCounter_++;
                lastOverflowSysTimer_ = curSysTimerUs;
                lastOverflowTimer_ = rtcCounterAndTicksToUs((overflowCounter_ + 1) / 2, curTimerTicks);
                // Mark that interrupt was handled.
                clearOverflowPendingEvent();
                HAL_enable_irq(s);

                increasing = true;

                __DMB();

                // Result should be incremented. overflowCounter_ will be incremented after mutex is released.
            } else {
                // Either overflow handling is not needed OR we acquired the mutex just after it was released.
                // Overflow is handled after mutex is released, but it cannot be assured that overflowCounter_
                // was incremented for the second time, so we increment the result here.
            }

            overflowCounter = (overflowCounter_ + 1) / 2;

            releaseMutex();

            if (increasing) {
                // It's virtually impossible that overflow event is pending again before next instruction is performed. It
                // is an error condition.
                SPARK_ASSERT(overflowCounter_ & 0x01);

                // Increment the counter for the second time, to allow instructions from other context get correct value of
                // the counter.
                overflowCounter_++;
            }
        } else {
            // Failed to acquire mutex.
            if (getOverflowPendingEvent()) {
                // Lower priority context is currently incrementing overflowCounter_ variable.
                // We cannot be certain about the current overflow value, so we'll need to use systimer instead
                overflowCounter = 0;
                if (uncertain) {
                    *uncertain = true;
                }
            } else if (overflowCounter_ & 0x01) {
                overflowCounter = (overflowCounter_ + 2) / 2;
            } else {
                // Lower priority context has already incremented overflowCounter_ variable or incrementing is not needed
                // now.
                overflowCounter = overflowCounter_ / 2;
            }
        }

        return overflowCounter;
    }

private:
    bool enabled_ = false;
    volatile uint32_t overflowCounter_ = 0;
    volatile int64_t  usSystemTimeMicrosBase_ = 0;  ///< Base offset for Particle-specific microsecond counter
    volatile uint8_t mutex_ = 0;
    volatile uint64_t lastOverflowSysTimer_ = 0;
    volatile uint64_t lastOverflowTimer_ = 0;
};

RtlTimer* getInstance() {
    static RtlTimer timer;
    return &timer;
}

};  // Anonymouse

// Particle-specific
int hal_timer_init(const hal_timer_init_config_t* conf) {
    auto timerInstance = getInstance();
    CHECK(timerInstance->init(conf));
    return timerInstance->start();
}

int hal_timer_deinit(void* reserved) {
    auto timerInstance = getInstance();
    return timerInstance->deinit();
}

uint64_t hal_timer_micros(void* reserved) {
    auto timerInstance = getInstance();
    return timerInstance->getTime();
}

uint64_t hal_timer_millis(void* reserved) {
    return hal_timer_micros(nullptr) / 1000ULL;
}

system_tick_t HAL_Timer_Get_Micro_Seconds(void) {
    return (system_tick_t)hal_timer_micros(nullptr);
}

system_tick_t HAL_Timer_Get_Milli_Seconds(void) {
    return (system_tick_t)hal_timer_millis(nullptr);
}
