/* Copyright (C) 2015-2016 Andrew J. Kroll
   and
Copyright (C) 2011 Circuits At Home, LTD. All rights reserved.

This software may be distributed and modified under the terms of the GNU
General Public License version 2 (GPL2) as published by the Free Software
Foundation and appearing in the file GPL2.TXT included in the packaging of
this file. Please note that GPL2 Section 2[b] requires that all works based
on this software must also be made publicly available under the terms of
the GPL2 ("Copyleft").

Contact information
-------------------

Circuits At Home, LTD
Web      :  http://www.circuitsathome.com
e-mail   :  support@circuitsathome.com
 */

#if !defined(_UHS_host_h_) || defined(__ADDRESS_H__)
#error "Never include UHS_address.h directly; include UHS_Usb.h instead"
#else
#define __ADDRESS_H__



/* NAK powers. To save space in endpoint data structure, amount of retries before giving up and returning 0x4 is stored in */
/* bmNakPower as a power of 2. The actual nak_limit is then calculated as nak_limit = ( 2^bmNakPower - 1) */
#define USB_NAK_MAX_POWER               15      //NAK binary order maximum value
#define USB_NAK_DEFAULT                 14      //default 32K-1 NAKs before giving up
#define USB_NAK_NOWAIT                  1       //Single NAK stops transfer
#define USB_NAK_NONAK                   0       //Do not count NAKs, stop retrying after USB Timeout

#define bmUSB_DEV_ADDR_ADDRESS          0x07
#define bmUSB_DEV_ADDR_PARENT           0x38
#define bmUSB_DEV_ADDR_HUB              0x40

struct UHS_EpInfo {
        uint8_t epAddr; // Endpoint address
        uint8_t maxPktSize; // Maximum packet size

        union {
                uint8_t epAttribs;

                struct {
                        uint8_t bmSndToggle : 1; // Send toggle, when zero bmSNDTOG0, bmSNDTOG1 otherwise
                        uint8_t bmRcvToggle : 1; // Send toggle, when zero bmRCVTOG0, bmRCVTOG1 otherwise
                        uint8_t bmNakPower : 6; // Binary order for NAK_LIMIT value
                } __attribute__((packed));
        };
} __attribute__((packed));

//        7   6   5   4   3   2   1   0
//  ---------------------------------
//  |   | H | P | P | P | A | A | A |
//  ---------------------------------
//
// H - if 1 the address is a hub address
// P - parent hub address
// A - device address / port number in case of hub
//

struct UHS_DeviceAddress {

        union {

                struct {
                        uint8_t bmAddress : 3; // device address/port number
                        uint8_t bmParent : 3; // parent hub address
                        uint8_t bmHub : 1; // hub flag
                        uint8_t bmReserved : 1; // reserved, must be zero
                } __attribute__((packed));
                uint8_t devAddress;
        };
} __attribute__((packed));

struct UHS_Device {
        volatile UHS_EpInfo *epinfo; // endpoint info pointer
        UHS_DeviceAddress address;
        uint8_t bIface;
        uint8_t epcount; // number of endpoints
        uint8_t speed; // indicates device speed
} __attribute__((packed));

typedef void (*UsbDeviceHandleFunc)(UHS_Device *pdev);

class AddressPool {
        UHS_EpInfo dev0ep; //Endpoint data structure used during enumeration for uninitialized device

        uint8_t hubCounter; // hub counter is kept
        // in order to avoid hub address duplication

        UHS_Device thePool[UHS_HOST_MAX_INTERFACE_DRIVERS];

        // Initializes address pool entry

        void UHS_NI InitEntry(uint8_t index) {
                thePool[index].address.devAddress = 0;
                thePool[index].epcount = 1;
                thePool[index].speed = 0;
                thePool[index].epinfo = &dev0ep;
        };

        // Returns thePool index for a given address

        uint8_t UHS_NI FindAddressIndex(uint8_t address = 0) {
                for(uint8_t i = 1; i < UHS_HOST_MAX_INTERFACE_DRIVERS; i++) {
                        if(thePool[i].address.devAddress == address)
                                return i;
                }
                return 0;
        };

        // Returns thePool child index for a given parent

        uint8_t UHS_NI FindChildIndex(UHS_DeviceAddress addr, uint8_t start = 1) {
                for(uint8_t i = (start < 1 || start >= UHS_HOST_MAX_INTERFACE_DRIVERS) ? 1 : start; i < UHS_HOST_MAX_INTERFACE_DRIVERS; i++) {
                        if(thePool[i].address.bmParent == addr.bmAddress)
                                return i;
                }
                return 0;
        };

        // Frees address entry specified by index parameter

