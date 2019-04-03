#include <SPI.h> 
#include <nRF24L01.h> 
#include <RF24.h> 
#include <Adafruit_NeoPixel.h>
#include "printf.h"

// This pin is used to set the nRF24 to standby (0) or active mode (1)
#define pinCE 9

// This pin is used to tell the nRF24 whether the SPI communication
// is a command or message to send out
#define pinCSN 10

// This is the pin the WS2812B data line is connected to
#define pinLED 6

//This is the number of WS2812B LEDs in the strip
#define NUM_LEDS 3

// Node's light index, i. e. which light this node controls
// Available Lights:
// 0: All Lights
// 1: Wall Light
// 2: Desk Light
// 3: Mirror Light
// 4: Bed Light
// 5: Couch Light
// 6: PC Lights

int myIndex = 2;  // Desk light

// Global vars
RF24 wirelessSPI(pinCE, pinCSN);          // Create the nRF module
const uint64_t pAddress = 0xB00B1E5000LL; // Radio pipe address
char buffer [13];                         // Buffer for received frame
int flag = 0;                             // Flag that indicates when buffer updated
int power = 1;                            // Light's power state: 1 = on; 0 = off
int brightness = 32;                      // Light's brightness [0-255]
uint32_t prevColor;                           // Temporary storage for color while power is off

// Initialize the NeoPixel strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, pinLED, NEO_GRB + NEO_KHZ800);

void setup() { 
  Serial.begin(115200);
  wirelessSPI.begin();                      // Start the nRF24 module
  wirelessSPI.setAutoAck(1);                // Ensure autoACK is enabled so rec sends ack packet to let you know it got the transmit packet payload
  wirelessSPI.enableAckPayload();           // allows you to include payload on ack packet
  wirelessSPI.maskIRQ(1,1,0);               // mask all IRQ triggers except for receive (1 is mask, 0 is no mask)
  wirelessSPI.setPALevel(RF24_PA_LOW);      // Set power level to low, won't work well at higher levels (interfere with receiver)
  wirelessSPI.setDataRate(RF24_250KBPS);
  wirelessSPI.openReadingPipe(1,pAddress);  // open pipe o for recieving meassages with pipe address
  wirelessSPI.startListening();             // Start listening for messages
  printf_begin();
  wirelessSPI.printDetails();
  strip.begin();
  strip.setBrightness(32);
  strip.show();
  for(int p = 0; p < NUM_LEDS; p++) {
    strip.setPixelColor(p, 255, 166, 87);
  }
  strip.show();
  
  attachInterrupt(digitalPinToInterrupt(2), interruptFunction, FALLING);  // Allow for interrupts on receive at Pin 2
}

void loop() {
  // If the interrupt sets this flag, read and parse the buffer
  while (flag == 1) {
    bufferUpdated();
  } 
//  if(!power) {
//    strip.show();  
//  }
}

int getBrightnessValue(char *buff) {
  char val [3];
  for (int i=3; i < 6; i++) {
    val[i - 3] = buff[i];
  }
  return atoi(val);
}

void setColor(char *buff) {
  char red [4];
  char green [4];
  char blue [4];
  for (int i=3; i < 12; i++) {
    if (i < 6) {
      red[i - 3] = buff[i];
    }
    else if (i >= 6 && i < 9) {
      green[i - 6] = buff[i];
    }
    else if (i >= 9 && i < 12) {
      blue[i - 9] = buff[i];
    }
  }
  int redi = atoi(red);
  int greeni = atoi(green);
  int bluei = atoi(blue);
  
  for(int p = 0; p < NUM_LEDS; p++) {
    strip.setPixelColor(p, redi, greeni, bluei);
    strip.show();
  }
}

void setBrightnessLevel(int val = brightness) {
  if (val >= 0 && val <= 255) {
    strip.setBrightness(val);
  }
  else if (val < 0) {
    brightness = 0;
    setBrightnessLevel();
  }
  else if (val > 255) {
    brightness = 255;
    setBrightnessLevel();
  }
  strip.show();
}

void setPower(char state) {
  Serial.println(state);
  if (state == '0') {
    power = 0;
    prevColor = strip.getPixelColor(2);
    Serial.println(prevColor);
    for(int p = 0; p < NUM_LEDS; p++) {
      strip.setPixelColor(p, 0, 0, 0);
    }
    strip.show();
  }
  else if (state == '1') {
    power = 1;
    if(prevColor) {
      for(int p = 0; p < NUM_LEDS; p++) {
        strip.setPixelColor(p, prevColor);
      }
    }
    else {
      for(int p = 0; p < NUM_LEDS; p++) {
        strip.setPixelColor(p, 255, 166, 87);
      }
    }
    
    strip.show();
  }
}

// Function runs when IRQ received
void interruptFunction() {
  while (wirelessSPI.available()) {        // While there is data ready
        wirelessSPI.read( &buffer, 13 );   // Get the payload
  }
  flag = 1;
  Serial.println(buffer);
  detachInterrupt(digitalPinToInterrupt(2));
}

// Runs to read in the buffer and calls appropriate functions
void bufferUpdated() {
  if (buffer[0] == 'X') {                             // Ensure the frame is valid, all valid frames begin with 'X'
    //sets the light index
    int index = buffer[1];
    if (index == myIndex || index == '0') {           // Checks if the received command is for this node's light(s)
      switch (buffer[2]) {                            // Choose the specified operation
        case 'P'  :
          setPower(buffer[3]);                  // set power to 1 if on, 0 if off 
           break;
        case 'B'  :
          brightness = getBrightnessValue(buffer);    // compose brightness value
          setBrightnessLevel();
           break;
        case 'I'  :
          brightness = brightness + 17;
          setBrightnessLevel();
           break;
        case 'D'  :
          brightness = brightness - 17;
          setBrightnessLevel();
           break;
        case 'C'  :
          setColor(buffer);
           break;
      }
    }
  }
  // Write the buffer for debug purposes
  // Serial.println((char*)buffer);
  // Resets flag and restores the interrupt
  flag = 0;
  delay(100);
  attachInterrupt(digitalPinToInterrupt(2), interruptFunction, FALLING);
}
