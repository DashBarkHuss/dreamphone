////////////////////////////////////////////////////////////////////////
// This code is meant to be used with H&B spikershield EOG.
// In this sketch, once an eye movement is made, we can not detect that eye movement twice in a row.
// Meaning you can't look right a little bit, stop, and
// then look right even more-> that will only register as one "right" not "right right".
//
//This sketch waits for the subject to sleep, then detects rem, then attempts to induce lucidity
//once the subject signals that they are lucid, the sketch interprets the morse code the subject is sending
////////////////////////////////////////////////////////////////////////
//--------------------------------Interpret eog  variables--------------------------
#define EOG A0
#include <SPI.h>
#include <SD.h>

File myFile;

//button and speaker variables
#define SPEAKER 6

int inPin = 4;    // pushbutton connected to digital pin 7
int lastButtonVal = 0;
int newButtonVal;

//bool volume = 1;

bool volume = 0;

const int bufferSize = 20;
const int highThreshold = 600;
const int lowThreshold = 422;
int readingsBuffer[bufferSize]; //last readings
enum eogSignal {
  NORMAL,
  JAW,
  RIGHT,
  LEFT
};
enum eogSignal lastEogSignal;
//--------------------------------State Management variables--------------------------
enum dspStates {
  WAIT_FOR_SLEEP,       // delay program to allow subject to fall asleep
  WATCH_FOR_REM,        // monitor eye movements to detect rem
  INDUCE_LUCIDITY,      // flicker lights (trigger) and monitor subject for 'lucid' signal.
  INTERPRET_MORSE_CODE  // interpret eye morse code movements
};
//enum dspStates state = 0;
enum dspStates state = INDUCE_LUCIDITY;
//------------------------------------------WAIT_FOR_SLEEP variables----------------------------------
int minutesUntilSleep = 0;
//----------------WAIT_FOR_SLEEP Button variables----------
const int buttonPin = 7;    // the number of the pushbutton pin
const int ledPin = 9;      // the number of the LED pin on the Spikershield to indicate the button was pressed
// Variables will change:
int ledState = HIGH;         // the current state of the output pin
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
//--------------------------------WATCH_FOR_REM variables--------------------------
unsigned long remDetectedAt = 0;
unsigned long eyesMovedBuffer[4] = {0, 0, 0, 0}; //Every "interval" milliseconds store if the eyes moved
const int interval = 2 * 1000; //the milliseconds for "interval"^
unsigned long lastIntervalStart = 0;
//-------------------------INTERPRET_MORSE_CODE && lucid code variables-------------------------
bool subjectStartedSignaling = false;
//--------------MORSE CODE--------------
const unsigned long morseBitBase = 800 ; //morse bit length
const unsigned long wiggleRoom = 0;
const unsigned long morseBit = morseBitBase + wiggleRoom;
const unsigned long morse3Bit = (morseBit * 3) + wiggleRoom;
unsigned long lastSignalChangeAt;
bool lastSignal;
unsigned long clearGapLength = morseBit * 5 + wiggleRoom; //message variable
unsigned long sendBeepLength = morseBit * 5 + wiggleRoom; //message variable
String message = "";
enum morseState {
  NEW_INTRA_CHARACTER,
  NEW_CHARACTER,
  NEW_WORD,
  NEW_MESSAGE
};
enum morseState currentMorseState = NEW_INTRA_CHARACTER;
String subjectsMorseCode = "";
//--------------INDUCE_LUCIDITY
const int brightnessIncrement = 255 / 5;
int brightness = brightnessIncrement;
const int lucidLedPin = 9; //whatever pin your leds are connected to
bool subjectLucid = false;
unsigned long triggeredLucidityAt;
const unsigned long minLucidityLength = morseBit * 5; //4000 of beep
String subjectsLucidSignal = "";
const String lucidSignal = "l.";
unsigned long subjectLucidAt;
const int minutesUntilNextTrigger = 1;
const int minutesUntilDetectRemAgain = 20;
//------------------------minutes
const unsigned long minutes = 60000;
void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT); // LED1.
  pinMode(lucidLedPin, OUTPUT); // LED1.

  pinMode(SPEAKER, OUTPUT);
  pinMode(inPin, INPUT);    // sets the digital pin 7 as input

  Serial.print("Initializing SD card...");

  if (!SD.begin(5)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");


}
void loop() {
  newButtonVal = digitalRead(inPin);   // read the input pin
  if (newButtonVal != lastButtonVal) {
    toggleVolume();
    digitalRead(SPEAKER);
  }


  //-----------------------------------wait for subject to sleep--------------------------------
  if (state == WAIT_FOR_SLEEP) {
    Serial.print(F("waiting ")) ;
    Serial.print(minutesUntilSleep);
    Serial.println(F(" minutes"));
    delay(minutesUntilSleep * minutes);
    state = WATCH_FOR_REM;
  } else if (state != WAIT_FOR_SLEEP) {
    manageWaitForSleepButton();
  }

  //--------------------------------variables---------------------------------
  int reading = analogRead(EOG);
  unsigned long now = millis();
  updateBuffer(reading);
  int timeSinceRem = now - remDetectedAt;
  bool remRecentlyDetected = timeSinceRem < 20 * minutes;
  //---------------determine if the last analogRead signals was an eye movement---------------------
  enum eogSignal eog = determineEOGSignal(); //returns enum LEFT, RIGHT, JAW, NORMAL
  bool eyeMovementDetected = eog > 1;

  //------- sound ------- helpful for debugging--- comment out if not debugging
  if (eog == LEFT) {
    speakerOn();
  } else if (eog == RIGHT) {
    speakerOff();
  }

  //------------------------------WATCH_FOR_REM-----------------------------------
  if (state == WATCH_FOR_REM) {
    if (eyeMovementDetected) {
      if (subjectStartedSignaling) subjectStartedSignaling = false; // reset subject started signalling
      updateEyeMovedBuffer(now);
      if (detectRem(now)) {
        remDetectedAt = now;
        state = INDUCE_LUCIDITY;
      };
    }
    //------------------------------INDUCE_LUCIDITY-----------------------------------
  } else if (state == INDUCE_LUCIDITY) {
    bool lucidityRecentlyTriggered =  triggeredLucidityAt == 0 ? 0 : (now - triggeredLucidityAt <= minutesUntilNextTrigger * minutes) ;
    if (eyeMovementDetected) {
      if (!subjectStartedSignaling) {
        subjectStartedSignaling = true;
      } else if (subjectStartedSignaling) {
        unsigned long signalLength = now - lastSignalChangeAt;
        Serial.print(F("Signal: "));
        Serial.println(lastSignal);
        Serial.println(signalLength);
        updateSubjectsLucidSignal( signalLength, lastSignal );
        Serial.println(subjectsLucidSignal);
        if (subjectsLucidSignal == lucidSignal) {
          state = INTERPRET_MORSE_CODE;
          subjectLucidAt = now;
          Serial.println(F("++++++LUCID+++++++++"));
          subjectsLucidSignal = ""; //reset for next time
          brightness = brightnessIncrement; //reset
        }
      }
      lastSignalChangeAt = now;
      lastSignal = eog == RIGHT ? 0 : 1;
    }
    //                               Serial.print(lucidityRecentlyTriggered);
    if (state == INDUCE_LUCIDITY && !lucidityRecentlyTriggered) {
      flickerLight(brightness, 100, 5); //can get brighter overtime?
      if (brightness < 256) {
        brightness = brightness + brightnessIncrement;
      }
      Serial.print(F("++++++Attempt To induce lucidity+++++++++"));
      Serial.println();
      triggeredLucidityAt = now;
    }
    if (now - remDetectedAt > minutesUntilDetectRemAgain * minutes) {
      state == WATCH_FOR_REM;
    }
    //------------------------------INTERPRET_MORSE_CODE-----------------------------------
  } else if (state == INTERPRET_MORSE_CODE) {

    //--------------------------------sound---------------------------------

    if (eyeMovementDetected) {
      if (!subjectStartedSignaling) {
        subjectStartedSignaling = true;
      } else {
        unsigned long signalLength = now - lastSignalChangeAt;
        char morseIntraChar = signalToMorse(signalLength, lastSignal);
        Serial.print(F("subjectsMorseCode: '")); Serial.print(subjectsMorseCode); Serial.println(F("'"));
        if (morseIntraChar != 'g' && morseIntraChar != '*' && morseIntraChar != 'l') {
          subjectsMorseCode.concat(morseIntraChar);
        } else if (morseIntraChar == 'l') {
          Serial.print(F("Message: "));
          Serial.println(message);
          //
          //          //-----how the message is handled----
          myFile = SD.open("test.txt", FILE_WRITE);
          Serial.print(F("myFile: "));
          Serial.println(myFile);


          // if the file opened okay, write to it:
          if (myFile) {
            Serial.print(F("Writing to test.txt..."));
            myFile.println("New message:" + message);
            // close the file:
            myFile.close();
            Serial.println(F("done."));
          } else {
            // if the file didn't open, print an error:
            Serial.println(F("error opening test.txt"));
          };
          message = "";
          subjectsMorseCode = "";
        } else if (morseIntraChar != '*') {
          if (currentMorseState == NEW_CHARACTER || currentMorseState == NEW_WORD) {

            char engChar = morseToEnglishChar(subjectsMorseCode);
            if (engChar == '*')Serial.print("error, wrong input: " + subjectsMorseCode);
            Serial.print(" char: ");
            Serial.println(engChar);
            if (engChar != '*') {
              if (currentMorseState == NEW_WORD) message.concat(" ");
              message.concat(engChar);
              //
              Serial.print(F("message pending: "));
              Serial.println(message);
            }
            subjectsMorseCode = "";
          }  else if (currentMorseState == NEW_MESSAGE) {
            message = "";
            subjectsMorseCode = "";
            Serial.print(F("cleared message pending: "));
            Serial.println(message);
          }
        }
      }
      lastSignalChangeAt = now;
      lastSignal = eog == RIGHT ? 0 : 1;
    }
    if (now - minutesUntilDetectRemAgain > 20 * minutes) {
      state == WATCH_FOR_REM;
    }
  }
}
//===============================WAIT_FOR_SLEEP functions================
void manageWaitForSleepButton() {
  int buttonReading = digitalRead(buttonPin);
  bool buttonWasPressed = buttonPressed(buttonReading);
  if (buttonWasPressed) {
    minutesUntilSleep = 20;
    state = WAIT_FOR_SLEEP;
  }
}
bool buttonPressed(bool buttonReading) {
  bool buttonPressed = false;
  if (buttonReading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (buttonReading != buttonState) {
      buttonState = buttonReading;
      if (buttonState == HIGH) {
        buttonPressed = true;
      }
    }
  }
  lastButtonState = buttonReading;
  return buttonPressed;
}
//===============================Morse Code Interpretation functions=========================
void updateSubjectsLucidSignal(unsigned long signalLength, bool lastSignal ) {
  char morseChar = signalToMorse(signalLength, lastSignal);
  if (morseChar != 'g') {
    subjectsLucidSignal.concat(morseChar);
  } else if (currentMorseState == NEW_CHARACTER || currentMorseState == NEW_WORD) {
    subjectsLucidSignal = "";
  }
}
char signalToMorse(unsigned long signalLength, bool signalType ) {



  char morse;
  if (signalType) {
    morse = interpretBeep(signalLength);
  } else {

    manageMorseState (signalLength);

    morse = 'g'; //gap
  }
  return morse;
}
char interpretBeep (unsigned long millisBeep) {
  char meaningOfBeep;
  if ( millisBeep >= 100 && millisBeep <= morseBit) {
    meaningOfBeep = '.';
  }  else if ( millisBeep > morseBit && millisBeep < minLucidityLength) { //or can be set to <= morse3Bit
    meaningOfBeep = '-';
  } else if ( millisBeep >= minLucidityLength ) {
    meaningOfBeep = 'l'; // used in lucid signal (l.) in INDUCE_LUCIDITY state or can be used as "SEND"/"ENTER" when in INTERPRET_MORSE_CODE state
  } else {
    meaningOfBeep = '*';
  }
  return meaningOfBeep;
}
void manageMorseState (unsigned long millisGap) {
  if ( millisGap < 100 ) {
    return; //noise
  } else if ( millisGap >= 100 && millisGap <= morseBit) {
    currentMorseState = NEW_INTRA_CHARACTER;
  }  else if ( millisGap >= morseBit && millisGap < morse3Bit) {
    currentMorseState = NEW_CHARACTER;
  } else if ( millisGap > morse3Bit && millisGap < clearGapLength ) {
    currentMorseState = NEW_WORD;
  }  else if ( millisGap > clearGapLength ) {
    currentMorseState = NEW_MESSAGE; //aka clear last message
  }
}

