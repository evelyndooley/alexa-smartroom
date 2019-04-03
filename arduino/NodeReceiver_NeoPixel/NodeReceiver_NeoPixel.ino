#include <SPI.h> 
#include <nRF24L01.h> 
#include <RF24.h> 
#include <Adafruit_NeoPixel.h>
#include "printf.h"

/** Node Receiver by Evelyn Dooley
 *  NeoPixel variant
 *  For nodes connected directly to a NeoPixel strip
 *  
 *  Capabilities:
 *  Power
 *  Color
 *  ColorTemp
 *  Brightness
 *    - Increment
 *    - Decrement
 */

// This pin is used to set the nRF24 to standby (0) or active mode (1)
#define pinCE 9

// This pin is used to tell the nRF24 whether the SPI communication
// is a command or message to send out
#define pinCSN 10

// This is the pin the WS2812B data line is connected to
#define pinLED 6

//This is the number of WS2812B LEDs in the strip
#define NUM_LEDS 300

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
RF24 wirelessSPI(pinCE, pinCSN);            // Create the nRF module
const uint64_t pAddress = 0xB00B1E5000LL;   // Radio pipe address
char buffer [13];                           // Buffer for received frame
int flag = 0;                               // Flag that indicates when buffer updated
int brightness = 32;                        // Light's brightness [0-255], default 32
uint32_t prevColor;                         // Temporary storage for color while power is off

// Initialize the NeoPixel strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, pinLED, NEO_GRB + NEO_KHZ800);

// Setup runs only once
void setup() { 
  Serial.begin(115200);
  wirelessSPI.begin();                      // Start the nRF24 module
  wirelessSPI.maskIRQ(1,1,0);               // mask all IRQ triggers except for receive (1 is mask, 0 is no mask)
  wirelessSPI.setPALevel(RF24_PA_LOW);      // Set power level to low, won't work well at higher levels (interfere with receiver)
  wirelessSPI.setDataRate(RF24_250KBPS);    // 250KBPS data rate is more reliable, and frames are only 12 bytes
  wirelessSPI.openReadingPipe(1,pAddress);  // open pipe o for recieving meassages with pipe address
  wirelessSPI.startListening();             // Start listening for messages
  //printf_begin();                           // Start printf to display radio details 
  //wirelessSPI.printDetails();               // Prints the radio's stats to the serial console, for debugging purposes
  strip.begin();                            // Start the NeoPixel strip
  strip.setBrightness(32);                  // Default Brightness Value
  strip.show();                             // Initialize the strip
  for(int p = 0; p < NUM_LEDS; p++) {       // Set it to the default color, 'Soft White'
    strip.setPixelColor(p, 255, 166, 87);
  }
  strip.show();
  
  attachInterrupt(digitalPinToInterrupt(2), interruptFunction, FALLING);  // Allow for interrupts on receive at Pin 2
}

// Loop
void loop() {
  // If the interrupt sets this flag, read and parse the buffer
  while (flag == 1) {
    bufferUpdated();
  } 
}

// Get Brightness function
//
// Returns the brightness value from the buffer
//
// buff: The buffer to decode
int getBrightnessValue(char *buff) {
  char val [3];   
  for (int i=3; i < 6; i++) {    // Reads the 3 characters that are the brightness value
    val[i - 3] = buff[i];
  }
  return atoi(val);              // Return the brightness as an int
}

// Color setting function
//
// Decodes the color from the buffer, and sets the strip to it
//
// buff: the buffer to decode
void setColor(char *buff) {
  char red [4];
  char green [4];
  char blue [4];
  for (int i=3; i < 12; i++) {  // Processing loop, sorts the RGB values into variables
    
    // Red
    if (i < 6) {
      red[i - 3] = buff[i];
    }

    // Green
    else if (i >= 6 && i < 9) {
      green[i - 6] = buff[i];
    }

    // Blue
    else if (i >= 9 && i < 12) {
      blue[i - 9] = buff[i];
    }
  }
  
  int redi = atoi(red);        // Cast to int so they can be set with setPixelColor
  int greeni = atoi(green);
  int bluei = atoi(blue);
  
  for(int p = 0; p < NUM_LEDS; p++) {            // Set all the pixels to the color
    strip.setPixelColor(p, redi, greeni, bluei);
  }
  strip.show();                // Show the colors once they have been set
}


// Brightness Function
//
// val: the brightness level to be set to, defaults to the
// global stored brightness value, but it can be passed a
// value if needed
void setBrightnessLevel(int val = brightness) {
  if (val >= 0 && val <= 255) {    // Sanity check so we don't try to set the brightness to
    strip.setBrightness(val);        // an out of bounds number
  }
  else if (val < 0) {
    brightness = 0;
    setBrightnessLevel(); // recursion, baby
  }
  else if (val > 255) {
    brightness = 255;
    setBrightnessLevel();
  }
  strip.show();
}


// Power function to turn the light on or off
//
// When turning off, store the previous color value and
// black out the pixels
// When turning on, if a color value is stored, sets the
// strip to that value. If not, set it to a default value
//
// state: char of value '1' or '0', 1 is on 0 is off
void setPower(char state) {
  // Power Off
  if (state == '0') {
    prevColor = strip.getPixelColor(2);
    //Serial.println(prevColor);
    for(int p = 0; p < NUM_LEDS; p++) {
      strip.setPixelColor(p, 0, 0, 0);
    }
    strip.show();
  }

  // Power On
  else if (state == '1') {
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
  while (wirelessSPI.available()) {           // While there is data ready
        wirelessSPI.read( &buffer, 13 );      // Get the payload
  }
  flag = 1;                                   // Set the flag so that on the next loop the buffer will process
  detachInterrupt(digitalPinToInterrupt(2));  // Detach the interrupt so we can't be interrupted before
}                                             // the current interrupt's functions are running

// Runs to read in the buffer and calls appropriate functions
void bufferUpdated() {
  if (buffer[0] == 'X') {                             // Ensure the frame is valid, all valid frames begin with 'X'
    //sets the light index
    int index = buffer[1];
    if (index == myIndex || index == '0') {           // Checks if the received command is for this node's light(s)
      switch (buffer[2]) {                            // Choose the specified operation

        // Power
        case 'P'  :
          setPower(buffer[3]);                        // Set power to 1 if on, 0 if off 
           break;

        // Brightness
        case 'B'  :
          brightness = getBrightnessValue(buffer)*2.55;    // Compose brightness value
          setBrightnessLevel();                       // Set the brightness
           break;

        // Increment/Decrement the brightness
        case 'I'  :
          brightness = brightness + 10;
          setBrightnessLevel(); 
           break;
        case 'D'  :
          brightness = brightness - 10;
          setBrightnessLevel();
           break;

        // Color
        case 'C'  :
          setColor(buffer);                           // Set the color to the value in the buffer 
           break;
      }
    }
  }
  Serial.println((char*)buffer);                     // Write the buffer for debug purposes
  flag = 0;                                           // Reset flag since we've processed the buffer
  delay(100);                                         // Short delay to protect from 
  attachInterrupt(digitalPinToInterrupt(2), interruptFunction, FALLING);
}
