// Modified 2026 by Erik Meinders for idf-knx-device (https://github.com/ErikMeinders/idf-knx-device).
// Based on thelsing/knx (GPLv3) at commit 980c047. See README.md, "Changes from upstream".

#ifndef ARDUINO
#ifdef ESP_PLATFORM
// esp_idf_platform.h
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "driver/uart.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "knx/platform.h"// Include the provided base class

// A snapshot of the multicast link counters, see Esp32IdfPlatform::linkStats().
// Addresses are IPv4 in host byte order. Times come from esp_timer, in ms.
struct KnxLinkStats
{
    static constexpr size_t kMaxSources = 8;
    struct Source
    {
        uint32_t ip;        // 0 = unused slot
        uint32_t count;     // datagrams received from this address
        uint32_t lastMs;    // when the last one arrived
    };

    // Own telegrams heard back through multicast loopback. The own address is taken at
    // the join and at every renewal, so with CONFIG_KNX_IGMP_REFRESH_S=0 a later DHCP
    // address change counts own telegrams under rxOthers.
    uint32_t rxOwn;
    uint32_t rxOthers;        // datagrams from every other address
    uint32_t rxUntracked;     // of rxOthers: senders beyond the kMaxSources table
    uint32_t msSinceOthers;   // since the last datagram from another address; UINT32_MAX = never
    uint32_t renewals;        // multicast memberships renewed (IGMP report sent)
    uint32_t msSinceRenewal;  // since the last renewal attempt (or the join)
    uint32_t renewalFailures; // renewals lwIP refused
    uint32_t sendFailures;    // sendto() calls that failed
    Source sources[kMaxSources]; // other addresses, in order of first appearance
};

class Esp32IdfPlatform : public Platform
{
  public:
    Esp32IdfPlatform(uart_port_t uart_num = UART_NUM_1);
    ~Esp32IdfPlatform();

    // uart
    void knxUartPins(int8_t rxPin, int8_t txPin);
    void knxUartBaudRate(uint32_t baudRate); // Add baud rate configuration

    // Call this after WiFi/Ethernet has started and received an IP.
    void setNetif(esp_netif_t* netif);

    // --- Overridden Virtual Functions ---

    // ip stuff
    uint32_t currentIpAddress() override;
    uint32_t currentSubnetMask() override;
    uint32_t currentDefaultGateway() override;
    void macAddress(uint8_t* addr) override;

    // unique serial number
    uint32_t uniqueSerialNumber() override;

    // basic stuff (pure virtual in base)
    void restart() override;
    void fatalError() override;

    // multicast
    void setupMultiCast(uint32_t addr, uint16_t port) override;
    void closeMultiCast() override;
    bool sendBytesMultiCast(uint8_t* buffer, uint16_t len) override;
    int readBytesMultiCast(uint8_t* buffer, uint16_t maxLen, uint32_t& src_addr, uint16_t& src_port) override;

    // True when a datagram is waiting on the multicast socket, so the next
    // readBytesMultiCast() returns at once. Never blocks. KnxFacade::loop() uses it to
    // drain bursts: lwIP queues only CONFIG_LWIP_UDP_RECVMBOX_SIZE datagrams per socket
    // (6 by default) and silently drops the rest.
    bool multicastPending();

    // Renews the KNX multicast membership while the socket stays open: in the lwIP
    // thread, schedules an unsolicited IGMP membership report (sent within 0.5 s) for
    // every group the interface has joined. It sends no IGMP Leave; only if lwIP no
    // longer knows the KNX group is it joined again. Wi-Fi access points and mesh
    // systems without an IGMP querier stop forwarding multicast to a station at random
    // moments; a report makes them resume within seconds. The platform already does this
    // every CONFIG_KNX_IGMP_REFRESH_S seconds from readBytesMultiCast(); call it yourself
    // when a telegram you expect regularly stays away. Can be called from any task (but
    // see CHANGELOG, "Known issues"). Returns false when the socket isn't open or lwIP
    // refused.
    bool refreshMembership();

    // Counters for diagnosing reception, safe to call from any task.
    KnxLinkStats linkStats() const;

    // unicast
    bool sendBytesUniCast(uint32_t addr, uint16_t port, uint8_t* buffer, uint16_t len) override;

    // UART
    void setupUart() override;
    void closeUart() override;
    int uartAvailable() override;
    size_t writeUart(const uint8_t data) override;
    size_t writeUart(const uint8_t* buffer, size_t size) override;
    int readUart() override;
    size_t readBytesUart(uint8_t* buffer, size_t length) override;
    void flushUart() override;

    // Memory (EEPROM emulation via NVS)
    // We override these two functions to provide the low-level storage mechanism.
    // The base Platform class will use them when _memoryType is Eeprom.
    uint8_t* getEepromBuffer(uint32_t size) override;
    void commitToEeprom() override;

  private:
    // Network
    esp_netif_t* _netif = nullptr;
    int _sock = -1;
    struct sockaddr_in _remote_addr;
    uint32_t _multicast_addr = 0;
    uint16_t _multicast_port = 0;
    std::atomic<uint32_t> _ownIp{ 0 };   // host byte order, for telling own telegrams apart
    std::atomic<uint32_t> _lastRefreshMs{ 0 };

    void countReceived(uint32_t srcAddr);

    // Link counters: written by the KNX task, read by any task through linkStats().
    struct SourceSlot
    {
        std::atomic<uint32_t> ip{ 0 };
        std::atomic<uint32_t> count{ 0 };
        std::atomic<uint32_t> lastMs{ 0 };
    };
    SourceSlot _sources[KnxLinkStats::kMaxSources];
    std::atomic<uint32_t> _rxOwn{ 0 };
    std::atomic<uint32_t> _rxOthers{ 0 };
    std::atomic<uint32_t> _rxUntracked{ 0 };
    std::atomic<uint32_t> _lastOthersMs{ 0 };
    std::atomic<bool> _heardOthers{ false };
    std::atomic<uint32_t> _renewals{ 0 };
    std::atomic<uint32_t> _renewalFailures{ 0 };
    std::atomic<uint32_t> _sendFailures{ 0 };

    // UART
    uart_port_t _uart_num;
    int8_t _rxPin = -1;
    int8_t _txPin = -1;
    uint32_t _baudRate = 19200; // Default baud rate, can be changed
    bool _uart_installed = false;

    // NVS (for EEPROM emulation)
    nvs_handle_t _nvs_handle;
    uint8_t* _eeprom_buffer = nullptr;
    uint32_t _eeprom_size = 0;
    const char* _nvs_namespace = "eeprom";
    const char* _nvs_key = "eeprom";
};
#endif
#endif