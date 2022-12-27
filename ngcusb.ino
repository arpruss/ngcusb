#include <libmaple/iwdg.h>
#include "GameControllers.h"
#include <USBComposite.h>

// left:XBox, right:padjoy, down:wasd13, up:arrows,space,backspace,+,-

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
// GameCube 6--3.3V

const uint32_t watchdogSeconds = 3;

#define PRODUCT_ID_JOYSTICK 0x7E47

enum { MODE_WASD13 = 0, MODE_XBOX, MODE_PADJOY, MODE_ARROWS };
enum { USB_JK, USB_XBOX };
const uint8_t usbMode[] = { USB_JK, USB_XBOX, USB_JK, USB_JK };
uint32_t mode = MODE_WASD13;

GameCubeController gc(PA6);
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
const uint8_t wasd13[16] = { 'e', 'q', 'z', 'c', '3'/*start*/,0,0,0, 'a', 'd', 's', 'w', '1', 0,0  };
const uint8_t arrows[16] = { ' ', KEY_BACKSPACE, '[', ']', '='/*start*/,0,0,0, KEY_LEFT_ARROW, KEY_RIGHT_ARROW, KEY_DOWN_ARROW, KEY_UP_ARROW, '-', 0,0  };
const uint8_t joy[16] = { 1, 2, 3, 4, 5, 0,0,0, 6,7,8,9, 10,11,12,0 };

const uint8_t xbuttons[16] = { XBOX_A, XBOX_B, XBOX_X, XBOX_Y, XBOX_START, 0,0,0, XBOX_DLEFT, XBOX_DRIGHT, XBOX_DDOWN, XBOX_DUP, XBOX_RSHOULDER, XBOX_R3, XBOX_L3 }; 

void startUSBMode() {
  if (usbMode[mode] == USB_XBOX) {
    USBComposite.setProductString("NGC pad to xbox");
    XBox360.begin();
    XBox360.setManualReportMode(true);
    delay(100);
  }
  else if (usbMode[mode] == USB_JK) {
    USBComposite.setProductString("NGC pad to usb");
    USBComposite.setProductId(PRODUCT_ID_JOYSTICK);  
    HID.begin(HID_KEYBOARD_JOYSTICK);
    joystick.setManualReportMode(true);
//    joystick.sendReport();
    keyboard.begin();
    delay(100);
  }
}

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
  else if (mode == MODE_WASD13 || mode == MODE_ARROWS) {
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

void setup() {
  iwdg_init(IWDG_PRE_256, watchdogSeconds*156);
  pinMode(LED,OUTPUT);
  digitalWrite(LED,1);
  EEPROM8_init();
  int v = EEPROM8_getValue(0);
  if (v < 0)
    mode = MODE_PADJOY;
  else
    mode = v;
  startUSBMode();
  while(!USBComposite);
  gc.begin();
}

static inline int16_t range10u16s(uint16_t x) {
  return (((int32_t)(uint32_t)x - 512) * 32767 + 255) / 512;
}

void emit(GameControllerData_t* d) {
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
    else if (mode == MODE_WASD13 || mode == MODE_ARROWS) {
      const uint8_t* keyMap = mode == MODE_WASD13 ? wasd13 : arrows;
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
      joystick.sendReport();
    }
}

void loop() {
  iwdg_feed();
  
  GameControllerData_t data;
  if (gc.read(&data)) {
    digitalWrite(LED,0);
    uint32_t newMode = mode;
    if ((data.buttons & (gcmaskStart | gcmaskZ)) == (gcmaskStart | gcmaskZ)) {
      if ((data.buttons & gcmaskDLeft)) {
        newMode = MODE_XBOX;
      }
      else if ((data.buttons & gcmaskDRight)) {
        newMode = MODE_PADJOY;
      }
      else if ((data.buttons & gcmaskDDown)) {
        newMode = MODE_WASD13;
      }
      else if ((data.buttons & gcmaskDUp)) {
        newMode = MODE_ARROWS;
      }
      if (newMode != mode) {
        endMode();
        if (usbMode[newMode] != usbMode[mode]) {
          endUSBMode();
          mode = newMode;
          startUSBMode();
          while(!USBComposite);
        }
        else {
          mode = newMode;
        }
        EEPROM8_storeValue(0, mode);
        return;
      }
    }
    emit(&data);
  }
  else {
    digitalWrite(LED,1);
  }
}

