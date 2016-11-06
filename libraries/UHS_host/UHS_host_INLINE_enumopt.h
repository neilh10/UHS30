
/*
 * File:   UHS_host_inline_enumopt.h
 * Author:  neilh77  on November 6, 2016, 9:05 AM
 *   Copied from including file
 */

#ifndef UHS_HOST_INLINE_ENUMOPT_H
#define UHS_HOST_INLINE_ENUMOPT_H

/**
 * Enumerates interfaces on devices
 *
 * @param parent index to Parent
 * @param port what port on the parent
 * @param speed the speed of the device
 * @return Zero for success, or error code
 */
uint8_t UHS_USB_HOST_BASE::Configuring(uint8_t parent, uint8_t port, uint8_t speed) {
        //uint8_t bAddress = 0;
        HOST_DUBUG("\r\n\r\n\r\nConfiguring: parent = %i, port = %i, speed = %i\r\n", parent, port, speed);
        uint8_t rcode = 0;
        uint8_t retries = 0;
        uint8_t numinf = 0;
        // Since any descriptor we are interested in should not be > 18 bytes, there really is no need for a parser.
        // I can do everything in one reusable buffer. :-)
        //const uint8_t biggest = max(max(max(sizeof
#if defined(UHS_DEVICE_WINDOWS_USB_SPEC_VIOLATION_DESCRIPTOR_DEVICE)
	const uint8_t biggest = 0x40;
#else
	const uint8_t biggest = 18;
#endif
        uint8_t buf[biggest];
        USB_DEVICE_DESCRIPTOR *udd = reinterpret_cast<USB_DEVICE_DESCRIPTOR *>(buf);
        USB_CONFIGURATION_DESCRIPTOR *ucd = reinterpret_cast<USB_CONFIGURATION_DESCRIPTOR *>(buf);

        //USB *pUsb = this;
        UHS_Device *p = NULL;
        //EpInfo epInfo; // cap at 16, this should be fairly reasonable.
        ENUMERATION_INFO ei;
        uint8_t bestconf = 0;
        uint8_t bestsuccess = 0;

        uint8_t devConfigIndex;
        //for(devConfigIndex = 0; devConfigIndex < UHS_HOST_MAX_INTERFACE_DRIVERS; devConfigIndex++) {
        //        if((devConfig[devConfigIndex]->bAddress) && (!devConfig[devConfigIndex]->bPollEnable)) {
        //                devConfig[devConfigIndex]->bAddress = 0;
        //        }
        //}
        //        Serial.print("HOST USB Host @ 0x");
        //        Serial.println((uint32_t)this, HEX);
        //        Serial.print("HOST USB Host Address Pool @ 0x");
        //        Serial.println((uint32_t)GetAddressPool(), HEX);

        sof_delay(200);
        p = addrPool.GetUsbDevicePtr(0);
        if(!p) {
                HOST_DUBUG("Configuring error: USB_ERROR_ADDRESS_NOT_FOUND_IN_POOL\r\n");
                return UHS_HOST_ERROR_NO_ADDRESS_IN_POOL;
        }

        p->speed = speed;
#if defined(UHS_DEVICE_WINDOWS_USB_SPEC_VIOLATION_DESCRIPTOR_DEVICE)
        p->epinfo[0].maxPktSize = 0x40; // Windows bug is expected.
        // poison data
        udd->bMaxPacketSize0 = 0U;
#else
        p->epinfo[0].maxPktSize = 0x08; // USB Spec, start small, work your way up.
#endif
again:
        HOST_DUBUG("\r\n\r\nConfiguring PktSize x%2.2x,  rcode: x%2.2x, retries %i,\r\n", p->epinfo[0].maxPktSize,rcode,retries);
#if defined(UHS_DEVICE_WINDOWS_USB_SPEC_VIOLATION_DESCRIPTOR_DEVICE)
        rcode = getDevDescr(0, biggest, (uint8_t*)buf,1);
        if (rcode || udd->bMaxPacketSize0 < 8)
#else
        rcode = getDevDescr(0, biggest, (uint8_t*)buf);
        if(rcode)
#endif
        {
                if(rcode == UHS_HOST_ERROR_JERR && retries < 4) {
                        //
                        // Some devices return JERR when plugged in.
                        // Attempts to reinitialize the device usually works.
                        //
                        // I have a hub that will refuse to work and acts like
                        // this unless external power is supplied.
                        // So this may not always work, and you may be fooled.
                        //
                        sof_delay(100);
                        retries++;
                        goto again;
#if defined(UHS_DEVICE_WINDOWS_USB_SPEC_VIOLATION_DESCRIPTOR_DEVICE)
                } else if((rcode == UHS_HOST_ERROR_DMA && retries < 4) || (udd->bMaxPacketSize0 < 8 && !rcode)) {
                        if(p->epinfo[0].maxPktSize > 8) p->epinfo[0].maxPktSize = p->epinfo[0].maxPktSize >> 1;
#else
                } else if(rcode == UHS_HOST_ERROR_DMA && retries < 4) {
                        if(p->epinfo[0].maxPktSize < 32) p->epinfo[0].maxPktSize = p->epinfo[0].maxPktSize << 1;
#endif
                        HOST_DUBUG("Configuring error: UHS_HOST_ERROR_DMA. Retry with maxPktSize: %i\r\n", p->epinfo[0].maxPktSize);
                        doSoftReset(parent, port, 0);
                        retries++;
                        sof_delay(200);
                        goto again;
                }
                HOST_DUBUG("Configuring error: %2.2x Can't get USB_DEVICE_DESCRIPTOR\r\n", rcode);
                return rcode;
        }

        //HOST_DUBUG("retries: %i\r\n", retries);

        ei.address = addrPool.AllocAddress(parent, IsHub(ei.klass), port);

        if(!ei.address) {
                return UHS_HOST_ERROR_ADDRESS_POOL_FULL;
        }

        p = addrPool.GetUsbDevicePtr(ei.address);
        // set to 1 if you suspect address table corruption.
#if 0
        if(!p) {
                return UHS_HOST_ERROR_NO_ADDRESS_IN_POOL;
        }
#endif

        p->speed = speed;

        rcode = doSoftReset(parent, port, ei.address);

        if(rcode) {
                addrPool.FreeAddress(ei.address);
                HOST_DUBUG("Configuring error: %2.2x Can't set USB INTERFACE ADDRESS\r\n", rcode);
                return rcode;
        }

#if defined(UHS_DEVICE_WINDOWS_USB_SPEC_VIOLATION_DESCRIPTOR_DEVICE)
        HOST_DUBUG("DevDescr 2nd poll \r\n");
        UHS_EpInfo dev1ep;
                dev1ep.maxPktSize =  buf[0];
                dev1ep.epAddr = 0;
                p->address.devAddress = ei.address;
                p->epcount = 1;
                p->epinfo = &dev1ep;

                 //if (rcode)   HOST_DUBUG("doSoftReset err: 0x%x & new msg sz:x%x\r\n", rcode,msg_sz);
                 sof_delay(10);

                 //if(!rcode)
                 {
                      rcode = getDevDescr(ei.address, dev1ep.maxPktSize, (uint8_t*)buf,1);
                     if (rcode)   HOST_DUBUG("getDevDescr err: 0x%x \r\n", rcode);
                 }

                if(rcode)  {
                       addrPool.FreeAddress(ei.address);
                       return rcode;
                }
                 sof_delay(10);

#endif

        ei.vid = udd->idVendor;
        ei.pid = udd->idProduct;
        ei.bcdDevice = udd->bcdDevice;
        ei.klass = udd->bDeviceClass;
        ei.subklass = udd->bDeviceSubClass;
        ei.protocol = udd->bDeviceProtocol;
        ei.bMaxPacketSize0 = udd->bMaxPacketSize0;
        ei.currentconfig = 0;
        ei.parent = parent;
        uint8_t configs = udd->bNumConfigurations;
#if 0
        rcode = doSoftReset(parent, port, ei.address);

        if(rcode) {
                addrPool.FreeAddress(ei.address);
                HOST_DUBUG("Configuring error: %2.2x Can't set USB INTERFACE ADDRESS\r\n", rcode);
                return rcode;
        }
#endif //0
        if(configs < 1) {
                HOST_DUBUG("No interfaces?!\r\n");
                addrPool.FreeAddress(ei.address);
                // rcode = TestInterface(&ei);
                // Not implemented (yet)
                rcode = UHS_HOST_ERROR_DEVICE_NOT_SUPPORTED;
        } else {
                HOST_DUBUG("configs: %i\r\n", configs);
                for(uint8_t conf = 0; (!rcode) && (conf < configs); conf++) {
                        // read the config descriptor into a buffer.
                        rcode = getConfDescr(ei.address, sizeof (USB_CONFIGURATION_DESCRIPTOR), conf, buf);
                        if(rcode) {
                                HOST_DUBUG("Configuring error: %2.2x Can't get USB_INTERFACE_DESCRIPTOR\r\n", rcode);
                                rcode = UHS_HOST_ERROR_FailGetConfDescr;
                                continue;
                        }
                        ei.currentconfig = conf;
                        numinf = ucd->bNumInterfaces; // Does _not_ include alternates!
                        HOST_DUBUG("CONFIGURATION: %i, bNumInterfaces %i, wTotalLength %i\r\n", conf, numinf, ucd->wTotalLength);
                        uint8_t success = 0;
                        uint16_t inf = 0;
                        uint8_t data[ei.bMaxPacketSize0];
                        UHS_EpInfo *pep;
                        pep = ctrlReqOpen(ei.address, mkSETUP_PKT8(UHS_bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, ei.currentconfig, USB_DESCRIPTOR_CONFIGURATION, 0x0000U, ucd->wTotalLength), data);
                        if(!pep) {
                                rcode = UHS_HOST_ERROR_NULL_EPINFO;
                                continue;
                        }
                        uint16_t left;
                        uint16_t read;
                        uint8_t offset;
                        rcode = initDescrStream(&ei, ucd, pep, data, &left, &read, &offset);
                        if(rcode) {
                                HOST_DUBUG("Configuring error: %2.2x Can't get USB_INTERFACE_DESCRIPTOR stream.\r\n", rcode);
                                break;
                        }
                        for(; (numinf) && (!rcode); inf++) {
                                // iterate for each interface on this config
                                rcode = getNextInterface(&ei, pep, data, &left, &read, &offset);
                                if(rcode == UHS_HOST_ERROR_END_OF_STREAM) {
                                        HOST_DUBUG("USB_INTERFACE END OF STREAM\r\n");
                                        ctrlReqClose(pep, UHS_bmREQ_GET_DESCR, left, ei.bMaxPacketSize0, data);
                                        rcode = 0;
                                        break;
                                }
                                if(rcode) {
                                        HOST_DUBUG("Configuring error: %2.2x Can't close USB_INTERFACE_DESCRIPTOR stream.\r\n", rcode);
                                        continue;
                                }
                                rcode = TestInterface(&ei);
                                if(!rcode) success++;
                                rcode = 0;
                        }
                        if(!inf) {
                                rcode = TestInterface(&ei);
                                if(!rcode) success++;
                                rcode = 0;
                        }
                        if(success > bestsuccess) {
                                bestconf = conf;
                                bestsuccess = success;
                        }
                }
                if(!bestsuccess) rcode = UHS_HOST_ERROR_DEVICE_NOT_SUPPORTED;
        }
        if(!rcode) {
                rcode = getConfDescr(ei.address, sizeof (USB_CONFIGURATION_DESCRIPTOR), bestconf, buf);
                if(rcode) {
                        HOST_DUBUG("Configuring error: %2.2x Can't get USB_INTERFACE_DESCRIPTOR\r\n", rcode);
                        rcode = UHS_HOST_ERROR_FailGetConfDescr;
                }
        }
        if(!rcode) {
                bestconf++;
                ei.currentconfig = bestconf;
                numinf = ucd->bNumInterfaces; // Does _not_ include alternates!
                HOST_DUBUG("CONFIGURATION: %i, bNumInterfaces %i, wTotalLength %i\r\n", bestconf, numinf, ucd->wTotalLength);
                if(!rcode) {
                        HOST_DUBUG("Best configuration is %i, enumerating interfaces.\r\n", bestconf);
                        uint16_t inf = 0;
                        uint8_t data[ei.bMaxPacketSize0];
                        UHS_EpInfo *pep;
                        pep = ctrlReqOpen(ei.address, mkSETUP_PKT8(UHS_bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, ei.currentconfig - 1, USB_DESCRIPTOR_CONFIGURATION, 0x0000U, ucd->wTotalLength), data);
                        if(!pep) {
                                rcode = UHS_HOST_ERROR_NULL_EPINFO;

                        } else {
                                uint16_t left;
                                uint16_t read;
                                uint8_t offset;
                                rcode = initDescrStream(&ei, ucd, pep, data, &left, &read, &offset);
                                if(rcode) {
                                        HOST_DUBUG("Configuring error: %2.2x Can't get USB_INTERFACE_DESCRIPTOR stream.\r\n", rcode);
                                } else {
                                        for(; (numinf) && (!rcode); inf++) {
                                                // iterate for each interface on this config
                                                rcode = getNextInterface(&ei, pep, data, &left, &read, &offset);
                                                if(rcode == UHS_HOST_ERROR_END_OF_STREAM) {
                                                        ctrlReqClose(pep, UHS_bmREQ_GET_DESCR, left, ei.bMaxPacketSize0, data);
                                                        rcode = 0;
                                                        break;
                                                }
                                                if(rcode) {
                                                        HOST_DUBUG("Configuring error: %2.2x Can't close USB_INTERFACE_DESCRIPTOR stream.\r\n", rcode);
                                                        continue;
                                                }

                                                if(enumerateInterface(&ei) == UHS_HOST_MAX_INTERFACE_DRIVERS) {
                                                        HOST_DUBUG("No interface driver for this interface.");
                                                } else {
                                                        HOST_DUBUG("Interface Configured\r\n");
                                                }
                                        }
                                }
                        }
                } else {
                        HOST_DUBUG("Configuring error: %2.2x Can't set USB_INTERFACE_CONFIG stream.\r\n", rcode);
                }
        }

        if(!rcode) {
                rcode = setConf(ei.address, bestconf);
                if(rcode) {
                        HOST_DUBUG("Configuring error: %2.2x Can't set Configuration.\r\n", rcode);
                        addrPool.FreeAddress(ei.address);
                } else {
                        for(devConfigIndex = 0; devConfigIndex < UHS_HOST_MAX_INTERFACE_DRIVERS; devConfigIndex++) {
                                HOST_DUBUG("Driver %i ", devConfigIndex);
                                if(!devConfig[devConfigIndex]) {
                                        HOST_DUBUG("no driver at this index.\r\n");
                                        continue; // no driver
                                }
                                HOST_DUBUG("@ %2.2x ", devConfig[devConfigIndex]->bAddress);
                                if(devConfig[devConfigIndex]->bAddress) {
                                        if(!devConfig[devConfigIndex]->bPollEnable) {
                                                HOST_DUBUG("Initialize\r\n");
                                                rcode = devConfig[devConfigIndex]->Finalize();
                                                rcode = devConfig[devConfigIndex]->Start();
                                                if(!rcode) {
                                                        HOST_DUBUG("Total endpoints = (%i)%i\r\n", p->epcount, devConfig[devConfigIndex]->bNumEP);
                                                } else {
                                                        break;
                                                }
                                        } else {
                                                HOST_DUBUG("Already initialized.\r\n");
                                                continue; // consumed
                                        }
                                } else {
                                        HOST_DUBUG("Skipped\r\n");
                                }
                        }
#if defined(UHS_HID_LOADED)
                        // Now do HID
#endif
                }
        } else {
                addrPool.FreeAddress(ei.address);
        }
        return rcode;
}

#endif /* UHS_HOST_INLINE_ENUMOPT_H */