        void UHS_NI FreeAddressByIndex(uint8_t index) {
                // Zero field is reserved and should not be affected
                if(index == 0)
                        return;

                UHS_DeviceAddress uda = thePool[index].address;
                // If a hub was switched off all port addresses should be freed
                if(uda.bmHub == 1) {
                        for(uint8_t i = 1; (i = FindChildIndex(uda, i));)
                                FreeAddressByIndex(i);

                        // If the hub had the last allocated address, hubCounter should be decremented
                        if(hubCounter == uda.bmAddress)
                                hubCounter--;
                }
                InitEntry(index);
        }

        void InitAllAddresses(void) {
                for(uint8_t i = 1; i < UHS_HOST_MAX_INTERFACE_DRIVERS; i++) InitEntry(i);
                hubCounter = 0;
        };
public:

        AddressPool() {
                hubCounter = 0;
                // Zero address is reserved
                InitEntry(0);

                thePool[0].address.devAddress = 0;
                thePool[0].epinfo = &dev0ep;
                dev0ep.epAddr = 0;
                dev0ep.maxPktSize = 8;
                dev0ep.epAttribs = 0; //set DATA0/1 toggles to 0
                dev0ep.bmNakPower = USB_NAK_MAX_POWER;
                InitAllAddresses();
        };

        // Returns a pointer to a specified address entry

        UHS_Device* UHS_NI GetUsbDevicePtr(uint8_t addr) {
                if(!addr)
                        return thePool;

                uint8_t index = FindAddressIndex(addr);

                return (!index) ? NULL : &thePool[index];
        };

#if 0
        // Unused??
        // Performs an operation specified by pfunc for each addressed device

        void UHS_NI ForEachUsbDevice(UsbDeviceHandleFunc pfunc) {
                if(!pfunc)
                        return;

                for(uint8_t i = 1; i < UHS_HOST_MAX_INTERFACE_DRIVERS; i++)
                        if(thePool[i].address.devAddress)
                                pfunc(thePool + i);
        };
#endif

        // Allocates new address

        uint8_t UHS_NI AllocAddress(uint8_t parent, bool is_hub = false, uint8_t port = 1) {
                /* if (parent != 0 && port == 0)
                        USB_HOST_SERIAL.println("PRT:0"); */
                UHS_DeviceAddress _parent;
                _parent.devAddress = parent;
                if(_parent.bmReserved || port > 7)
                        //if(parent > 127 || port > 7)
                        return 0;

                if(is_hub && hubCounter == 7)
                        return 0;

                // finds first empty address entry starting from one
                uint8_t index = FindAddressIndex(0);

                if(!index) // if empty entry is not found
                        return 0;

#define OLD_JUNK_ADDRESS 0
#if OLD_JUNK_ADDRESS
                if(_parent.devAddress == 0) {
                        if(is_hub) {
                                hubCounter++;
                                thePool[index].address.devAddress = 0x41;
                        } else
                                thePool[index].address.devAddress = 1;

                        return thePool[index].address.devAddress;
                }
#endif
                UHS_DeviceAddress addr;
                addr.devAddress = port;
                addr.bmParent = _parent.bmAddress;
                if(is_hub) {
                        hubCounter++;
                        addr.bmHub = 1;
                      addr.bmAddress = hubCounter;
#if OLD_JUNK_ADDRESS
                } else {
                        addr.bmHub = 0;
                        addr.bmAddress = port;
#endif
                }
                thePool[index].address = addr;
#if DEBUG_PRINTF_EXTRA_HUGE
#if defined(UHS_DEBUG_USB_ADDRESS)
                printf("Address: %x (%x.%x.%x)\r\n", addr.devAddress, addr.bmHub, addr.bmParent, addr.bmAddress);
#endif
#endif
                return thePool[index].address.devAddress;
        };

        void UHS_NI FreeAddress(uint8_t addr) {
                // if the root hub is disconnected all the addresses should be initialized
                if(addr == 0x41) {
                        InitAllAddresses();
                        return;
                }
                uint8_t index = FindAddressIndex(addr);
                FreeAddressByIndex(index);
        };

#if 0

        class AddressPool {
        public:
                virtual UHS_Device* GetUsbDevicePtr(uint8_t addr) = 0;
                virtual uint8_t AllocAddress(uint8_t parent, bool is_hub = false, uint8_t port = 0) = 0;
                virtual void FreeAddress(uint8_t addr) = 0;
        };

        class AddressPoolImpl : public AddressPool {
                UHS_EpInfo dev0ep; //Endpoint data structure used during enumeration for uninitialized device

                uint8_t hubCounter; // hub counter is kept
                // in order to avoid hub address duplication

                UHS_Device thePool[UHS_HOST_MAX_INTERFACE_DRIVERS];

                // Initializes address pool entry

                void InitEntry(uint8_t index) {
                        thePool[index].address.devAddress = 0;
                        thePool[index].epcount = 1;
                        thePool[index].speed = 0;
                        thePool[index].epinfo = &dev0ep;
                };

                // Returns thePool index for a given address

