#include <SPI.h>
#include <LoRa.h>
#include "enums.h"

#define pingPin (16)
#define ss 5
#define rst 21
#define dio0 2

String Incoming = "";
String Message = "";

byte LocalAddress = 0x02;        //--> address of this device (Slave 1).
byte Destination_Master = 0x01;  //--> destination to send to Master (ESP32).

float binLevel = 0;

unsigned long previousMillis = 0;
const long interval = 2000;

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

void onReceive(int packetSize) {
  if (packetSize == 0) return;

  int recipient = LoRa.read();
  byte sender = LoRa.read();
  byte incomingLength = LoRa.read();

  Incoming = "";

  while (LoRa.available()) {
    Incoming += (char)LoRa.read();
  }

  if (incomingLength != Incoming.length()) {
    Serial.println("error: message length does not match length");
    return;
  }

  if (recipient != LocalAddress) {
    Serial.println("This message is not for me.");
    return;
  }

  Serial.println();
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + Incoming);
  //Serial.println("RSSI: " + String(LoRa.packetRssi()));
  //Serial.println("Snr: " + String(LoRa.packetSnr()));
  //----------------------------------------
  Processing_incoming_data();
}

void Processing_incoming_data() {

  if (Incoming == "SDS1") {

    unsigned long sendTime = millis();

    Message = "SL1," + String(binLevel) + "," + String(sendTime);

    Serial.println();
    Serial.print("Send message to Master : ");
    Serial.println(Message);

    sendMessage(Message, Destination_Master);
  }
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

  LoRa.setPins(ss, rst, dio0);

  Serial.println();
  Serial.println("Start LoRa init...");
  applyConfig(currentGroup, currentConfigIndex);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    binLevel = mapFloat(readUltrasonic(), 0, 84.0, 100.0, 0);

    unsigned long sendTime = millis();
    String additionalInfo = "SL1," + String(binLevel) + "," + String(sendTime);

    Message = additionalInfo + "," + String(packetCounter) + "," + currentGroup + String(currentConfigIndex);
    sendMessage(Message, Destination_Master);

    packetCounter++;
    sentCounter++;

    if (packetCounter >= 40) {
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
