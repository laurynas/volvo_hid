//Lbus = LIN BUS from Car
//Vss = Ground
//Vbb = +12V

// MCP2004 LIN bus frame:
// ZERO_BYTE SYN_BYTE ID_BYTE DATA_BYTES.. CHECKSUM_BYTE

#include "Mouse.h"
#include "Keyboard.h"

// https://github.com/zapta/linbus/tree/master/analyzer/arduino
#include "lin_frame.h"

#define CS_PIN 15
#define RX_LED 17

#define HEARTBEAT_TIMEOUT 2000
#define KEY_TIMEOUT 100
#define MOUSE_BASE_SPEED 8
#define MOUSE_SPEEDUP 3

#define SYN_FIELD 0x55
#define SWM_ID 0x20

// Volvo V50 2007 SWM key codes
//
// BTN_NEXT       20 0 10 0 0 EF
// BTN_PREV       20 0 2 0 0 FD
// BTN_VOL_UP     20 0 0 1 0 FE
// BTN_VOL_DOWN   20 0 80 0 0 7F
// BTN_BACK       20 0 1 0 0 F7
// BTN_ENTER      20 0 8 0 0 FE
// BTN_UP         20 1 0 0 0 FE
// BTN_DOWN       20 2 0 0 0 FD
// BTN_LEFT       20 4 0 0 0 FB
// BTN_RIGHT      20 8 0 0 0 F7
// IGN_KEY_ON     50 E 0 F1

#define NAV_UP 1
#define NAV_DOWN 2
#define NAV_LEFT 4
#define NAV_RIGHT 8
#define NAV_BACK 1
#define NAV_ENTER 8

LinFrame frame = LinFrame();

unsigned long currentMillis, lastHeartbeat, lastBackDown, lastEnterDown, lastMouseDown;
int mouseSpeed = MOUSE_BASE_SPEED;

void setup() {
  pinMode(RX_LED, OUTPUT);
  
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  Serial.begin(9600);
  Serial1.begin(9600);

  Mouse.begin();
  Keyboard.begin();
}

void loop() {
  currentMillis = millis();
  
  if (Serial1.available())
    read_lin_bus();

  release_keys();
  check_ignition_key();
}

void read_lin_bus() {
  byte b = Serial1.read();
  int n = frame.num_bytes();

  if (b == SYN_FIELD && n > 2 && frame.get_byte(n - 1) == 0) {
    digitalWrite(RX_LED, LOW);
    
    frame.pop_byte();
    handle_swm_frame();
    frame.reset();
    
    digitalWrite(RX_LED, HIGH);
  } else if (n == LinFrame::kMaxBytes) {
    frame.reset();
  } else {
    frame.append_byte(b);
  }
}

void handle_swm_frame() {
  if (frame.get_byte(0) != SWM_ID)
    return;

  lastHeartbeat = currentMillis;
  
  // skip zero values 20 0 0 0 0 FF
  if (frame.get_byte(5) == 0xFF)
    return;

  if (!frame.isValid())
    return;

//  dump_frame();
  handle_joystic();
  handle_buttons();  
}

void handle_joystic() {
  switch (frame.get_byte(1)) {
    case NAV_UP:
      move_mouse(0, -1);
      break;
    case NAV_DOWN:
      move_mouse(0, 1);
      break;
    case NAV_LEFT:
      move_mouse(-1, 0);
      break;
    case NAV_RIGHT:
      move_mouse(1, 0);
      break;
  }  
}

void handle_buttons() {
  switch (frame.get_byte(2)) {
    case NAV_BACK:
      if (!lastBackDown) 
        Keyboard.press(KEY_ESC); 
       
      lastBackDown = currentMillis;
      break;
      
    case NAV_ENTER:
      if (!lastEnterDown)
        Mouse.press(); 
      
      lastEnterDown = currentMillis;
      break;
  }
}

void move_mouse(int dx, int dy) {
  Mouse.move(dx * mouseSpeed, dy * mouseSpeed, 0); 
  lastMouseDown = currentMillis;
  mouseSpeed += MOUSE_SPEEDUP;
}

void release_keys() {
  if (lastEnterDown && currentMillis - lastEnterDown > KEY_TIMEOUT) {
    Mouse.release();
    lastEnterDown = 0;
  }
  
  if (lastBackDown && currentMillis - lastBackDown > KEY_TIMEOUT) {
    Keyboard.release(KEY_ESC);
    lastBackDown = 0;
  }

  if (lastMouseDown && currentMillis - lastMouseDown > KEY_TIMEOUT) { 
    lastMouseDown = 0;
    mouseSpeed = MOUSE_BASE_SPEED;
  }
}

void check_ignition_key() {
   if (lastHeartbeat && currentMillis - lastHeartbeat > HEARTBEAT_TIMEOUT) {
     lastHeartbeat = 0;
     turn_off();
   }
}

void turn_off() {
  Serial.println("TURN OFF");
}

// -- debugging

void dump_frame() {
  for (int i = 0; i < frame.num_bytes(); i++) {
    Serial.print(frame.get_byte(i), HEX);
    Serial.print(" ");
  }
  Serial.println();
}