                uint8_t FindAddressIndex(uint8_t address = 0) {
                        for(uint8_t i = 1; i < UHS_HOST_MAX_INTERFACE_DRIVERS; i++) {
                                if(thePool[i].address.devAddress == address)
                                        return i;
                        }
                        return 0;
                };

                // Returns thePool child index for a given parent

                uint8_t FindChildIndex(UHS_DeviceAddress addr, uint8_t start = 1) {
                        for(uint8_t i = (start < 1 || start >= UHS_HOST_MAX_INTERFACE_DRIVERS) ? 1 : start; i < UHS_HOST_MAX_INTERFACE_DRIVERS; i++) {
                                if(thePool[i].address.bmParent == addr.bmAddress)
                                        return i;
                        }
                        return 0;
                };

                // Frees address entry specified by index parameter

                void FreeAddressByIndex(uint8_t index) {
                        // Zero field is reserved and should not be affected
                        if(index == 0)
                                return;

                        UHS_DeviceAddress uda = thePool[index].address;
                        // If a hub was switched off all port addresses should be freed
                        if(uda.bmHub == 1) {
                                for(uint8_t i = 1; (i = FindChildIndex(uda, i));)
                                        FreeAddressByIndex(i);

                                // If the hub had the last allocated address, hubCounter should be decremented
                                if(hubCounter == uda.bmAddress)
                                        hubCounter--;
                        }
                        InitEntry(index);
                }

                // Initializes the whole address pool at once

                void InitAllAddresses(void) {
                        for(uint8_t i = 1; i < UHS_HOST_MAX_INTERFACE_DRIVERS; i++) InitEntry(i);
                        hubCounter = 0;
                };

        public:

                AddressPoolImpl() : hubCounter(0) {
                        // Zero address is reserved
                        InitEntry(0);

                        thePool[0].address.devAddress = 0;
                        thePool[0].epinfo = &dev0ep;
                        dev0ep.epAddr = 0;
                        dev0ep.maxPktSize = 8;
                        dev0ep.epAttribs = 0; //set DATA0/1 toggles to 0
                        dev0ep.bmNakPower = USB_NAK_MAX_POWER;

                        InitAllAddresses();
                };

                // Returns a pointer to a specified address entry

                virtual UHS_Device* GetUsbDevicePtr(uint8_t addr) {
                        if(!addr)
                                return thePool;

                        uint8_t index = FindAddressIndex(addr);

                        return (!index) ? NULL : &thePool[index];
                };

                // Performs an operation specified by pfunc for each addressed device

                void ForEachUsbDevice(UsbDeviceHandleFunc pfunc) {
                        if(!pfunc)
                                return;

                        for(uint8_t i = 1; i < UHS_HOST_MAX_INTERFACE_DRIVERS; i++)
                                if(thePool[i].address.devAddress)
                                        pfunc(thePool + i);
                };

                // Allocates new address

                virtual uint8_t AllocAddress(uint8_t parent, bool is_hub = false, uint8_t port = 0) {
                        /* if (parent != 0 && port == 0)
                                USB_HOST_SERIAL.println("PRT:0"); */
                        UHS_DeviceAddress _parent;
                        _parent.devAddress = parent;
                        if(_parent.bmReserved || port > 7)
                                //if(parent > 127 || port > 7)
                                return 0;

                        if(is_hub && hubCounter == 7)
                                return 0;

                        // finds first empty address entry starting from one
                        uint8_t index = FindAddressIndex(0);

                        if(!index) // if empty entry is not found
                                return 0;

                        if(_parent.devAddress == 0) {
                                if(is_hub) {
                                        thePool[index].address.devAddress = 0x41;
                                        hubCounter++;
                                } else
                                        thePool[index].address.devAddress = 1;

                                return thePool[index].address.devAddress;
                        }

                        UHS_DeviceAddress addr;
                        addr.devAddress = 0; // Ensure all bits are zero
                        addr.bmParent = _parent.bmAddress;
                        if(is_hub) {
                                addr.bmHub = 1;
                                addr.bmAddress = ++hubCounter;
                        } else {
                                addr.bmHub = 0;
                                addr.bmAddress = port;
                        }
                        thePool[index].address = addr;
                        /*
                                        USB_HOST_SERIAL.print("Addr:");
                                        USB_HOST_SERIAL.print(addr.bmHub, HEX);
                                        USB_HOST_SERIAL.print(".");
                                        USB_HOST_SERIAL.print(addr.bmParent, HEX);
                                        USB_HOST_SERIAL.print(".");
                                        USB_HOST_SERIAL.println(addr.bmAddress, HEX);
                         */
                        return thePool[index].address.devAddress;
                };

                // Empties pool entry

                virtual void FreeAddress(uint8_t addr) {
                        // if the root hub is disconnected all the addresses should be initialized
                        if(addr == 0x41) {
                                InitAllAddresses();
                                return;
                        }
                        uint8_t index = FindAddressIndex(addr);
                        FreeAddressByIndex(index);
                };

#endif

        };

#endif // __ADDRESS_H__
