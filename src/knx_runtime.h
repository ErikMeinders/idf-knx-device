// Copyright 2026 Erik Meinders. Part of idf-knx-device, licensed under GPLv3 (see LICENSE).

#pragma once

// Owns the FreeRTOS task that drives a KnxFacade's loop() and (optionally) polls a
// physical programming-mode button with software debounce. Callers stay decoupled
// from the specific KnxFacade<Platform, Bau> template instantiation (which is
// application-specific -- the mask/Bau type varies per device) by supplying plain
// callbacks instead of the facade object itself.

#include <cstdint>
#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct KnxRuntimeConfig
{
    std::function<void()> loopFn;         // required: e.g. []{ knx.loop(); }
    std::function<void()> onButtonPress;  // called once per debounced press; ignored if buttonGpio < 0
    int buttonGpio = -1;                  // active-low prog-mode button (configured as input + pull-up), or -1 to disable
    uint32_t tickIntervalMs = 10;         // matches the ~10ms cadence the button debounce below is tuned for
    uint32_t taskStackSize = 4096;
    UBaseType_t taskPriority = 5;
    const char* taskName = "knx_runtime";
};

// Spawns the runtime task and returns immediately -- does not block the caller.
// The task runs forever, so app_main() (or whoever calls this) is free to return.
void knx_runtime_start(const KnxRuntimeConfig& config);
