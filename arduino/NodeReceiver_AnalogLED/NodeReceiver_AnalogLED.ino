#include <SPI.h> 
#include <nRF24L01.h> 
#include <RF24.h> 
#include "printf.h"

/** Node Receiver by Evelyn Dooley
 *  Analog LED variant
 *  For nodes connected directly to analog LEDs
 *  
 *  Capabilities:
 *  Power
 *  Color
 *  ColorTemp
 */

// This pin is used to set the nRF24 to standby (0) or active mode (1)
#define pinCE 9

// This pin is used to tell the nRF24 whether the SPI communication
// is a command or message to send out
#define pinCSN 10

// The pins that the red, green, and blue LED wires are connected to
// Must be PWM pins
#define REDPIN 5
#define GREENPIN 6
#define BLUEPIN 3

// Node's light index, i. e. which light this node controls
// Available Lights:
// 0: All Lights
// 1: Wall Light
// 2: Desk Light
// 3: Mirror Light
// 4: Bed Light
// 5: Couch Light
// 6: PC Lights

int myIndex = 5;  // Couch light

// Global vars
RF24 wirelessSPI(pinCE, pinCSN);            // Create the nRF module
const uint64_t pAddress = 0xB00B1E5000LL;   // Radio pipe address
char buffer [13];                           // Buffer for received frame
int flag = 0;                               // Flag that indicates when buffer updated
uint8_t red = 255;                          // Current RGB color values, set to 'Soft White'
uint8_t green = 110;                              
uint8_t blue = 57;                               
uint8_t prevRed = red;                      // Previously saved color values
uint8_t prevGreen = green;
uint8_t prevBlue = blue;

// Setup runs only once
void setup() { 
  Serial.begin(115200);
  wirelessSPI.begin();                      // Start the nRF24 module
  wirelessSPI.maskIRQ(1,1,0);               // mask all IRQ triggers except for receive (1 is mask, 0 is no mask)
  wirelessSPI.setPALevel(RF24_PA_LOW);      // Set power level to low, won't work well at higher levels (interfere with receiver)
  wirelessSPI.setDataRate(RF24_250KBPS);    // 250KBPS data rate is more reliable, and frames are only 12 bytes
  wirelessSPI.openReadingPipe(1,pAddress);  // open pipe o for recieving meassages with pipe address
  wirelessSPI.startListening();             // Start listening for messages
  printf_begin();                           // Start printf to display radio details 
  wirelessSPI.printDetails();               // Prints the radio's stats to the serial console, for debugging purposes
  pinMode(REDPIN, OUTPUT);                  // Set up output pins for analog writing
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(2), interruptFunction, FALLING);  // Allow for interrupts on receive at Pin 2
}

// Loop
void loop() {
  // If the interrupt sets this flag, read and parse the buffer
  while (flag == 1) {
    bufferUpdated();
  } 

  // Write the RGB color values to the strip
  analogWrite(REDPIN, red);
  analogWrite(GREENPIN, green);
  analogWrite(BLUEPIN, blue);
}

// Color decode function
//
// Decodes the color from the buffer, and sets the variables
//
// buff: the buffer to decode
void setColor(char *buff) {
  Serial.println("Color");
  char redc [4];
  char greenc [4];
  char bluec [4];
  for (int i=3; i < 12; i++) {  // Processing loop, sorts the RGB values into variables
    
    // Red
    if (i < 6) {
      redc[i - 3] = buff[i];
    }

    // Green
    else if (i >= 6 && i < 9) {
      greenc[i - 6] = buff[i];
    }

    // Blue
    else if (i >= 9 && i < 12) {
      bluec[i - 9] = buff[i];
    }
  }
  
  red = atoi(redc);        // Cast to int so they can be set on the next loop
  green = map(atoi(greenc), 0, 255, 0, 150); // Color correction for strip color variance
  blue = map(atoi(bluec), 0, 255, 0, 150);
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
  Serial.println("Power");
  // Power Off
  if (state == '0') {
    prevRed = red;
    prevGreen = green;
    prevBlue = blue;
    red = 0;
    green = 0;
    blue = 0;
  }

  // Power On
  else if (state == '1') {
    red = prevRed;
    green = prevGreen;
    blue = prevBlue;
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
    Serial.println(index);
    if (index == myIndex || index == '0') {           // Checks if the received command is for this node's light(s)
      Serial.println("My light");
      switch (buffer[2]) {                            // Choose the specified operation

        // Power
        case 'P'  :
          setPower(buffer[3]);                        // Set power to 1 if on, 0 if off 
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
