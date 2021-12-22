#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

#include <ArduinoJson.h>
// #include <stdlib.h>
// #include <bits/stdc++.h>
#include <algorithm>
#include <TFT_eSPI.h> // Hardware-specific library



#define BLACK 0x0000
#define WHITE 0xFFFF


// #include "Participant.h"


TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

int16_t h;
int16_t w;

uint32_t seed;

uint16_t bgColour = TFT_MAGENTA;

typedef struct Participant {
  std::string name;
  int imageCount;
} Participant;

SPIClass SdSPI;
std::vector<uint8_t> RaffleVector;
std::vector<Participant> WfofParticipants;
StaticJsonDocument<16384> WfofJson;
// #include <bitmapFunctions.h>

/***********************************************************************************************************************************/
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.println("Booted...: ");
  h = tft.height();
  w = tft.width();
  tft.setRotation(3);
  WiFi.begin();
  WiFi.mode(WIFI_STA);
  tft.println("Tap to generate a new sequence");
  randomSeed(esp_random());
  seed = esp_random();

  SdSPI.begin(18, 19, 23, 5);
  SD.begin(5, SdSPI);

  File testTxt = SD.open("/wfof.json");

  uint8_t *TxtBuffer = new uint8_t[testTxt.size() + 1];
  testTxt.read(TxtBuffer, testTxt.size());
  TxtBuffer[testTxt.size()] = 0;
  Serial.printf("\n%s\n", TxtBuffer);


  deserializeJson(WfofJson, TxtBuffer);
  delete TxtBuffer;

  // RaffleVector.reserve(4096);

  JsonArray WfofArray = WfofJson["participants"].as<JsonArray>();
  // WfofParticipants.reserve(8192);

  int count = 0;
  for (JsonVariant v : WfofArray) {
    Participant lParticipant;
    lParticipant.name = v["name"].as<std::string>();
    lParticipant.imageCount = v["images"].as<int>();
    WfofParticipants.push_back(lParticipant);
    count++;
  }
  Serial.printf("Containing %i participants", WfofParticipants.size());
  for (int i = 0; i < WfofParticipants.size(); i++) {
    Serial.printf("\nFrom WFOF vector: \n %s has %i images \n", WfofParticipants[i].name.c_str(), WfofParticipants[i].imageCount);
    for (uint8_t imageCounter = 0; imageCounter < WfofParticipants[i].imageCount; imageCounter++) {
      RaffleVector.push_back(i);
    }
  }

  Serial.print("Vector: ");
  for (uint16_t i = 0; i < RaffleVector.size(); i++) {
    Serial.printf("%i, ", RaffleVector[i]);
  }
  tft.setCursor(20, 20);
  tft.setTextSize(2);

}


/***********************************************************************************************************************************/
void loop() {
  uint16_t touchX, touchY;
  bool pressed = tft.getTouch(&touchX, &touchY);
  if (pressed) {
    std::vector<uint8_t> ShuffleVector = RaffleVector;

    std::shuffle(ShuffleVector.begin(), ShuffleVector.end(), std::default_random_engine(seed));
    Serial.println("The Shuffled array is: ");

    for (uint8_t i = 0; i < ShuffleVector.size(); i++) {
      Serial.print(ShuffleVector[i]);
      Serial.print(", ");
    }
    delay(200);
    //Pick a "winner":

    uint16_t winnerIndex = random(ShuffleVector.size());
    uint8_t winner = ShuffleVector[winnerIndex];
    //Need to convert to c_str to then convert to an "Arduino string"...
    Serial.println("The chosen index is: " + String(winnerIndex) + " which is the number " + String(winner) + " Which is tied to: " + String(WfofParticipants[winner].name.c_str()));

    //Again, the string conversion
    tft.println("The winner is " + String(WfofParticipants[winner].name.c_str()));
    delay(2000);
    tft.setCursor(20, 20);
    tft.fillScreen(TFT_MAGENTA);

    //remove all instances of "winner" from the vector:
    Serial.println("Removing " + String(winner));
    std::remove(RaffleVector.begin(), RaffleVector.end(), winner);
    //Reduce vector size since this doesn't seem to happen automatically...
    for (uint16_t i = 0; i < WfofParticipants[winner].imageCount; i++) {
      RaffleVector.pop_back();
    }

    //Re-print the vector for debugging:
    for (uint16_t i = 0; i < RaffleVector.size(); i++) {
      Serial.print(RaffleVector[i]);
      Serial.print(", ");
    }
  }
}

