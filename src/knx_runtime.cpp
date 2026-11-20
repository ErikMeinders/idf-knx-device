// Copyright 2026 Erik Meinders. Part of idf-knx-device, licensed under GPLv3 (see LICENSE).

#include "knx_runtime.h"

#include <atomic>
#include <memory>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/queue.h"

namespace {

const char* TAG = "knx_runtime";

// Holds heap-allocated std::function pointers; the runtime task runs and deletes them.
// Atomic because knx_runtime_start() sets it while other tasks may already be posting.
std::atomic<QueueHandle_t> s_postQueue{ nullptr };

void runPosted()
{
    QueueHandle_t queue = s_postQueue.load();
    if (!queue) {
        return;
    }
    // Only what is waiting now: a task that keeps posting must not starve loopFn.
    UBaseType_t waiting = uxQueueMessagesWaiting(queue);
    std::function<void()>* fn = nullptr;
    while (waiting-- > 0 && xQueueReceive(queue, &fn, 0) == pdTRUE) {
        std::unique_ptr<std::function<void()>> owned(fn);
        if (*owned) {
            (*owned)();
        }
    }
}

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
        runPosted();
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
    if (config.coreId != tskNO_AFFINITY && (config.coreId < 0 || config.coreId >= portNUM_PROCESSORS)) {
        ESP_LOGE(TAG, "coreId %d is not tskNO_AFFINITY or below %d; runtime not started",
                 (int)config.coreId, (int)portNUM_PROCESSORS);
        return;
    }

    if (config.buttonGpio >= 0) {
        // The button is polled (active low), so it only needs to be an input
        // with a pull-up; no ISR is involved.
        gpio_config_t io = {};
        io.pin_bit_mask = 1ULL << config.buttonGpio;
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&io));
    }

    QueueHandle_t createdQueue = nullptr;
    if (config.postQueueLength > 0 && !s_postQueue.load()) {
        createdQueue = xQueueCreate(config.postQueueLength, sizeof(std::function<void()>*));
        if (!createdQueue) {
            ESP_LOGE(TAG, "No memory for the post queue; knx_runtime_post() is disabled");
        }
    }

    auto* configCopy = new KnxRuntimeConfig(config);
    if (xTaskCreatePinnedToCore(knxRuntimeTask, config.taskName, config.taskStackSize, configCopy,
                                config.taskPriority, nullptr, config.coreId) != pdPASS) {
        ESP_LOGE(TAG, "Could not create the %s task; runtime not started", config.taskName);
        delete configCopy;
        if (createdQueue) {
            vQueueDelete(createdQueue);  // never published, so nothing can have been posted
        }
        return;
    }
    // Published only once the task exists, so knx_runtime_post() returns false until
    // then and after a failed start. The task skips posting while it is still null.
    if (createdQueue) {
        s_postQueue.store(createdQueue);
    }
}

bool knx_runtime_post(std::function<void()> fn, TickType_t wait)
{
    QueueHandle_t queue = s_postQueue.load();
    if (!queue || !fn) {
        return false;
    }
    auto* heapFn = new std::function<void()>(std::move(fn));
    if (xQueueSend(queue, &heapFn, wait) != pdTRUE) {
        delete heapFn;
        return false;
    }
    return true;
}
