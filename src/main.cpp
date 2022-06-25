#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>

#include <stdlib.h>
// #include <bits/stdc++.h>
#include <algorithm>      //for std::shuffle
#include <list>
#include <ArduinoJson.h>
// #include <TFT_eSPI.h> // Hardware-specific library

#include <Adafruit_SSD1306.h>

#define USE_SPIFFS

int16_t h;
int16_t w;

uint32_t seed;

#define BUTTONPIN 13

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

void scrollText(String text) {
  display.setTextSize(4);
  display.setTextWrap(false);
  int16_t x, y;
  uint16_t StringWidth, StringHeight;
  display.getTextBounds(text, 0, 0, &x, &y, &StringWidth, &StringHeight);
  for (int16_t i = SCREEN_WIDTH; i > (-StringWidth); i--) {
    display.clearDisplay();
    display.setCursor(i, 0);
    display.print(text);
    display.display();
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void pickWinnerAnimation() {
  for (uint8_t i = 0; i < 10; i++) {
    display.fillScreen(SSD1306_WHITE);
    display.display();
    delay(200);
    display.clearDisplay();
    display.display();
    delay(200);
  }
}

TaskHandle_t LoopTaskHandler;
void screenLoopTask(void *args) {
  String *theText = (String *)args;
  for (;;) {
    scrollText(*theText);
  }
}

SPIClass SdSPI;
StaticJsonDocument<8192> WfofJson;

std::list<Participant *> WfofParticipants;
std::list<Participant *>::iterator participantIterator;

/***********************************************************************************************************************************/
void setup() {
  //Set up serial port:
  Serial.begin(115200);

  pinMode(BUTTONPIN, INPUT_PULLUP);
  //Set up the display:

  Wire.begin(21, 22);
  //Scan i2c bus:
  for (uint8_t i = 0; i < 128; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) { //If endtransmission gives no error, a device was detected
      Serial.printf("Found device at address %#2X", i);
    }
  }
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.display();
  delay(500);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.print("Hello World");
  display.display();


  //Initialise the random number generator:
  WiFi.mode(WIFI_STA);  //set up Wi-Fi to get true random generation
  randomSeed(esp_random());
  seed = esp_random();
  Serial.println("Initialised with random seed:" + String(seed));
  WiFi.mode(WIFI_OFF);

  //Init the SD card:
  SdSPI.begin(18, 19, 23, 9);
#ifndef USE_SPIFFS
  if (!SD.begin(9, SdSPI)) {
    ESP_LOGE("SD", "Could not load SD card");
  }

  //Open the JSON file:
  File testTxt = SD.open("/wfof.json");
#else
  if (!SPIFFS.begin()) {
    ESP_LOGE("SD", "Could not open SPIFFS");
  }

  File testTxt = SPIFFS.open("/wfof.json");
#endif
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


}


/***********************************************************************************************************************************/
void loop() {
  uint16_t touchX, touchY;
  bool taskRunning = false;
  while (1) {
    bool pressed = !digitalRead(BUTTONPIN); //Inverted because of pull-up resistor
    if (pressed) {
      //Stop running the Display:
      if (taskRunning) {
        vTaskDelete(LoopTaskHandler);
        delay(200);
        Serial.println("Stopping current loop");
      }
      display.clearDisplay();
      display.display();

      //Pick a winner:
      participantIterator = WfofParticipants.begin();
      long lWinnerIndex = random(WfofParticipants.size() - 1); //Full size would start at 1, subtract 1 after for zero indexing
      Serial.printf("\nPicked index %lu, the range was 0 - %lu", lWinnerIndex, WfofParticipants.size());
      std::advance(participantIterator, lWinnerIndex);
      Participant *lWinner = *participantIterator; //Iterator is moved to index of the winner, grab their name

      String *winnerName = new String(lWinner->name.c_str());
      pickWinnerAnimation();
      taskRunning = true;
      xTaskCreatePinnedToCore(screenLoopTask, "DisplayWinner", 8192, (void *)winnerName, 5, &LoopTaskHandler, 0);

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

      Serial.printf("\n\nFree heap: %lu", esp_get_free_heap_size());
    }
  }
}

