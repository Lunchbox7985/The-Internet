#include "BY8X01-16P.h"
#include <RotaryEncoder.h>
#include <TM1637Display.h>
#include <SoftwareSerial.h>

// Define pins connected to the rotary encoder
const int DT_PIN = 4;
const int CLK_PIN = 5;
const int SW_PIN = 6;

// define pins for the 7segment display
const int CLK = 2;
const int DIO = 3;

//define pins for the LEDs
const int offLed = A0;     //the red light on the front of the box
const int onLed = A1;      //the green light on the front of the box
const int offSelLed = A2;  //the light that indicates you have "off" sound selected to change
const int onSelLed = A3;   //the light that indicates you have "on" sound selected to change

//define pins for the knife switch
const int onSw = 12;   //the on side of the knife sw
const int offSw = 13;  //the off side of the knife sw

//variables for the current on and off sounds
int onSound = 1;             //current on sound
int offSound = 1;            // current off sound
int combinedDisplay = 0;     //combined value of on then off digits
bool controlOnSound = true;  // Flag to control which variable is being modified
bool controlVolume = false;  //flag to change to volume control
int otherSound = 1;          // variable to store value of sounds and volume for when you switch between them
bool randomMode = false;     //flag to enable random play

//number of sounds loaded to the sound board for on and off
//set this to the maximum number of sounds you have for on and off
//files will be numbered, so your first off sound will be the number
//after your last on sound. example if you have 10 of each
//file 11 will be off sound 1
int numOnSounds = 16;   //how many "on" sounds you have
int numOffSounds = 16;  //how many "off" sounds you have

int volume = 15;         //sound board boot up volume (value of 1-30) can be changed with knob
int dispBrightness = 4;  //display brightness (value from 1 to 7)

// Create a TM1637 display object
TM1637Display display(CLK, DIO);
//Create object for rotary encoder
RotaryEncoder encoder(DT_PIN, CLK_PIN);
// create object for mp3 player
const byte rxPin = 10;
const byte txPin = 11;
SoftwareSerial swSerial(rxPin, txPin);  // SoftwareSerial using RX pin D2 and TX pin D3

BY8X0116P player(swSerial);

// state of the switch:
int OnState = 0;   // variable for reading the On status
int OffState = 0;  // variable for reading the Off status

//initally set states to off. these are states for the position of the knofe switch
bool on = false;
bool off = false;
bool mid = false;

//variables to help the red offLed flash when knife switch is off
unsigned long previousMillis = 0;
const long interval = 250;  //how often to flash the red led in milliseconds
int ledState = LOW;

//this is so the display only updates on change
int previousCombinedDisplay = 0;  // Store the previous value of combinedDisplay
int previousvolume = 1;           // Store the previous value of volume
int previousDisp = 0;
//variables for button debounce
unsigned long lastButtonPress = 0;
const int debounceTime = 500;  // debounce time for pressing knob

const uint8_t SEG_rAnd[] = {
  SEG_E | SEG_G,                                  // r
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  // A
  SEG_C | SEG_E | SEG_G,                          // n
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,          // d

};


void setup() {
  Serial.begin(9600);
  swSerial.begin(player.getSerialBaud());  // Begin SoftwareSerial
  pinMode(rxPin, INPUT);                   // Must manually set pin modes for RX/TX pins (SoftwareSerial bug)
  pinMode(txPin, OUTPUT);
  player.init();  // Initializes module
  player.setVolume(volume);
  display.setBrightness(dispBrightness);
  pinMode(onLed, OUTPUT);
  pinMode(offLed, OUTPUT);
  pinMode(onSelLed, OUTPUT);
  pinMode(offSelLed, OUTPUT);
  pinMode(onSw, INPUT_PULLUP);
  pinMode(offSw, INPUT_PULLUP);
  analogWrite(onSelLed, 255);  //defaults to selecting on sound, so turn this led on at boot
  analogWrite(offSelLed, 0);   //defaults to selecting on sound, so turn this led off at boot
  encoder.setPosition(1);      //defaults to 0 which trips the wrap around and sets on sound to 10 on boot
}  // setup()

