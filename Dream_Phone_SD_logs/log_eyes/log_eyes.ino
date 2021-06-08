#define EOG A0
#include <SPI.h>
#include <SD.h>

File myFile;
unsigned long now = millis();


void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  Serial.print(F("Initializing SD card..."));

  if (!SD.begin(5)) {
    Serial.println(F("initialization failed!"));
    while (1);
  }
  Serial.println(F("initialization done."));



}

void loop() {


  if (now != millis()) {
    now = millis();
    writeToFile(String(now), "time");


    int reading = analogRead(EOG);
    writeToFile(String(reading), "EOG");


  }

}

//================================Write to File==============

void writeToFile(String msg, String typeOfMessage) {
  myFile = SD.open("RAW_EOG.txt", FILE_WRITE);


  // if the file opened okay, write to it:
  if (myFile) {


    const String str = typeOfMessage + " : " + msg;


    Serial.print(F("Writing to test.txt..." ));
    Serial.println(str);

    myFile.println(str);
    // close the file:
    myFile.close();
    Serial.println(F("done."));
  } else {
    // if the file didn't open, print an error:
    Serial.println(F("error opening test.txt"));
  };
}
