// Modified 2026 by Erik Meinders for idf-knx-device (https://github.com/ErikMeinders/idf-knx-device).
// Based on thelsing/knx (GPLv3) at commit 980c047. See README.md, "Changes from upstream".

#pragma once

#include "config.h"
#include "bau_systemB.h"
#include "device_object.h"
#include "address_table_object.h"
#include "association_table_object.h"
#include "group_object_table_object.h"
#include "security_interface_object.h"
#include "application_program_object.h"
#include "application_layer.h"
#include "secure_application_layer.h"
#include "transport_layer.h"
#include "network_layer_device.h"
#include "data_link_layer.h"
#include "platform.h"
#include "memory.h"

// idf-knx-device: group communication events reported to the application, e.g.
// to blink a status LED. `asap` is the group object number.
enum class GroupActivity : uint8_t
{
    ReadAnswered,  // a GroupValueRead for one of our objects was answered
    ReadIgnored,   // ... was not answered because the object has no value yet
    WriteSent,     // a GroupValueWrite from one of our objects was transmitted
    WriteReceived, // a GroupValueWrite updated one of our (write-enabled) objects
};
typedef void (*GroupActivityCallback)(GroupActivity activity, uint16_t asap);

class BauSystemBDevice : public BauSystemB
{
    public:
        BauSystemBDevice(Platform& platform);
        void loop() override;
        bool configured() override;
        GroupObjectTableObject& groupObjectTable();

        // Called from loop(), i.e. in the task that drives the stack. Keep it
        // short and do not call back into the stack from it.
        void groupActivityCallback(GroupActivityCallback callback);

    protected:
        void notifyGroupActivity(GroupActivity activity, uint16_t asap);
        GroupActivityCallback _groupActivityCallback = nullptr;

        ApplicationLayer& applicationLayer() override;

        void groupValueWriteLocalConfirm(AckType ack, uint16_t asap, Priority priority, HopCountType hopType, const SecurityControl& secCtrl,
                                         uint8_t* data, uint8_t dataLength, bool status) override;
        void groupValueReadLocalConfirm(AckType ack, uint16_t asap, Priority priority, HopCountType hopType, const SecurityControl& secCtrl, bool status) override;
        void groupValueReadIndication(uint16_t asap, Priority priority, HopCountType hopType, const SecurityControl& secCtrl) override;
        void groupValueReadAppLayerConfirm(uint16_t asap, Priority priority, HopCountType hopType, const SecurityControl& secCtrl,
                                           uint8_t* data, uint8_t dataLength) override;
        void groupValueWriteIndication(uint16_t asap, Priority priority, HopCountType hopType, const SecurityControl& secCtrl,
                                       uint8_t* data, uint8_t dataLength) override;

        void sendNextGroupTelegram();
        void updateGroupObject(GroupObject& go, uint8_t* data, uint8_t length);

        void doMasterReset(EraseCode eraseCode, uint8_t channel) override;

        AddressTableObject _addrTable;
        AssociationTableObject _assocTable;
        GroupObjectTableObject _groupObjTable;
#ifdef USE_DATASECURE
        SecureApplicationLayer _appLayer;
        SecurityInterfaceObject _secIfObj;
#else
        ApplicationLayer _appLayer;
#endif
        TransportLayer _transLayer;
        NetworkLayerDevice _netLayer;

        bool _configured = true;
};
