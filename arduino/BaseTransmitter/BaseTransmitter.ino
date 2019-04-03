// -*- mode: C++ -*-
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "printf.h"

//This pin is used to set the nRF24 to standby (0) or active mode (1)
#define pinCE 9

//This pin is used to tell the nRF24 whether the SPI communication
// is a command or message to send out
#define pinCSN 10

uint8_t frame[] = "X0P000000000"; // Packet format X[Light Index][Function][9 bytes of data].
// Available Lights:
// 0: All Lights
// 1: Wall Light
// 2: Desk Light
// 3: Mirror Light
// 4: Bed Light
// 5: Couch Light
// 6: PC Lights

// Available functions:
//
// P: Power : Turns the specified light on or off.
// Data Format: [P]0000000
// P can be 0 for power off or 1 for power on.
//
// B: Brightness
// Data Format: [000-255]000000
// Brightness value between 0 and 255, expressed with 3 digits (1 would be 001)
//
// I/D: Increment/Decrement Brightness
// Data Format: 000000000
// Increments or decrements the brightness by 1 value of 17.
//
// C: Set the Color
// Data Format: [RRR][GGG][BBB]
// Sets the color of the LEDs via red, green, and blue values.

// Create your nRF24 object or wireless SPI connection
RF24 wirelessSPI(pinCE, pinCSN);
// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pAddress = 0xB00B1E5000LL;

void setup()  
{
  //start serial to communicate process
  Serial.begin(9600);
  printf_begin();
  //Start the nRF24 module
  wirelessSPI.begin();
  wirelessSPI.setPALevel(RF24_PA_MAX);
  wirelessSPI.setDataRate(RF24_250KBPS);
  // pipe address that we will communicate over,
  // must be the same for each nRF24 module
  wirelessSPI.openWritingPipe(pAddress);
  //transmitter so stop listening for data
  wirelessSPI.stopListening();
  wirelessSPI.printDetails();
}

void loop() {
//  wirelessSPI.write( &frame, sizeof(frame) );
  while(Serial.available() > 0) {
      uint8_t inByte = Serial.read();
      // Filtering the serial input to ensure it matches proper format
      if(inByte == 'X') {
          Serial.readBytes(&frame[1],11);
          if(frame[1] < '0' || frame[1] > '6') break;
          if(frame[2] < 'A' || frame[2] > 'Z' || frame[2] == 'X') break;
          if(frame[3] < '0' || frame[3] > '9') break;
          if(frame[4] < '0' || frame[4] > '9') break;
          if(frame[5] < '0' || frame[5] > '9') break;
          if(frame[6] < '0' || frame[6] > '9') break;
          if(frame[7] < '0' || frame[7] > '9') break;
          if(frame[8] < '0' || frame[8] > '9') break;
          if(frame[9] < '0' || frame[9] > '9') break;
          if(frame[10] < '0' || frame[10] > '9') break;
          if(frame[11] < '0' || frame[11] > '9') break;
          
          //if the send fails let the user know over serial monitor
          Serial.write((char*)frame);
          if (wirelessSPI.write( &frame, sizeof(frame) )){
              Serial.write("Transmitted frame successfully!");
              delay(100);
              wirelessSPI.write(&frame, sizeof(frame)); // write again due to unreliable connection
          } else {
              Serial.write("Transmitting frame failed!");
          }
      }
  }
}
