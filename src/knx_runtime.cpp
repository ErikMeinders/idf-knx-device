// Copyright 2026 Erik Meinders. Part of idf-knx-device, licensed under GPLv3 (see LICENSE).

#include "knx_runtime.h"

#include <memory>

#include "driver/gpio.h"
#include "esp_err.h"

namespace {

// Mechanical bounce and (on boards where the prog-mode LED shares a nearby GPIO)
// electrical crosstalk can keep the pin flickering for tens of ms around a real
// press, so this requires several consecutive same-level samples before trusting
// the level, rather than trusting the first edge or a quiet-period-since-last-edge
// heuristic (which breaks down if edges keep arriving throughout the press).
constexpr int kDebounceSamples = 4; // ~40ms of consistent level at a 10ms tick

void knxRuntimeTask(void* arg)
{
    // Owned by this task for its entire (infinite) lifetime.
    std::unique_ptr<KnxRuntimeConfig> config(static_cast<KnxRuntimeConfig*>(arg));

    bool armed = true;
    int lowStreak = 0;
    int highStreak = 0;

    while (true) {
        config->loopFn();

        if (config->buttonGpio >= 0) {
            bool low = gpio_get_level((gpio_num_t)config->buttonGpio) == 0;
            if (low) {
                lowStreak++;
                highStreak = 0;
            } else {
                highStreak++;
                lowStreak = 0;
            }

            if (lowStreak == kDebounceSamples && armed) {
                armed = false;
                if (config->onButtonPress) {
                    config->onButtonPress();
                }
            } else if (highStreak == kDebounceSamples) {
                armed = true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(config->tickIntervalMs));
    }
}

} // namespace

void knx_runtime_start(const KnxRuntimeConfig& config)
{
    if (config.buttonGpio >= 0) {
        // The button is polled (active low), so it only needs to be an input
        // with a pull-up; no ISR is involved.
        gpio_config_t io = {};
        io.pin_bit_mask = 1ULL << config.buttonGpio;
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&io));
    }

    auto* configCopy = new KnxRuntimeConfig(config);
    xTaskCreate(knxRuntimeTask, config.taskName, config.taskStackSize, configCopy, config.taskPriority, nullptr);
}
