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
    // Pin the task to this core, e.g. to keep a core free for timing-critical work.
    // Must be tskNO_AFFINITY or 0 .. portNUM_PROCESSORS - 1 (only 0 on single-core chips
    // such as the ESP32-C6); knx_runtime_start() refuses anything else.
    BaseType_t coreId = tskNO_AFFINITY;
    UBaseType_t postQueueLength = 16;     // capacity for knx_runtime_post(); 0 disables posting
};

// Spawns the runtime task and returns immediately -- does not block the caller.
// The task runs forever, so app_main() (or whoever calls this) is free to return.
// Call it once. If coreId is invalid or the task can't be created, it logs an error
// and starts nothing (knx_runtime_post() then returns false).
void knx_runtime_start(const KnxRuntimeConfig& config);

// Runs `fn` in the runtime task, before its next loopFn. The KNX stack is not
// thread-safe, and an ETS download reallocates group objects inside knx.loop(), so this
// is how other tasks safely read or write group objects and parameters. Returns false
// when the runtime isn't started, posting is disabled, or the queue stays full for
// `wait` ticks. From inside loopFn or a posted function, use wait = 0: the runtime
// task can't empty its own queue while it waits.
bool knx_runtime_post(std::function<void()> fn, TickType_t wait = 0);
