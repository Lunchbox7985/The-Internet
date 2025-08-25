#include <DFMiniMp3.h>
#include <RotaryEncoder.h>
#include <TM1637Display.h>
#include <SoftwareSerial.h>

// Define pins connected to the rotary encoder
const int DT_PIN = 4;
const int CLK_PIN = 5;
const int SW_PIN = 6;

// define pins for the 7segment display
const int CLK = A4;
const int DIO = A5;

//define pins for the LEDs
//make red off led blink by plugging it in to pin 2, and fade by plugging it into pin 3
const int offLedFade = 3;   //the red light on the front of the box in fade mode
const int offLedBlink = 2;  //the red light on the front of the box in blink mode
const int onLed = A1;       //the green light on the front of the box
const int offSelLed = A2;   //the light that indicates you have "off" sound selected to change
const int onSelLed = A3;    //the light that indicates you have "on" sound selected to change

//define pins for the knife switch
const int onSw = 12;   //the on side of the knife sw
const int offSw = 13;  //the off side of the knife sw

//various variables for code execution
int onSound = 1;             //current on sound
int offSound = 1;            // current off sound
int combinedDisplay = 0;     //combined value of on then off digits as a 4 digit number
bool controlOnSound = true;  // Flag to control which variable is being modified
bool controlVolume = false;  //flag to change to volume control
int otherSound = 1;          // variable to store value of sounds and volume for when you switch between them
bool randomMode = false;     //flag to enable random play

//variable for how many sounds for on and off, this gets automatically updated during
// void setup based on the number of files in each folder
int numOnSounds = 0;   //how many "on" sounds you have
int numOffSounds = 0;  //how many "off" sounds you have

//sets default volume and display brightness on boot. volume can be changed after,
//did not implement user control of display brightness
int volume = 15;         //sound board boot up volume (value of 1-30) can be changed with knob
int dispBrightness = 4;  //display brightness (value from 1 to 7)

class Mp3Notify;
// Create a TM1637 display object
TM1637Display display(CLK, DIO);
//Create object for rotary encoder
RotaryEncoder encoder(DT_PIN, CLK_PIN);

//defines pins for software serial communication to sound board
const byte rxPin = 10;
const byte txPin = 11;
SoftwareSerial swSerial(rxPin, txPin);

// defines several things for the soundboard. honestly i copied this from one of the examples,
//i dont fully understand it. i know it prints some status messages in serial monitor.
typedef DFMiniMp3<SoftwareSerial, Mp3Notify> DfMp3;
DfMp3 player(swSerial);

class Mp3Notify {
public:
  static void PrintlnSourceAction(DfMp3_PlaySources source, const char* action) {
    if (source & DfMp3_PlaySources_Sd) {
      Serial.print("SD Card, ");
    }
    if (source & DfMp3_PlaySources_Usb) {
      Serial.print("USB Disk, ");
    }
    if (source & DfMp3_PlaySources_Flash) {
      Serial.print("Flash, ");
    }
    Serial.println(action);
  }
  static void OnError([[maybe_unused]] DfMp3& mp3, uint16_t errorCode) {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }
  static void OnPlayFinished([[maybe_unused]] DfMp3& mp3, [[maybe_unused]] DfMp3_PlaySources source, uint16_t track) {
    Serial.print("Play finished for #");
    Serial.println(track);
  }
  static void OnPlaySourceOnline([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "online");
  }
  static void OnPlaySourceInserted([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "inserted");
  }
  static void OnPlaySourceRemoved([[maybe_unused]] DfMp3& mp3, DfMp3_PlaySources source) {
    PrintlnSourceAction(source, "removed");
  }
};

// state of the switch:
int OnState = 0;   // variable for reading the On status
int OffState = 0;  // variable for reading the Off status

//initally set states to off. these are states for the position of the knife switch
bool on = false;
bool off = false;
bool mid = false;

//make red off led blink by plugging it in to pin 2, and fade by plugging it into pin 3
//variables to help the red offLed flash when knife switch is off
unsigned long previousMillis = 0;
const long interval = 250;  //how often to flash the red led in milliseconds
int ledState = LOW;

//variable to help the red offLed fade on and off when the knife switch is off
unsigned long previousMillisFade = 0;
int fadeSpeed = 25;  //determines speed of fade, higher number is slower
int brightness = 0;  // Adjust this value (0-255) to control brightness
bool fadeUp = true;  // Flag to track fade direction (up or down)

//this is so the display only updates on change
int previousCombinedDisplay = 0;  // Store the previous value of combinedDisplay
int previousvolume = 1;           // Store the previous value of volume
int previousDisp = 0;

//variables for button debounce
unsigned long lastButtonPress = 0;
const int debounceTime = 500;  // debounce time for pressing knob

//settings for 7 seg to display "rAnd" for random mode
const uint8_t SEG_rAnd[] = {
  SEG_E | SEG_G,                                  // r
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  // A
  SEG_C | SEG_E | SEG_G,                          // n
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,          // d
};

