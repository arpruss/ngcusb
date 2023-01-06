#include <libmaple/iwdg.h>
#include "GameControllers.h"
#include <USBComposite.h>

//timing: mat: 401us
//usb 24ms
//usb at 4: 
//xbox 8ms

//#define TEST

#define MAX_CONTROLLERS 1

#ifdef TEST
#define TEST1 PB12
#define TEST2 PB13
#define TEST1WRITE(x) digitalWrite(TEST1,(x))
#define TEST2WRITE(x) digitalWrite(TEST2,(x))
#else
#define TEST1WRITE(x) 
#define TEST2WRITE(x)
#endif

// left:XBox, right:padjoy, down:power pad right, up:power pad left

#define LED PC13
// Facing GameCube socket (as on console), flat on top:
//    123
//    ===
//    456

// Connections for GameCube adapter:
// GameCube 2--PA6
// GameCube 2--1Kohm--3.3V
// GameCube 3--GND
// GameCube 4--GND

const uint32_t watchdogSeconds = 4;

#define PRODUCT_ID_JOYSTICK 0x7E47
#define PRODUCT_ID_KEYBOARD 0x7E48

const uint32_t maximumPollingRateMicroseconds=2500;
uint32_t lastPollTime = 0;
enum { MODE_POWERPADRIGHT = 0, MODE_XBOX, MODE_PADJOY, MODE_POWERPADLEFT };
enum { USB_J, USB_K, USB_XBOX };
const uint8_t usbMode[] = { USB_K, USB_XBOX, USB_J, USB_K };
uint32_t mode = MODE_POWERPADRIGHT;

GameCubeController gc1(PA6);
#if MAX_CONTROLLERS == 1
GameCubeController* controllers[MAX_CONTROLLERS] = { &gc1 };
#else
GameCubeController gc2(PA7);
GameCubeController* controllers[MAX_CONTROLLERS] = { &gc1, &gc2 };
#endif
USBXBox360 XBox360;
USBHID HID;
HIDKeyboard keyboard(HID);
HIDJoystick joystick(HID);
bool pressed[16];
/* 
 *  const uint16_t gcmaskA = 0x01;
const uint16_t gcmaskB = 0x02;
const uint16_t gcmaskX = 0x04;
const uint16_t gcmaskY = 0x08;
const uint16_t gcmaskDLeft = 0x100;
const uint16_t gcmaskDRight = 0x200;
const uint16_t gcmaskDDown = 0x400;
const uint16_t gcmaskDUp = 0x800;
const uint16_t gcmaskZ = 0x1000;
const uint16_t gcmaskR = 0x2000;
const uint16_t gcmaskL = 0x4000;

 */

/*
 * Pad arrangement:
 * 12 -- 04
 * 01 11 00
 * 08 -- 09
 * -- 10 --
 * 
 * left pad:
 * z  -- q
 * z  a  q
 * x  -- w
 * -- d  --
 * 
 * right pad:
 * tb -- sp
 * r  f  v
 * e  -- c
 * -- s  --
 */ 

#define TAB KEY_TAB 
#define EN KEY_RETURN
#define BS KEY_BACKSPACE

                       //     00   01   02   03   04   05   06   07   08   09   10   11   12   13   14   15
const uint8_t ppRight[16] = { 'v', 'r', 0,   0,   ']', 0,   0,   0,   'e', 'c', 's', 'f', '[', 0,   0,   0 };
const uint8_t ppLeft[16]  = { 'q', 'z', 0,   0,   '=',  0,   0,   0,   'x', 'w', 'd', 'a', '-',  0,   0,   0 };
//const uint8_t arrows[16] =  { ' ', BS, '[',  ']', '=', 0,   0,   0, KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_DOWN_ARROW, KEY_UP_ARROW, '-', 0,0  };
const uint8_t joy[16] = { 1, 2, 3, 4, 5, 0,0,0, 6,7,8,9, 10,11,12,0 };
const uint8_t xbuttons[16] = { XBOX_A, XBOX_B, XBOX_X, XBOX_Y, XBOX_START, 0,0,0, XBOX_DLEFT, XBOX_DRIGHT, XBOX_DDOWN, XBOX_DUP, XBOX_RSHOULDER, XBOX_R3, XBOX_L3 }; 

