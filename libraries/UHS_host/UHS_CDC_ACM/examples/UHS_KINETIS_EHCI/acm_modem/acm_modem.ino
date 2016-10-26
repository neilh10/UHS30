// Load the USB Host System core
#define LOAD_USB_HOST_SYSTEM
// Load the Kinetis core
#define LOAD_UHS_KINETIS_EHCI
// Use USB hub
#define LOAD_UHS_HUB

// Patch printf so we can use it.
#define LOAD_UHS_PRINTF_HELPER
//#define DEBUG_PRINTF_EXTRA_HUGE 1
//#define DEBUG_PRINTF_EXTRA_HUGE_UHS_HOST 1
//#define DEBUG_PRINTF_EXTRA_HUGE_USB_HOST_SHIELD 1

// Redirect debugging and printf
#define USB_HOST_SERIAL Serial1

// These all get combined under UHS_CDC_ACM multiplexer.
// Each should only add a trivial amount of code.
// XR21B1411 can run in a pure CDC-ACM mode, as can PROLIFIC.
// FTDI has a large code and data footprint. Avoid this chip if you can.
#define LOAD_UHS_CDC_ACM
#define LOAD_UHS_CDC_ACM_XR21B1411
// This needs testing.
#define LOAD_UHS_CDC_ACM_PROLIFIC
// This needs testing.
#define LOAD_UHS_CDC_ACM_FTDI

#include <Arduino.h>
#ifdef true
#undef true
#endif
#ifdef false
#undef false
#endif

#include <UHS_host.h>
UHS_KINETIS_EHCI KINETIS_EHCI_Usb;

UHS_USBHub hub_KINETIS_EHCI(&KINETIS_EHCI_Usb);

class MY_ACM : public UHS_CDC_ACM {
public:

        MY_ACM(UHS_USB_HOST_BASE *p) : UHS_CDC_ACM(p) {
        };
        void OnRelease(void);
        uint8_t OnStart(void);
};

void MY_ACM::OnRelease(void) {
        // Tell the user that the device has disconnected
        if(bAddress) printf("\r\n\r\nDisconnected.\r\n\r\n");
}

uint8_t MY_ACM::OnStart(void) {
        uint8_t rcode;
        // Set DTR = 1 RTS=1
        rcode = SetControlLineState(3);

        if(rcode) {
                printf_P(PSTR("SetControlLineState %x\r\n"), rcode);
                return rcode;
        }

        UHS_CDC_LINE_CODING lc;
        lc.dwDTERate = 9600;
        lc.bCharFormat = 0;
        lc.bParityType = 0;
        lc.bDataBits = 8;

        rcode = SetLineCoding(&lc);

        if(rcode) {
                printf_P(PSTR("SetLineCoding %x\r\n"), rcode);
                return rcode;
        }
        // Tell the user that the device has connected
        printf("\r\n\r\nConnected.\r\n\r\n");
        return 0;
}

MY_ACM Acm(&KINETIS_EHCI_Usb);

void setup() {
        while(!USB_HOST_SERIAL);
        USB_HOST_SERIAL.begin(115200);

        printf_P(PSTR("\r\n\  EHCI CDC-ACM modem 161025_1212\r\n"));
        while(KINETIS_EHCI_Usb.Init(1000) != 0);
        printf_P(PSTR("Waiting for Connection...\r\n"));
}

void loop() {
uint8_t d;

       if (USB_HOST_SERIAL.available() > 0) {

                d = USB_HOST_SERIAL.read();
                if(d=='s') {
                        printf("USB0_INTEN: 0x%x ", USB0_INTEN);
                        printf("USB0_CTL: 0x%x\r\n", USB0_CTL);
                } else if(d=='p') {
                      KINETIS_EHCI_Usb.poopOutStatus();
                } else if(d=='0') {
                      KINETIS_EHCI_Usb.vbusPower(vbus_off);
                          printf("  PowerOff\r\n");
                } else if(d=='1') {
                      KINETIS_EHCI_Usb.vbusPower(vbus_on);
                      printf("  PowerOn\r\n");
                }
        }
#if 0
        if(Acm.isReady()) {
                uint8_t rcode;

                /* read the keyboard */
                if(USB_HOST_SERIAL.available()) {
                        uint8_t data = USB_HOST_SERIAL.read();
                        /* send to client */
                        rcode = Acm.Write(1, &data);
                        if(rcode) {
                                printf_P(PSTR("\r\nError %i on write\r\n"), rcode);
                                return;
                        }
                }


                /* read from client
                 * buffer size must be greater or equal to max.packet size
                 * it is set to the largest possible maximum packet size here.
                 * It must not be set less than 3.
                 */
                uint8_t buf[64];
                uint16_t rcvd = 64;
                rcode = Acm.Read(&rcvd, buf);
                if(rcode && rcode != hrNAK) {
                        printf_P(PSTR("\r\nError %i on read\r\n"), rcode);
                        return;
                }

                if(rcvd) {
                        // More than zero bytes received, display the text.
                        for(uint16_t i = 0; i < rcvd; i++) {
                                putc((char)buf[i], stdout);
                                //printf_P(PSTR("%c"),(char)buf[i]);
                        }
                        fflush(stdout);
                }
        }
#endif
}
