#include "keyboard.h"
#include "keys.h"
#include "trackball.h"
#include "devterm.h"
#include "tickwaiter.h"

#include <USBComposite.h>

#define SER_NUM_STR "20230713"

USBHID HID;
DEVTERM dev_term;

const uint8_t reportDescription[] = { 
   HID_CONSUMER_REPORT_DESCRIPTOR(),
   HID_KEYBOARD_REPORT_DESCRIPTOR(),
   HID_JOYSTICK_REPORT_DESCRIPTOR(),
   HID_MOUSE_REPORT_DESCRIPTOR()
};

static const uint32_t LOOP_INTERVAL_MS = 0;
static TickWaiter<LOOP_INTERVAL_MS> waiter;

static const uint32_t INACTIVE_DELAY = 1000; //ms
unsigned long inactive_last_time;

HardwareTimer timer(1);
//HardwareTimer ctrl_timer(4);

void setup() {
  USBComposite.setManufacturerString("ClockworkPI");
  USBComposite.setProductString("uConsole");
  USBComposite.setSerialString(SER_NUM_STR);
  
  dev_term.Keyboard = new HIDKeyboard(HID);
  dev_term.Joystick = new HIDJoystick(HID);
  dev_term.Mouse    = new HIDMouse(HID);
  dev_term.Consumer = new HIDConsumer(HID);

  dev_term.Keyboard->setAdjustForHostCapsLock(false);

  dev_term.state = new State();

  dev_term.Keyboard_state.layer = 0;
  dev_term.Keyboard_state.prev_layer = 0;
  dev_term.Keyboard_state.fn_on = 0;
  dev_term.Keyboard_state.sf_on = 0;
  
  //dev_term.Keyboard_state.shift = 0;
  dev_term.Keyboard_state.backlight = 0;
  dev_term.Keyboard_state.lock = 0;
  
  dev_term.Keyboard_state.ctrl.lock = 0;
  dev_term.Keyboard_state.ctrl.time = 0;
  dev_term.Keyboard_state.ctrl.begin = 0;

  dev_term.Keyboard_state.shift.lock = 0;
  dev_term.Keyboard_state.shift.time = 0;
  dev_term.Keyboard_state.shift.begin = 0;

  dev_term.Keyboard_state.alt.lock = 0;
  dev_term.Keyboard_state.alt.time = 0;
  dev_term.Keyboard_state.alt.begin = 0;
      
  dev_term.Keyboard_state.fn.lock = 0;
  dev_term.Keyboard_state.fn.time = 0;
  dev_term.Keyboard_state.fn.begin = 0;  
  
  dev_term._Serial = new  USBCompositeSerial;
  
  HID.begin(*dev_term._Serial,reportDescription, sizeof(reportDescription));

  while(!USBComposite);//wait until usb port been plugged in to PC
  

  keyboard_init(&dev_term);
  keys_init(&dev_term);
  trackball_init(&dev_term);
  
  //dev_term._Serial->println("setup done");

  pinMode(PD2,INPUT);// switch 2 in back 

  timer.setPeriod(KEYBOARD_LED_PWM_PERIOD);
  timer.resume();

  /*
  ctrl_timer.setPeriod(20*1000);
  ctrl_timer.attachInterrupt(1,ctrl_timer_handler);
  ctrl_timer.refresh();
  ctrl_timer.resume();
  */
  pinMode(PA8,PWM);
  pwmWrite(PA8,0);

  
  delay(1000);

  inactive_last_time = millis();
}

#define LOCK_TIME 50

//DO NOT USE dev_term._Serial->println(""); in timer interrupt function,will block 
void check_keyboard_lock(KEYBOARD_LOCK*lock){
   if( lock->begin >0) {
    lock->time++;

    if( lock->time>=LOCK_TIME && lock->time<200){
      lock->lock = 1;
    }
    
    if( lock->time > 200){
     if(lock->begin != _FN_KEY) {
      	dev_term.Keyboard->release(lock->begin);
      }
      lock->time = 0;
      lock->lock = 0;
      lock->begin = 0;
    }
  } 
}

#define LOCK_TIME 50
void ctrl_timer_handler(void) {
  
  check_keyboard_lock(&dev_term.Keyboard_state.ctrl);
  check_keyboard_lock(&dev_term.Keyboard_state.shift);
  check_keyboard_lock(&dev_term.Keyboard_state.alt);
  check_keyboard_lock(&dev_term.Keyboard_state.fn);

}

void loop() {
  dev_term.delta = waiter.waitForNextTick();
  dev_term.state->tick(dev_term.delta);
  
  // trackball, keys above keyboard, keyboard itself
  bool active = false;
  active |= trackball_task(&dev_term);
  active |= keys_task(&dev_term);
  active |= keyboard_task(&dev_term);

  if (active) {
    inactive_last_time = millis();
  } else {
    unsigned long current_time = millis();
    if (current_time - inactive_last_time > INACTIVE_DELAY) {
      matrix_to_interrupt();
      keys_to_interrupt();
      __asm volatile ("wfi");
      matrix_to_normal();
      keys_to_normal();
      trackball_init(&dev_term);
      inactive_last_time = millis();
    }
  }
  

}