//settings for 7 seg to display "LOAd" for random mode
const uint8_t SEG_LOAd[] = {
  SEG_F | SEG_E | SEG_D,                          // L
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,  // O
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,  // A
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,          // d
};

void setup() {
  display.setBrightness(dispBrightness);  //sets display brightness to global set value
  display.setSegments(SEG_LOAd);
  Serial.begin(115200);
  swSerial.begin(9600);   // Begin SoftwareSerial
  pinMode(rxPin, INPUT);  // Must manually set pin modes for RX/TX pins (SoftwareSerial bug)
  pinMode(txPin, OUTPUT);
  player.begin();  // Initializes module
  player.reset();  //start from a know state

  //the following is a workaround to get total number of tracks in each folder
  player.setVolume(0);                            //mutes player since we have to play a file for a moment
  player.playFolderTrack(1, 1);                   //plays a file in the first folder
  delay(100);                                     //waits a moment
  player.stop();                                  //then stops play
  numOnSounds = (player.getFolderTrackCount(1));  //reads total tracks from last folder played
  player.playFolderTrack(2, 1);                   //repeats the process for the second folder
  delay(100);
  player.stop();
  numOffSounds = (player.getFolderTrackCount(1));
  //end folder track counts section

  player.setVolume(volume);  //sets volume to default value
  pinMode(onLed, OUTPUT);    //sets inputs and outputs accordingly
  pinMode(offLedFade, OUTPUT);
  pinMode(offLedBlink, OUTPUT);
  pinMode(onSelLed, OUTPUT);
  pinMode(offSelLed, OUTPUT);
  pinMode(onSw, INPUT_PULLUP);  //uses pullup resistors to prevent folating values and sporadic behaviour
  pinMode(offSw, INPUT_PULLUP);
  analogWrite(onSelLed, 255);  //defaults to selecting on sound, so turn this led on at boot
  analogWrite(offSelLed, 0);   //defaults to selecting on sound, so turn this led off at boot
  encoder.setPosition(1);      //defaults to 0 which trips the wrap around, so we manually set it to 1 on boot
}  // setup()

void loop() {

  //  needed for serial communication between arduino and DFPlayer, otherwise messages get backlogged and comms stop
player.loop();
  
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
      encoder.setPosition(numOffSounds);
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
    digitalWrite(offLedBlink, LOW);
    if (randomMode == false) {
      player.setVolume(volume);
      player.playFolderTrack(1, onSound);  //plays selected on sound
    } else {
      player.setVolume(volume);
      player.playFolderTrack(1, (random(1, numOnSounds + 1)));  //plays random on sound
    }
    on = true;
    mid = false;
    off = false;
  }

  // if switch is off
  if ((OnState == HIGH) && (OffState == LOW) && (off == false)) {
    if (randomMode == false) {
      player.setVolume(volume);
      player.playFolderTrack(2, offSound);  //plays selected off sound
    } else {
      player.setVolume(volume);
      player.playFolderTrack(2, (random(1, numOffSounds + 1)));  //plays random off sound
    }
    analogWrite(onLed, 0);
    on = false;
    mid = false;
    off = true;
  }

  // if switch is in the middle
  if ((OnState == HIGH) && (OffState == HIGH) && (mid == false)) {
    analogWrite(onLed, 0);
    digitalWrite(offLedBlink, LOW);
    analogWrite(offLedFade, 0);
    player.stop();
    on = false;
    mid = true;
    off = false;
    brightness = 0;
    fadeUp = true;
  }

  //switch case for controlling what variable you are changing with the knob
  if (digitalRead(SW_PIN) == LOW && millis() - lastButtonPress > debounceTime) {
    lastButtonPress = millis();
    previousCombinedDisplay = 1;
    previousvolume = 1;
    previousDisp = 1;
    encoder.setPosition(otherSound);
    static int mode = 1;  // Variable to store current mode (1, 2, 3, or 4)
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
      if (ledState == HIGH) {
        ledState = LOW;
      } else {
        ledState = HIGH;
      }

      // set the LED with the ledState of the variable:
      digitalWrite(offLedBlink, ledState);
    }
    // Fading functionality for offLedFade (assuming pin 3) with up/down control
    unsigned long currentMillisFade = millis();  // Separate millis for fading


    if (currentMillisFade - previousMillisFade >= fadeSpeed) {  // Adjust interval for fading speed
      previousMillisFade = currentMillisFade;

      if (fadeUp) {
        brightness = brightness + 5;
        if (brightness >= 255) {
          brightness = 255;
          fadeUp = false;
        }
      } else {
        brightness = brightness - 5;
        if (brightness <= 0) {
          brightness = 0;
          fadeUp = true;
        }
      }
      analogWrite(offLedFade, brightness);
    }
  }
}  // loop ()

