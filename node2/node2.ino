#include <SPI.h>
#include <LoRa.h>
#include "DHT.h"

#define pingPin (16)
#define ss 5
#define rst 21
#define dio0 2

String Incoming = "";
String Message = "";

byte LocalAddress = 0x03;
byte Destination_Master = 0x01;

float binLevel = 0;

unsigned long previousMillis = 0;
const long interval = 2000;

void sendMessage(String Outgoing, byte Destination) {
  LoRa.beginPacket();
  LoRa.write(Destination);
  LoRa.write(LocalAddress);
  LoRa.write(Outgoing.length());
  LoRa.print(Outgoing);
  LoRa.endPacket();
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
  if (Incoming == "SDS2") {
    Message = "SL2," + String(binLevel);

    Serial.println();
    Serial.print("Send message to Master : ");
    Serial.println(Message);

    sendMessage(Message, Destination_Master);
  }
}

void setup() {
  Serial.begin(115200);

  LoRa.setPins(ss, rst, dio0);
LoRa.

  Serial.println();
  Serial.println("Start LoRa init...");
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;  // if failed, do nothing
  }
  Serial.println("LoRa init succeeded.");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    binLevel = mapFloat(readUltrasonic(), 0, 84.0, 100.0, 0);
  }
  onReceive(LoRa.parsePacket());
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
