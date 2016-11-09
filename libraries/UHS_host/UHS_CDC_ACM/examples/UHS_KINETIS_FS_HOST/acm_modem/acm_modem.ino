// Load the USB Host System core
#define LOAD_USB_HOST_SYSTEM
// Load the Kinetis core
#define LOAD_UHS_KINETIS_FS_HOST
// Use USB hub
#define LOAD_UHS_HUB

// Patch printf so we can use it.
#define LOAD_UHS_PRINTF_HELPER
//#define DEBUG_PRINTF_EXTRA_HUGE 1
//#define DEBUG_PRINTF_EXTRA_HUGE_UHS_HOST 1
//#define DEBUG_PRINTF_EXTRA_HUGE_USB_HUB 1
//#define DEBUG_PRINTF_EXTRA_HUGE_USB_HOST_SHIELD 1
//#define DEBUG_PRINTF_EXTRA_HUGE_ACM_HOST 1
//#define ENABLE_UHS_DEBUGGING 1
//#define UHS_DEBUG_USB_ADDRESS 1

// Redirect debugging and printf
#define USB_HOST_SERIAL Serial1

// Invoke a large memory model - Andrew calls it thus
#define UHS_DEVICE_WINDOWS_USB_SPEC_VIOLATION_DESCRIPTOR_DEVICE 1

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

//#define LOAD_UHS_ENUMERATION_OPT "UHS_host_INLINE_enumopt.h"
#define PROG_NAME "Wk1611081620 FS modem\r\n"

#include <Arduino.h>
#ifdef true
#undef true
#endif
#ifdef false
#undef false
#endif

#include <UHS_host.h>

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
        // Set DTR = 1 RTS = 1
        rcode = SetControlLineState(3);

        if(rcode) {
                printf_P(PSTR("SetControlLineState %x\r\n"), rcode);
                return rcode;
        }

        UHS_CDC_LINE_CODING lc;
        lc.dwDTERate = 115200;
        lc.bCharFormat = 0;
        lc.bParityType = 0;
        lc.bDataBits = 8;

        rcode = SetLineCoding(&lc);

        if(rcode) {
                printf_P(PSTR("SetLineCoding %x\r\n"), rcode);
                return rcode;
        }
        // Tell the user that the device has connected
        printf("\r\nConnected.");
        return 0;
}


UHS_KINETIS_FS_HOST *KINETIS_Usb;
UHS_USBHub *hub_KINETIS1;
UHS_USBHub *hub_KINETIS2;
MY_ACM *Acm;

void setup() {
        // This is so you can be ensured the dev board has power,
        // since teensy lacks a power indicator LED.
        // It also flashes at each stage.
        // If the code wedges at any point, you'll see the LED stuck on.
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWriteFast(LED_BUILTIN, HIGH);
        delay(250);
        digitalWriteFast(LED_BUILTIN, LOW);
        delay(250);
        digitalWriteFast(LED_BUILTIN, HIGH);
        delay(250);

        // USB data switcher, PC -> device.
        pinMode(5,OUTPUT),
        digitalWriteFast(5, HIGH);

        KINETIS_Usb = new UHS_KINETIS_FS_HOST();
        hub_KINETIS1 = new UHS_USBHub(KINETIS_Usb);
        digitalWriteFast(LED_BUILTIN, LOW);
        delay(250);
        digitalWriteFast(LED_BUILTIN, HIGH);
        delay(250);
        hub_KINETIS2 = new UHS_USBHub(KINETIS_Usb);
        Acm = new MY_ACM(KINETIS_Usb);
        digitalWriteFast(LED_BUILTIN, LOW);
        delay(250);
        digitalWriteFast(LED_BUILTIN, HIGH);
        delay(250);
        while(!USB_HOST_SERIAL);
        digitalWriteFast(LED_BUILTIN, LOW);
        delay(250);
        USB_HOST_SERIAL.begin(115200);

        printf("\r\n" PROG_NAME );

        while(KINETIS_Usb->Init(1000) != 0);
         printf("\r\n't' for tty - T36Host-to-Photon. Then {v} to start repitive test\r\n");
}
typedef enum {
        USBS_INIT=0,
        USBS_TTY, // Starting serial processing, waiting for input from serial TTY or USB
                             // if USBserial string {c..}, transition to USBS_TTY_TX_V
                             // if ESC change  to USBS_INIT
} usb_state_t;
typedef enum {
         USBM_TTY_RX_V,// Monitor USBserial and output to screen.
                                        // for {v, ..} change state to USBS_TTY_TX_V
                                        // if ESC change  to USBS_INIT
         USBM_INIT= USBM_TTY_RX_V,
         USBM_TTY_TX_V,//Tx USBserial  {v..} and transition to USBS_TTY_RX_V
} usb_msg_state_t;

