#include <SPI.h>
#include <LoRa.h>

#define ss 5
#define rst 21
#define dio0 2

String Incoming = "";
String Message = "";

byte LocalAddress = 0x01;
byte Destination_ESP32_Node_1 = 0x02;
byte Destination_ESP32_Node_2 = 0x03;

unsigned long previousMillis = 0;
const long interval = 1000;

byte node = 0;

unsigned long packetsSentToNode1 = 0;
unsigned long packetsReceivedFromNode1 = 0;
unsigned long packetsSentToNode2 = 0;
unsigned long packetsReceivedFromNode2 = 0;

void incrementSentPackets(String node) {
  if (node == "SL1") {
    packetsSentToNode1++;
  } else if (node == "SL2") {
    packetsSentToNode2++;
  }
}

// Function to increment received packet counters
void incrementReceivedPackets(String node) {
  if (node == "SL1") {
    packetsReceivedFromNode1++;
  } else if (node == "SL2") {
    packetsReceivedFromNode2++;
  }
}


void sendMessage(String Outgoing, byte Destination) {
  LoRa.beginPacket();
  LoRa.write(Destination);
  LoRa.write(LocalAddress);
  LoRa.write(Outgoing.length());
  LoRa.print(Outgoing);
  LoRa.endPacket();

  if (Destination == Destination_ESP32_Node_1) {
    incrementSentPackets("SL1");
  } else if (Destination == Destination_ESP32_Node_2) {
    incrementSentPackets("SL2");
  }
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
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));

  String nodeIdentifier = Incoming.substring(0, Incoming.indexOf(','));

  // Increment the received packet counter for the appropriate node
  if (nodeIdentifier == "SL1") {
    packetsReceivedFromNode1++;
  } else if (nodeIdentifier == "SL2") {
    packetsReceivedFromNode2++;
  }
  //----------------------------------------
}

void displayPDR() {
  float pdrNode1 = (float)packetsReceivedFromNode1 / packetsSentToNode1 * 100;
  float pdrNode2 = (float)packetsReceivedFromNode2 / packetsSentToNode2 * 100;

  Serial.print("PDR for Node 1: ");
  Serial.print(pdrNode1);
  Serial.println("%");

  Serial.print("PDR for Node 2: ");
  Serial.print(pdrNode2);
  Serial.println("%");
}

void setup() {
  Serial.begin(115200);

  LoRa.setPins(ss, rst, dio0);

  Serial.println("Start LoRa init...");
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;  // if failed, do nothing
  }
  Serial.println("LoRa init succeeded.");
  //----------------------------------------
}

void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    node++;
    if (node > 2) node = 1;

    Message = "SDS" + String(node);

    if (node == 1) {
      Serial.println();
      Serial.print("Send message to ESP32 Node " + String(node));
      Serial.println(" : " + Message);
      sendMessage(Message, Destination_ESP32_Node_1);
    }

    if (node == 2) {
      Serial.println();
      Serial.print("Send message to ESP32 Node " + String(node));
      Serial.println(" : " + Message);
      sendMessage(Message, Destination_ESP32_Node_2);
    }

    if (packetsSentToNode1 >= 40 || packetsSentToNode2 >= 40) {
      displayPDR();
    })
  }

  onReceive(LoRa.parsePacket());
}