void loop() {

  //read and set encoder positions to handle value changes and wrap arounds
  static int pos = 1;
  unsigned long currentMillis = millis();
  encoder.tick();
  OnState = digitalRead(onSw);
  OffState = digitalRead(offSw);
  int newPos = encoder.getPosition();
  if (pos != newPos) {
    pos = newPos;
  }
  if (controlOnSound == true && controlVolume == false && randomMode == false) {
    onSound = newPos;
    if (onSound > numOnSounds) {
      onSound = 1;
      encoder.setPosition(1);
    }
    if (onSound < 1) {
      onSound = numOnSounds;
      encoder.setPosition(numOnSounds);
    }
  } else if (controlOnSound == false && controlVolume == false && randomMode == false) {
    offSound = newPos;
    if (offSound > numOffSounds) {
      offSound = 1;
      encoder.setPosition(1);
    }
    if (offSound < 1) {
      offSound = numOffSounds;
      encoder.setPosition(numOnSounds);
    }
  } else if (controlVolume == true && randomMode == false) {
    volume = newPos;
    if (volume > 30) {
      volume = 30;
      encoder.setPosition(30);
    }
    if (volume < 1) {
      volume = 1;
      encoder.setPosition(1);
    }
  }

  // if switch is on
  if ((OnState == LOW) && (OffState == HIGH) && (on == false)) {
    analogWrite(onLed, 255);
    analogWrite(offLed, 0);
    if (randomMode == false) {
      player.setVolume(volume);
      player.playFileIndex(onSound);  //plays selected on sound
    } else {
      player.setVolume(volume);
      player.playFileIndex(random(1, numOnSounds + 1));  //plays random on sound
    }
    on = true;
    mid = false;
    off = false;
  }

  // if switch is off
  if ((OnState == HIGH) && (OffState == LOW) && (off == false)) {
    if (randomMode == false) {
      player.setVolume(volume);
      player.playFileIndex((offSound + numOnSounds));  //plays selected off sound
    } else {

      player.setVolume(volume);
      player.playFileIndex(random(numOnSounds + 1, numOnSounds + numOffSounds + 1));  //plays random off sound
    }
    analogWrite(onLed, 0);
    on = false;
    mid = false;
    off = true;
  }

  // if switch is in the middle
  if ((OnState == HIGH) && (OffState == HIGH) && (mid == false)) {
    analogWrite(onLed, 0);
    analogWrite(offLed, 0);
    player.stop();
    on = false;
    mid = true;
    off = false;
  }

  //switch case for controlling what you are changing with the knob
  if (digitalRead(SW_PIN) == LOW && millis() - lastButtonPress > debounceTime) {
    lastButtonPress = millis();
    previousCombinedDisplay = 1;
    previousvolume = 1;
    previousDisp = 1;
    encoder.setPosition(otherSound);
    static int mode = 1;  // Variable to store current mode (1, 2, or 3)
    mode++;               // Increment mode on each press (wraps around to 1)
    if (mode > 4) {
      mode = 1;
    }
    switch (mode) {
      case 1:  //on sound
        randomMode = false;
        controlOnSound = true;
        controlVolume = false;
        otherSound = offSound;
        analogWrite(onSelLed, 255);
        analogWrite(offSelLed, 0);
        break;
      case 2:  //off sound
        randomMode = false;
        controlOnSound = false;
        controlVolume = false;
        otherSound = volume;
        analogWrite(onSelLed, 0);
        analogWrite(offSelLed, 255);
        break;
      case 3:  //volume
        randomMode = false;
        controlOnSound = false;
        controlVolume = true;
        otherSound = onSound;
        analogWrite(onSelLed, 0);
        analogWrite(offSelLed, 0);
        break;
      case 4:  //Random Mode
        randomMode = true;
        controlVolume = false;
        analogWrite(onSelLed, 255);
        analogWrite(offSelLed, 255);
        break;
    }
  }

  // combine the variables for on and off sounds into a single 4 digit number and display them
  //only updates if changed
  if ((controlVolume == false) && (randomMode == false)) {
    combinedDisplay = onSound * 100 + offSound;
    if (combinedDisplay != previousCombinedDisplay) {
      display.showNumberDecEx(combinedDisplay, 0x40, true);
      previousCombinedDisplay = combinedDisplay;  // Update for next comparison
    }
  } else if ((controlVolume == true) && (randomMode == false)) {
    //volume display code
    if (previousvolume != volume) {
      display.showNumberDec(volume, false);
      previousvolume = volume;
    }
  } else {
    //displays the word rAnd on the display
    if (previousDisp == 1) {
      display.setSegments(SEG_rAnd);
      previousDisp = 0;
    }
  }

  if (off == true) {
    // Blinking functionality
    if (currentMillis - previousMillis >= interval) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;

      // if the LED is off turn it on and vice-versa:
      if (ledState == 255) {
        ledState = 0;
      } else {
        ledState = 255;
      }

      // set the LED with the ledState of the variable:
      analogWrite(offLed, ledState);
    }
  }
}  // loop ()