usb_state_t  usbfs_state_e   = USBS_INIT;
usb_state_t  usbfs_state_old_e = USBS_INIT;
usb_msg_state_t usbm_state_e = USBM_INIT;

#define PH_VER_REQ_SZ 3
uint8_t  ph_ver_req[PH_VER_REQ_SZ+1] = "{v}";

#define ASCII_ESC 27

//****************************************************************
 void usbfs_state_USBS_TTY() {
     //Event move to USBS_TTY
         usbfs_state_e=USBS_TTY;
         usbm_state_e=USBM_INIT;
 }

 //****************************************************************
uint8_t in_keyboard() {
     uint8_t in_data =  0;
     uint8_t rcode =0;

      if(USB_HOST_SERIAL.available()) {
         digitalWriteFast(LED_BUILTIN, HIGH);
         in_data = USB_HOST_SERIAL.read();

         if (ASCII_ESC == in_data) {
              usbfs_state_e=USBS_INIT;
         } else {
                /* send to client */
                 rcode = Acm->Write(1, &in_data);
                  if(rcode) {
                          in_data = ASCII_ESC;
                        printf("\r\nError %i on write\r\n", rcode);
                        return in_data;
                   }
          }
          digitalWriteFast(LED_BUILTIN, LOW);
      }
     return in_data;
}
//****************************************************************
 uint8_t  usbm_tty_rx_check() {
        /* read from client
         * buffer size must be greater or equal to max.packet size
         * it is set to the largest possible maximum packet size here.
         * It must not be set less than 3.
         */
#define USBPKT_MAX_SZ 64
        uint8_t buf[USBPKT_MAX_SZ ];
        uint16_t rcvd = USBPKT_MAX_SZ ;
         uint8_t rcode=0;
         
         rcode = Acm->Read(&rcvd, buf);
         if(rcode && (rcode != UHS_HOST_ERROR_NAK)) {
                printf("\r\nError %i on read\r\n", rcode);
                return rcode ;
         }
         if(rcvd) {
             // More than zero bytes received, display the text.
             for(uint16_t i = 0; i < rcvd; i++) {
                    putc((char)buf[i], stdout);
                    if ( '}' == buf[i] ) {usbm_state_e = USBM_TTY_TX_V; }
             }
             fflush(stdout);
         }
         return rcode;
 }
//****************************************************************
void loop() {
  uint8_t in_d=0;

  if (usbfs_state_old_e!=usbfs_state_e) {
     printf("StateChange To=%d, Frm=%d\r\n",usbfs_state_e,usbfs_state_old_e);
     usbfs_state_old_e=usbfs_state_e;
  }

  if (USBS_INIT == usbfs_state_e) {
         if (USB_HOST_SERIAL.available() > 0) {
                  in_d =  in_keyboard();
                 if (in_d == 's') {
                          printf("USB0_INTEN: 0x%x ", USB0_INTEN);
                          printf("USB0_CTL: 0x%x\r\n", USB0_CTL);
                    //} else if(d=='p') {
                    ///KINETIS_Usb.poopOutStatus();
                  } else if(in_d == 't') {

                        usbfs_state_USBS_TTY();
                        printf("TTY mode <ESC> to exit\r\n");
                  }
        }
   }else  if(Acm->isReady() )   {
         uint8_t rcode;

         /* check for any user  ESC */
         if ( ASCII_ESC == (in_d =  in_keyboard()) ) {return;}

         digitalWriteFast(LED_BUILTIN, HIGH);
         switch (usbm_state_e) {
                  case  USBM_TTY_RX_V:
                       usbm_tty_rx_check();
                      break;

                  case USBM_TTY_TX_V: //Tx USBserial  {v..} and transition to USBS_TTY_RX_V
                      rcode = Acm->Write(PH_VER_REQ_SZ, &ph_ver_req[0]);
                       if(rcode) {
                                printf("\r\nTX_V:Error %i on write\r\n", rcode);
                                //return in_data;
                           } else {
                                 usbm_state_e = USBM_TTY_RX_V;
                           }
                      break;
                  default: break;
              }
                digitalWriteFast(LED_BUILTIN, LOW);

    }
 }
//End of acm_modem.ino