void startUSBMode() {
  if (usbMode[mode] == USB_XBOX) {
    USBComposite.setProductString("NGC pad to xbox");
    XBox360.begin();
    XBox360.setManualReportMode(true);
    delay(100);
  }
  else if (usbMode[mode] == USB_J) {
    USBComposite.setProductString("NGC pad to joystick");
    USBComposite.setProductId(PRODUCT_ID_JOYSTICK);  
    HID.setTXInterval(4);
    HID.begin(HID_JOYSTICK);
    joystick.setManualReportMode(true);
    keyboard.begin();
    delay(100);
  }
  else if (usbMode[mode] == USB_K) {
    USBComposite.setProductString("NGC pad to keyboard");
    USBComposite.setProductId(PRODUCT_ID_KEYBOARD);  
    HID.setTXInterval(4);
    HID.begin(HID_KEYBOARD);
    joystick.setManualReportMode(true);
    keyboard.begin();
    delay(100);
  }
}

/*
void endUSBMode() {
  if (usbMode[mode] == USB_XBOX) {
    XBox360.end();
  }
  else if (usbMode[mode] == USB_JK) {
    HID.end();
  }
  delay(100);
}

void endMode() {
  if (mode == MODE_XBOX) {
    XBox360.buttons(0);
    XBox360.send();
  }
  else if (mode == MODE_POWERPADRIGHT || mode == MODE_POWERPADLEFT) {
    keyboard.releaseAll();
    for (uint32_t i = 0 ; i < 16 ; i++)
      pressed[i] = false;
  }
  else if (mode == MODE_PADJOY) {
    joystick.buttons(0);
    joystick.X(512);
    joystick.Y(512);
    joystick.Xrotate(512);
    joystick.Yrotate(512);
    joystick.sliderLeft(0);
    joystick.sliderRight(0);
    joystick.sendReport();
  }
}
*/

void setup() {
#ifdef TEST
  pinMode(TEST1,OUTPUT);
  pinMode(TEST2,OUTPUT);
  digitalWrite(TEST1,0);
  digitalWrite(TEST2,0);
#endif  
  TEST1WRITE(0);
  TEST2WRITE(1);
  
  iwdg_init(IWDG_PRE_256, watchdogSeconds*156);
  pinMode(LED,OUTPUT);
  digitalWrite(LED,1);
  EEPROM8_init();
  int v = EEPROM8_getValue(0);
  if (v < 0)
    mode = MODE_PADJOY;
  else
    mode = v;
  mode = MODE_PADJOY;
  startUSBMode();
  while(!USBComposite);
  for (uint8_t i = 0 ; i < MAX_CONTROLLERS ; i++) {
    controllers[i]->setDPadToJoystick(false);
    controllers[i]->begin();  
  }
/*  uint32 id=0;
  
  controllers[0]->begin();  
  
  controllers[0]->sendBits(0b000000001, 9);
  if (controllers[0]->receiveBits(&id, 24)) {
    keyboard.println((id>>16) & 0xFF, HEX);
    keyboard.println((id>>8) & 0xFF, HEX);
    keyboard.println((id) & 0xFF, HEX);
  }
  else {
    keyboard.println("bye");
  }
  for (uint8_t i = 0 ; i < MAX_CONTROLLERS ; i++) {
    controllers[i]->setDPadToJoystick(false);
    controllers[i]->begin();  
  }
/*
  while(0) {
  controllers[0]->sendBits(0b0100000000000011000000001l, 25);
  uint8_t d[8];
  for (int i=0;i<8;i++) d[i]=0xFF;
  bool success = controllers[0]->receiveBits(d, 64);
  if (success) {
    keyboard.println("ok");
  }
    for (uint8_t i=0; i<8; i++)
      keyboard.println(d[i],HEX);
  }
*/
}


static inline int16_t range10u16s(uint16_t x) {
  return (((int32_t)(uint32_t)x - 512) * 32767 + 255) / 512;
}

