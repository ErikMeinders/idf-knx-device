# idf-knx-device -- ESP-IDF native build of the thelsing/knx device stack.
#
# Only the platform-independent core (src/knx), the ESP-IDF platform
# (esp32_idf_platform.*), the facade (knx_facade.*) and the FreeRTOS runtime
# helper (knx_runtime.*) are shipped; upstream's Arduino/RTOS platforms are not.

file(GLOB KNX_CORE_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/knx/*.cpp")

idf_component_register(
    SRCS
        "src/esp32_idf_platform.cpp"
        "src/knx_facade.cpp"
        "src/knx_runtime.cpp"
        ${KNX_CORE_SRCS}
    # Only src/ is public. Core headers are reached as "knx/<name>.h"; inside
    # src/knx they include each other by bare name, which resolves relative to
    # the including file, so generic names such as config.h and memory.h are
    # not put on every consumer's include path.
    INCLUDE_DIRS "src"
    REQUIRES
        esp_netif
        esp_driver_uart
        esp_driver_gpio
        esp_timer
        nvs_flash
        esp_wifi
        lwip
        freertos
    PRIV_REQUIRES
        esp_system
)

# The core reads these in every translation unit (src/knx/config.h), and the
# consumer's own code sees them through knx_facade.h, so they are PUBLIC.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    MASK_VERSION=${CONFIG_KNX_MASK_VERSION}
    KNX_FLASH_SIZE=${CONFIG_KNX_FLASH_SIZE}
    # The application owns the KnxFacade instance (platform + BAU objects);
    # upstream's implicit global `knx` would hide that choice.
    KNX_NO_AUTOMATIC_GLOBAL_INSTANCE
)
