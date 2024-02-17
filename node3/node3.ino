#include <SPI.h>
#include <LoRa.h>
#include "RTClib.h"
#include "enums.h"

#define pingPin (16)
#define ss 5
#define rst 4
#define dio0 2

String Incoming = "";
String Message = "";

byte LocalAddress = 0x04;        //--> address of this device (Slave 1).
byte Destination_Master = 0x01;  //--> destination to send to Master (ESP32).

RTC_DS3231 rtc;

float binLevel = 0;

unsigned long previousMillis = 0;
const long interval = 5000;

int packetCounter = 0;
int sentCounter = 0;
int ackCounter = 0;
char currentGroup = 'A';
int currentConfigIndex = 0;

void sendMessage(String Outgoing, byte Destination) {

  LoRa.beginPacket();             //--> start packet
  LoRa.write(Destination);        //--> add destination address
  LoRa.write(LocalAddress);       //--> add sender address
  LoRa.write(Outgoing.length());  //--> add payload length
  LoRa.print(Outgoing);           //--> add payload
  LoRa.endPacket();               //--> finish packet and send it
}

void applyConfig(char group, int index) {
  if (configs.find(group) != configs.end() && index < configs[group].size()) {
    LoRa.end();

    // Retrieve the configuration
    LoRaConfig config = configs[group][index];

    // Apply the configuration
    LoRa.setSignalBandwidth(config.bandwidth);
    LoRa.setCodingRate4(config.codingRateDenominator);
    LoRa.setSpreadingFactor(config.spreadingFactor);

    if (!LoRa.begin(433E6)) {
      Serial.println("LoRa init failed. Check your connections.");
      while (true)
        ;  // if failed, do nothing
    }
    Serial.println("LoRa reinitialized with new settings.");
  } else {
    // Handle the error: the group or index is out of bounds
    Serial.println("Error: Configuration not found.");
  }
}

char getNextGroup(char currentGroup) {
  if (currentGroup == 'A') return 'B';
  else if (currentGroup == 'B') return 'C';
  else return 'A';
}

void setup() {
  Serial.begin(115200);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  LoRa.setPins(ss, rst, dio0);

  Serial.println();
  Serial.println("Start LoRa init...");
  applyConfig(currentGroup, currentConfigIndex);
}

void loop() {
  unsigned long currentMillis = millis();

  DateTime now = rtc.now();

  uint32_t sendTime = now.hour() * 3600 + now.minute() * 60 + now.second();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    binLevel = mapFloat(readUltrasonic(), 0, 84.0, 100.0, 0);

    String additionalInfo = "SL3," + String(binLevel) + "," + String(sendTime);

    Message = additionalInfo + "," + String(packetCounter) + "," + currentGroup + String(currentConfigIndex);
    Serial.println(Message);
    sendMessage(Message, Destination_Master);

    packetCounter++;
    sentCounter++;

    if (packetCounter >= 30) {
      packetCounter = 0;
      currentConfigIndex++;

      if (currentConfigIndex >= configs[currentGroup].size()) {
        currentConfigIndex = 0;
        currentGroup = getNextGroup(currentGroup);
      }

      applyConfig(currentGroup, currentConfigIndex);
    }
  }
}

double readUltrasonic() {
  long duration, inches, cm;

  pinMode(pingPin, OUTPUT);
  digitalWrite(pingPin, LOW);
  delayMicroseconds(2);
  digitalWrite(pingPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(pingPin, LOW);

  pinMode(pingPin, INPUT);
  duration = pulseIn(pingPin, HIGH);

  cm = microsecondsToCentimeters(duration);
  if (cm >= 1200) cm = 0;

  return cm > 84.0 ? 84.0 : cm;
}

long microsecondsToCentimeters(long microseconds) {
  return microseconds / 29 / 2;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