void emit(GameControllerData_t* d, int controllerNumber) {
    if (mode == MODE_XBOX) {
      XBox360.buttons(0);
      uint16_t mask = 1;
      for (uint32_t i = 0 ; i < 16 ; i++,mask<<=1) {
        XBox360.button(xbuttons[i], 0 != (d->buttons & mask));
      }
      XBox360.X(range10u16s(d->joystickX));
      XBox360.Y(-range10u16s(d->joystickY));
      XBox360.XRight(range10u16s(d->cX));
      XBox360.YRight(-range10u16s(d->cY));
      XBox360.sliderLeft(d->shoulderLeft / 4);
      XBox360.sliderRight(d->shoulderRight / 4);
      
      XBox360.send();
    }
    else if (mode == MODE_POWERPADRIGHT || mode == MODE_POWERPADLEFT) {
      const uint8_t* keyMap = ((mode == MODE_POWERPADRIGHT) ^ (0 != controllerNumber)) ? ppRight : ppLeft;
      //joystick.sendReport(); // just in case
      uint32_t mask = 1;
      for (uint32_t i = 0 ; i < 16 ; i++,mask<<=1) {
        uint16_t k = keyMap[i];
        if (!k)
          continue;
        if ((d->buttons & mask)) {
          if (!pressed[i]) {
            keyboard.press(k);
            pressed[i] = true;
          }
        }
        else {
          if (pressed[i]) {
            keyboard.release(k);
            pressed[i] = false;
          }
        }
      }
    }
    else if (mode == MODE_PADJOY) {
      uint32_t mask = 1;
      for (uint32_t i = 0 ; i < 16 ; i++,mask<<=1) {
        uint16_t b = joy[i];
        if (b)
          joystick.button(b, 0 != (d->buttons & mask));
      }
      joystick.X(d->joystickX);
      joystick.Y(d->joystickY);
      joystick.Xrotate(d->cX);
      joystick.Yrotate(d->cY);        
      joystick.sliderLeft(d->shoulderLeft);
      joystick.sliderRight(d->shoulderRight);
      TEST2WRITE(1);
      joystick.sendReport();
      TEST2WRITE(0);
    }
}

void indicate(int n) {
  digitalWrite(LED,1);
  delay(200);
  for (int i=0;i<n;i++) {
    digitalWrite(LED,0);
    delay(200);
    digitalWrite(LED,1);
    delay(200);
  }
  delay(200);
}

void loop() {
  iwdg_feed();

#if MAX_CONTROLLERS == 1
  while((uint32_t)(micros()-lastPollTime) < maximumPollingRateMicroseconds) ;
#endif  
  
  GameControllerData_t data;
  bool someSuccess = false;
  for (uint8_t controller = 0 ; controller < MAX_CONTROLLERS ; controller++) {
    if (controllers[controller]->read(&data)) {
#if MAX_CONTROLLERS == 1      
      lastPollTime = micros();
#endif      
      someSuccess = true;
      uint32_t newMode = mode;
      if ((data.buttons & (gcmaskStart | gcmaskZ)) == (gcmaskStart | gcmaskZ)) {
        if ((data.buttons & gcmaskDLeft)) {
          newMode = MODE_XBOX;
        }
        else if ((data.buttons & gcmaskDRight)) {
          newMode = MODE_PADJOY;
        }
        else if ((data.buttons & gcmaskDDown)) {
          newMode = MODE_POWERPADRIGHT;
        }
        else if ((data.buttons & gcmaskDUp)) {
          newMode = MODE_POWERPADLEFT;
        }
        if (newMode != mode) {
          digitalWrite(LED,1);
          EEPROM8_storeValue(0, newMode);
          if (usbMode[newMode] != usbMode[mode]) {
            delay(100);
            nvic_sys_reset();
          }
          else {
            if (usbMode[newMode] == USB_K) {
              keyboard.releaseAll();
              for (uint32_t i=0; i<16; i++)
                pressed[i] = false;
            }
            mode = newMode;
            indicate(1+mode);
          }
        }
      }
      emit(&data, controller);
    }
  }
  digitalWrite(LED,! someSuccess);
  if (! someSuccess) {
//    keyboard.print("!");
    delay(1);
  }
}


