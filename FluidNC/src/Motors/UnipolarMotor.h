// Copyright (c) 2020 -	Bart Dring
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

// Backported from upstream FluidNC (bdring/FluidNC#1797, "Add ULN2003 support",
// merged 2026-09-05, first released in v4.1.0) to this v3.9.5-based fork so that
// the Dune Weaver Mini's 28BYJ-48 + ULN2003 motors can be driven again.

#pragma once

#include "MotorDriver.h"
#include "../Pin.h"

namespace MotorDrivers {
    class UnipolarMotor final : public MotorDriver {
    public:
        UnipolarMotor(const char* name) : MotorDriver(name) {}

        // Overrides for inherited methods
        void init() override;
        bool set_homing_mode(bool isHoming) override { return true; }
        void set_disable(bool disable) override;

        // The ISR path calls these directly, not through the vtable.  They are
        // non-virtual on purpose: whether a virtual call gets devirtualised is
        // the optimiser's choice, and a vtable read from the step ISR is a
        // flash access that panics the board when the cache is disabled.
        void step_isr();
        void set_direction_isr(bool dir);

        IsrStepFn isr_step_fn() override;
        IsrDirFn  isr_dir_fn() override;

        // Configuration handlers:
        void validate() override;

        void group(Configuration::HandlerBase& handler) override {
            // Coil drive outputs, e.g. IN1..IN4 on a ULN2003 board.
            handler.item("phase0_pin", _pin_phase0);
            handler.item("phase1_pin", _pin_phase1);
            handler.item("phase2_pin", _pin_phase2);
            handler.item("phase3_pin", _pin_phase3);

            // True uses the 8-phase half-step sequence (twice the resolution,
            // less torque).  False selects the 4-phase full-step sequence, which
            // halves the steps per revolution, so steps_per_mm must match.
            handler.item("half_step", _half_step);
        }

    private:
        static constexpr pinnum_t kNoPin = 255;

        Pin     _pin_phase0;
        Pin     _pin_phase1;
        Pin     _pin_phase2;
        Pin     _pin_phase3;
        uint8_t _current_phase = 0;
        bool    _half_step     = true;

        // Native GPIO numbers for the four phase pins, resolved in init().
        // step_isr() writes them with gpio_write() rather than through Pin,
        // whose write() dispatches through a PinDetail vtable that lives in
        // flash - not safe to touch from the step ISR.  validate() has already
        // required these to be gpio pins.
        pinnum_t _gpio_phase[4]   = { kNoPin, kNoPin, kNoPin, kNoPin };
        // Each phase pin's ActiveLow setting, captured beside its number so the
        // ISR can apply the inversion that Pin::write() would have applied.
        bool _invert_phase[4] = { false, false, false, false };
        bool _enabled         = false;
        bool _dir             = true;

    protected:
        void config_message() override;
    };
}
