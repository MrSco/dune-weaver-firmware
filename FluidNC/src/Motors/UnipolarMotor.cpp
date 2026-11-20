// Copyright (c) 2020 -	Bart Dring
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

/*
    Unipolar stepper motors, such as the 28BYJ-48 driven by a ULN2003 board.

    These have no step/direction inputs; the four coil phases are driven
    directly, so this driver energizes the phase pins itself rather than
    handing a step pin to the stepping engine.  It registers with
    Stepping::assignMotorDriver(), which then calls the driver's ISR entry
    points from the step ISR in place of the engine's pin operations.

    Backported from upstream FluidNC (bdring/FluidNC#1797, first released in
    v4.1.0).  The coil sequence and direction convention are identical to the
    FluidNC <= 3.8.x driver, so existing Dune Weaver Mini configs keep their
    phase pin order and turn the same way.
*/

#include "UnipolarMotor.h"

#include "../Machine/MachineConfig.h"
#include "../Stepping.h"             // Stepping::assignMotorDriver()
#include "Driver/fluidnc_gpio.h"     // gpio_write()
#include "../string_util.h"          // starts_with_ignore_case()

#include <esp_attr.h>  // IRAM_ATTR

using namespace Machine;

namespace MotorDrivers {
    void UnipolarMotor::init() {
        _pin_phase0.setAttr(Pin::Attr::Output);
        _pin_phase1.setAttr(Pin::Attr::Output);
        _pin_phase2.setAttr(Pin::Attr::Output);
        _pin_phase3.setAttr(Pin::Attr::Output);
        // Resolve the native pin numbers and their ActiveLow settings once,
        // here, where touching flash is fine.  step_isr() runs from an IRAM
        // ISR and cannot go through Pin.
        int i = 0;
        for (auto pin : { &_pin_phase0, &_pin_phase1, &_pin_phase2, &_pin_phase3 }) {
            _gpio_phase[i]   = pinnum_t(pin->index());
            _invert_phase[i] = pin->inverted();
            ++i;
        }
        _current_phase = 0;
        config_message();

        Stepping::assignMotorDriver(int(axis_index()), int(dual_axis_index()), this);
    }

    void UnipolarMotor::validate() {
        Assert(!_pin_phase0.undefined(), "Phase 0 pin should be configured.");
        Assert(!_pin_phase1.undefined(), "Phase 1 pin should be configured.");
        Assert(!_pin_phase2.undefined(), "Phase 2 pin should be configured.");
        Assert(!_pin_phase3.undefined(), "Phase 3 pin should be configured.");

        // The phase pins are written directly from the step ISR, bypassing the
        // stepping engine, so they must be pins that can be set immediately.
        for (auto pin : { &_pin_phase0, &_pin_phase1, &_pin_phase2, &_pin_phase3 }) {
            Assert(string_util::starts_with_ignore_case(pin->name(), "gpio"), "Phase pin %s type must be gpio", pin->name().c_str());
        }
    }

    void UnipolarMotor::config_message() {
        log_info("    " << name() << " Ph0:" << _pin_phase0.name() << " Ph1:" << _pin_phase1.name() << " Ph2:" << _pin_phase2.name()
                        << " Ph3:" << _pin_phase3.name() << " Half Step:" << (_half_step ? "Yes" : "No"));
    }

    void IRAM_ATTR UnipolarMotor::set_disable(bool disable) {
        // set_disable() can be called before init() has resolved the pins.
        if (disable && _gpio_phase[0] != kNoPin) {
            // Logical off, which is a high level on an ActiveLow pin.  Uses
            // gpio_write() so this stays safe with the flash cache disabled.
            gpio_write(_gpio_phase[0], _invert_phase[0]);
            gpio_write(_gpio_phase[1], _invert_phase[1]);
            gpio_write(_gpio_phase[2], _invert_phase[2]);
            gpio_write(_gpio_phase[3], _invert_phase[3]);
        }
        _enabled = !disable;
    }

    void IRAM_ATTR UnipolarMotor::set_direction_isr(bool dir) {
        _dir = dir;
    }

    void IRAM_ATTR UnipolarMotor::step_isr() {
        if (!_enabled) {
            return;  // don't do anything, phase is not changed or lost
        }

        /*
			8 Step : A – AB – B – BC – C – CD – D – DA
			4 Step : AB – BC – CD – DA

			Step		IN4	IN3	IN2	IN1
			A 		0 	0 	0 	1
			AB		0	0	1	1
			B		0	0	1	0
			BC		0	1	1	0
			C		0	1	0	0
			CD		1	1	0	0
			D		1	0	0	0
			DA		1	0	0	1

            Packed one nibble per step, least significant nibble first, so the
            pattern for a step is a shift and mask of a compile-time constant.
            A lookup table would live in flash, which an ISR must not touch.
        */
        const uint32_t sequence   = _half_step ? 0x98c46231 : 0x00009c63;
        const uint8_t  phase_mask = _half_step ? 7 : 3;

        // Wrap by masking: both sequences are a power-of-two long, and masking
        // keeps _current_phase in range even if half_step changes at runtime.
        _current_phase = (_dir ? _current_phase + 1 : _current_phase - 1) & phase_mask;

        const uint32_t pattern = sequence >> (_current_phase * 4);

        gpio_write(_gpio_phase[0], ((pattern & 1) != 0) ^ _invert_phase[0]);
        gpio_write(_gpio_phase[1], ((pattern & 2) != 0) ^ _invert_phase[1]);
        gpio_write(_gpio_phase[2], ((pattern & 4) != 0) ^ _invert_phase[2]);
        gpio_write(_gpio_phase[3], ((pattern & 8) != 0) ^ _invert_phase[3]);
    }

    // Thunks stand in for virtual calls from the step ISR, so they live in IRAM
    // like the methods they forward to.  UnipolarMotor is final, so the
    // forwarded calls resolve directly instead of through the vtable.
    static void IRAM_ATTR unipolar_step_thunk(MotorDriver* driver) {
        static_cast<UnipolarMotor*>(driver)->step_isr();
    }
    static void IRAM_ATTR unipolar_dir_thunk(MotorDriver* driver, bool dir) {
        static_cast<UnipolarMotor*>(driver)->set_direction_isr(dir);
    }

    IsrStepFn UnipolarMotor::isr_step_fn() {
        return unipolar_step_thunk;
    }
    IsrDirFn UnipolarMotor::isr_dir_fn() {
        return unipolar_dir_thunk;
    }

    // Configuration registration
    namespace {
        MotorFactory::InstanceBuilder<UnipolarMotor> registration("unipolar");
    }
}
