// KNX IP device demo for idf-knx-device.
//
// Simulated temperature sensor with the group object and parameter layout of
// thelsing/knx's knx-demo, so the ETS product in ../ets/knx-demo-ip.knxprod can
// commission it:
//   GO 1  current temperature (DPT 9.001, transmit)
//   GO 2  maximum temperature (DPT 9.001, transmit)
//   GO 3  minimum temperature (DPT 9.001, transmit)
//   GO 4  reset min/max       (DPT 1.017, write)

#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"

#include "esp32_idf_platform.h"
#include "knx/bau57B0.h"
#include "knx/dpt.h"
#include "knx_facade.h"
#include "knx_runtime.h"

static const char* TAG = "knx_demo";

static Esp32IdfPlatform knxPlatform;
static Bau57B0 knxBau(knxPlatform);
static KnxFacade<Esp32IdfPlatform, Bau57B0> knx(knxBau);

#define goCurrent knx.getGroupObject(1)
#define goMax knx.getGroupObject(2)
#define goMin knx.getGroupObject(3)
#define goReset knx.getGroupObject(4)

// ---------------------------------------------------------------------------
// Wi-Fi

static esp_netif_t* s_netif = nullptr;
static SemaphoreHandle_t s_netifReady = nullptr;

static void wifiEventHandler(void*, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(TAG, "Got IP " IPSTR, IP2STR(&event->ip_info.ip));
        // The platform reads its IP address, netmask and gateway from this
        // netif, and needs it before knx.start() joins the multicast group.
        knxPlatform.setNetif(s_netif);
        xSemaphoreGive(s_netifReady);
    }
}

static void wifiStart()
{
    s_netifReady = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEventHandler, nullptr, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifiEventHandler, nullptr, nullptr));

    wifi_config_t wifiConfig = {};
    strncpy(reinterpret_cast<char*>(wifiConfig.sta.ssid), CONFIG_DEMO_WIFI_SSID, sizeof(wifiConfig.sta.ssid) - 1);
    strncpy(reinterpret_cast<char*>(wifiConfig.sta.password), CONFIG_DEMO_WIFI_PASSWORD, sizeof(wifiConfig.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// ---------------------------------------------------------------------------
// Application

static float s_min = 1000.0f;
static float s_max = -1000.0f;
static int64_t s_lastSendMs = 0;

static void onReset(GroupObject& go)
{
    if (go.value()) {
        s_min = 1000.0f;
        s_max = -1000.0f;
        ESP_LOGI(TAG, "Min/max reset");
    }
}

// Runs in the KNX runtime task, right after knx.loop(), so group objects are
// only ever touched from one task.
static void measure()
{
    int64_t nowMs = esp_timer_get_time() / 1000;
    if (nowMs - s_lastSendMs < 10000) {
        return;
    }
    s_lastSendMs = nowMs;

    float temp = 18.0f + (esp_random() % 800) / 100.0f; // 18.00 .. 25.99 °C
    goCurrent.value(temp);
    if (temp > s_max) {
        s_max = temp;
        goMax.value(s_max);
    }
    if (temp < s_min) {
        s_min = temp;
        goMin.value(s_min);
    }
    ESP_LOGI(TAG, "Temperature %.2f (min %.2f, max %.2f)", temp, s_min, s_max);
}

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    wifiStart();

    // Load what ETS downloaded last time (address/association/GO tables and
    // parameters). Until the device has been commissioned, configured() is
    // false and the group objects have no backing storage: don't touch them.
    knx.readMemory();
    if (knx.configured()) {
        goReset.callback(onReset);
        goReset.dataPointType(DPT_Trigger);
        goCurrent.dataPointType(DPT_Value_Temp);
        goMax.dataPointType(DPT_Value_Temp);
        goMin.dataPointType(DPT_Value_Temp);
        ESP_LOGI(TAG, "Configured. Parameters: startup delay %d, send on change %d, cyclic %d, min/max %d, correction %d",
                 knx.paramByte(0), knx.paramByte(1), knx.paramByte(2), knx.paramByte(3), knx.paramByte(4));
    } else {
        ESP_LOGW(TAG, "Not configured: press the programming button and download from ETS");
    }

    if (xSemaphoreTake(s_netifReady, pdMS_TO_TICKS(15000)) != pdTRUE) {
        ESP_LOGW(TAG, "No IP address yet; KNX multicast join may fail");
    }

#if CONFIG_DEMO_PROG_LED_GPIO >= 0
    knx.ledPin(CONFIG_DEMO_PROG_LED_GPIO);
#if CONFIG_DEMO_PROG_LED_ACTIVE_HIGH
    knx.ledPinActiveOn(1);
#else
    knx.ledPinActiveOn(0);
#endif
#endif

    knx.start();

    KnxRuntimeConfig runtime;
    runtime.loopFn = []() {
        knx.loop();
        if (knx.configured()) {
            measure();
        }
    };
    runtime.buttonGpio = CONFIG_DEMO_PROG_BUTTON_GPIO;
    runtime.onButtonPress = []() {
        knx.toggleProgMode();
        ESP_LOGI(TAG, "Programming mode %s", knx.progMode() ? "on" : "off");
    };
    knx_runtime_start(runtime);
}