char morseToEnglishChar(String morse)
{
  Serial.print("morse code: ");
  Serial.println(morse);
  static String letters[] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-",
                             ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..", "E"
                            };
  int i = 0;
  char englishChar;
  if (morse == ".-.-.-")
  {
    englishChar = '.';        //for break
  }
  else if (morse == ".-.-.--")
  {
    englishChar = '!';        //for !
  } else if (morse == "..--..")
  {
    englishChar = '?';        //for ?
  }
  else
  {
    while (letters[i] != "E")  //loop for comparing input code with letters array
    {
      if (letters[i] == morse)
      {
        englishChar = (char('A' + i));
        break;
      }
      i++;
    }
    if (letters[i] == "E")
    {
      englishChar = '*';  //if input code doesn't match any letter, error
    }
  }
  return englishChar;
}
//========================================Rem Detection functions=========================================
bool detectRem(unsigned long now) {
  bool remDetected = true;
  for (int i = 0; i < 4; i++)  //reset buffer
  {
    if (eyesMovedBuffer[i] == 0) {
      remDetected =  false;
      return remDetected;
    }
  }
  Serial.println("REM++++++++++++++++++++++");
  return remDetected;
}
void updateEyeMovedBuffer(unsigned long now) {
  unsigned long delta = now - lastIntervalStart;
  if (delta < interval) {
    return;
  }
  if (delta >= interval && delta < 2 * interval || lastIntervalStart == 0) {
    unsigned long eyeMovedTime = now; //
    updateULBuffer( eyeMovedTime, eyesMovedBuffer, 4 );
    lastIntervalStart = now;
  } else if (delta >= 2 * interval) {
    for (int i = 0; i < 4; i++)  //reset buffer
    {
      eyesMovedBuffer[i] = 0;
    }
  }
}
void updateULBuffer(unsigned long reading, unsigned long bufferArray[], int bufSize) { // unsigned long too specific
  for (int i = 0; i < bufSize - 1; i++)
  {
    bufferArray[i] = bufferArray[i + 1];
  }
  bufferArray[bufSize - 1] = reading;
}
void flickerLight(int brightness, int msDelay, int numOfFlicklers) {
  Serial.println("LIGHT____________");
  for (int i = 1; i <= numOfFlicklers; i++) {
    analogWrite(lucidLedPin, brightness);
    delay(msDelay);
    analogWrite(lucidLedPin, 0);
    delay(msDelay);
  }
}
//===============================Eye Movement Interpretation functions=========================
enum eogSignal determineEOGSignal() {
  int reading = readingsBuffer[bufferSize - 1];
  enum eogSignal determinedSignal;
  if (!isNormal(reading)) {
    // what is the signal?
    if (signalIsJawClench(readingsBuffer)) {
      determinedSignal = JAW;
    } else if (signalIsEyeLeft(readingsBuffer) && lastEogSignal != LEFT) {
      lastEogSignal = LEFT;
      determinedSignal = LEFT;
    } else if (signalIsEyeRight(readingsBuffer) && lastEogSignal != RIGHT) {
      lastEogSignal = RIGHT;
      determinedSignal = RIGHT;
    } else {
      determinedSignal = 0;
    }
  } else {
    determinedSignal = 0;
  }
  return determinedSignal;
}
String getEogSignalName(enum eogSignal eogSignal) {
  switch (eogSignal)
  {
    case NORMAL: return "NORMAL";
    case JAW: return "JAW";
    case RIGHT: return "RIGHT";
    case LEFT: return "LEFT";
  }
}
void updateBuffer(int reading) {
  for (int i = 0; i < bufferSize - 1; i++)
  {
    readingsBuffer[i] = readingsBuffer[i + 1];
  }
  readingsBuffer[bufferSize - 1] = reading;
}
bool isNormal(int reading) {
  if (reading > lowThreshold && reading < highThreshold) {
    return true;
  } else {
    return false;
  };
};
bool isHigh(int reading) {
  if (reading > highThreshold) {
    return true;
  } else {
    return false;
  };
};
bool isLow(int reading) {
  if (reading < lowThreshold) {
    return true;
  } else {
    return false;
  };
};
bool signalIsJawClench(int readings[bufferSize]) {
  for (int i = 0; i < bufferSize; i++)
  {
    if (isHigh(readings[i])) { //if any reading is high
      for (int i2 = i; i2 < bufferSize - i; i2++) {
        if (isLow(readings[i2])) { // and any proceeding reading is low
          return true;
        }
      }
    }
    if (isLow(readings[i])) { //if any reading is Low
      for (int i2 = i; i2 < bufferSize - i; i2++) {
        if (isHigh(readings[i2])) { // and any proceeding reading is HIgh
          return true;
        }
      }
    }
  }
  return false;
}
bool signalIsEyeRight(int readings[bufferSize]) {
  for (int i = 0; i < bufferSize; i++) {
    if (!isHigh(readings[i])) {
      return false;
    }
  }
  return true;
}
bool signalIsEyeLeft(int readings[bufferSize]) {
  for (int i = 0; i < bufferSize; i++) {
    if (!isLow(readings[i])) {
      return false;
    }
  }
  return true;
}
//===============================Speaker Functions=========================

void toggleVolume() {
  if (newButtonVal == 1 && lastButtonVal == 0) {
    volume = !volume;
    lastButtonVal = !lastButtonVal;
  }

  if (newButtonVal == 0 && lastButtonVal == 1) {
    lastButtonVal = !lastButtonVal;
  }
}

void speakerOn() {
  analogWrite(SPEAKER, volume);
  //  Serial.println("on");
}
void speakerOff() {
  analogWrite(SPEAKER, 0);
  //  Serial.println("off");

}

//===============================Debbuging Functions=========================
void printArray(int myArray[], int size_of_myArray) {
  for (int i = 0; i < size_of_myArray; i++)
  {
    Serial.println(String(i) + String(": ") + String(myArray[i]));
  }
}
void printULArray(unsigned long myArray[], int size_of_myArray) {
  for (int i = 0; i < size_of_myArray; i++)
  {
    Serial.println(String(i) + String(": ") + String(myArray[i]));
  }
}
