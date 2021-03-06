#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

#include <stdlib.h>
// #include <bits/stdc++.h>
#include <algorithm>      //for std::shuffle
#include <list>
#include <ArduinoJson.h>
#include <TFT_eSPI.h> // Hardware-specific library

#define BLACK 0x0000
#define WHITE 0xFFFF

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

int16_t h;
int16_t w;

uint32_t seed;

uint16_t bgColour = TFT_MAGENTA;

class Participant {
public:
  std::string name;
  uint8_t imageCount;
  Participant(std::string aName, uint8_t aImageCount) {
    name = aName;
    imageCount = aImageCount;
  }
  ~Participant() {

  }
};

SPIClass SdSPI;
StaticJsonDocument<8192> WfofJson;

std::list<Participant *> WfofParticipants;
std::list<Participant *>::iterator participantIterator;

/***********************************************************************************************************************************/
void setup() {
  //Set up serial port:
  Serial.begin(115200);

  pinMode(4, INPUT_PULLUP);
  //Set up the display:
  tft.init();
  h = tft.height();
  w = tft.width();
  tft.setRotation(3);

  tft.print("Hello");

  //Initialise the random number generator:
  WiFi.mode(WIFI_STA);  //set up Wi-Fi to get true random generation
  randomSeed(esp_random());
  seed = esp_random();
  Serial.println("Initialised with random seed:" + String(seed));
  WiFi.mode(WIFI_OFF);

  //Init the SD card:
  SdSPI.begin(18, 19, 23, 5);
  if (!SD.begin(5, SdSPI)) {
    ESP_LOGE("SD", "Could not load SD card");
  }

  //Open the JSON file:
  File testTxt = SD.open("/wfof.json");
  //Parse into a JSON object:
  uint8_t *TxtBuffer = new uint8_t[testTxt.size() + 1];
  Serial.println("Text file size: " + String(testTxt.size()));
  testTxt.read(TxtBuffer, testTxt.size());
  TxtBuffer[testTxt.size()] = 0;  //Add a null terminator
  deserializeJson(WfofJson, TxtBuffer);
  Serial.printf("\n%s\n", TxtBuffer);

  //Fetch the array from the JSON:
  JsonArray WfofArray = WfofJson["participants"].as<JsonArray>();
  Serial.println("Found " + String(WfofArray.size()) + " participants");

  int count = 0;
  Serial.println("Full JSON");
  serializeJsonPretty(WfofJson, Serial);
  //Iterate through the JSON array and create participant objects:
  for (JsonVariant v : WfofArray) {
    serializeJson(v, Serial);
    Serial.println();
    Participant *lParticipant = new Participant(v["name"].as<std::string>(), v["images"].as<uint8_t>());
    // WfofParticipants.push_back(lParticipant);
    //Add weighted participant counts:
    for (uint8_t i = 0; i < lParticipant->imageCount; i++) {
      WfofParticipants.push_back(lParticipant);
    }
    Serial.printf("\nParticipant %s has %i images", v["name"].as<std::string>().c_str(), v["images"].as<uint8_t>());
    //count++;
  }

  //Print the list:
  for (participantIterator = WfofParticipants.begin(); participantIterator != WfofParticipants.end(); ++participantIterator) {
    Serial.printf("\nParticipant from list: %s, %i", (*participantIterator)->name.c_str(), (*participantIterator)->imageCount);
  }


  tft.setTextSize(5);
}


/***********************************************************************************************************************************/
void loop() {
  uint16_t touchX, touchY;
  bool pressed = tft.getTouch(&touchX, &touchY);
  // bool pressed = !digitalRead(4); //Inverted because of pull-up resistor
  if (pressed) {


    //Run a little "animation" before picking an actual winner:
    uint8_t loopAmount = (uint8_t)random(45, 60);
    for (uint8_t i = 0; i < loopAmount; i++) {
      tft.fillScreen(TFT_MAGENTA);
      long fakeIndex = random(WfofParticipants.size() - 1);
      participantIterator = WfofParticipants.begin();
      std::advance(participantIterator, fakeIndex);
      Participant *lWinner = *participantIterator; //Iterator is moved to index of the winner, grab their name

      int16_t txtWidth = tft.textWidth(lWinner->name.c_str());
      tft.setCursor(tft.width() / 2 - (txtWidth / 2), 100);
      tft.print(lWinner->name.c_str());
      delay(60 + (3 * i));  //Slow down over time
    }
    tft.fillScreen(TFT_MAGENTA);

    //Pick a winner:
    participantIterator = WfofParticipants.begin();
    long lWinnerIndex = random(WfofParticipants.size() - 1); //Full size would start at 1, subtract 1 after for zero indexing
    Serial.printf("\nPicked index %lu, the range was 0 - %lu", lWinnerIndex, WfofParticipants.size());
    std::advance(participantIterator, lWinnerIndex);
    Participant *lWinner = *participantIterator; //Iterator is moved to index of the winner, grab their name

    //Blink the name of the winner:
    for (uint8_t i = 0; i < 5; i++) {
      tft.fillScreen(TFT_GREEN);
      delay(100);
      int16_t txtWidth = tft.textWidth(lWinner->name.c_str());
      tft.setCursor(tft.width() / 2 - (txtWidth / 2), 100);
      tft.print(lWinner->name.c_str());
      delay(150);
    }

    //Remove all instances of the winner from the list:
    WfofParticipants.remove(lWinner);

    Serial.println("\nRemoved " + String(lWinner->name.c_str()) + " Chosen index: " + String(lWinnerIndex) + "\n");
    //Reprint the list:
    int lCounter = 0;
    for (participantIterator = WfofParticipants.begin(); participantIterator != WfofParticipants.end(); ++participantIterator) {
      Serial.printf("\n%i: Participant from list: %s, %i", lCounter, (*participantIterator)->name.c_str(), (*participantIterator)->imageCount);
      lCounter++;
    }
    delay(2000);
    tft.setCursor(30, 70);

    Serial.printf("\n\nFree heap: %lu", esp_get_free_heap_size());
  }
}

