#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <queue>

std::queue<String> httpQueue;

#define ss 5
#define rst 21
#define dio0 2

const char* ssid = "scam ni";
const char* password = "Walakokabalo0123!";
const char* webAppUrl = "https://script.google.com/macros/s/AKfycbyHnenfG2XHthIcA08X5MD8TXiF1ikFGyslCP_g510UhO2tWFdExeLSA_jp6FFWiDMSGQ/exec";  // The URL you got from the script deployment
WiFiClient client;

String Incoming = "";
String Message = "";

byte LocalAddress = 0x01;
byte Destination_ESP32_Node_1 = 0x02;
byte Destination_ESP32_Node_2 = 0x03;

struct NodeInfo {
  unsigned long lastPacketNumber = 0;
  String lastConfig = "";
  unsigned long packetsReceived = 0;
  unsigned long totalToA = 0;
  float totalRssi = 0;
  float totalSnr = 0;
};

NodeInfo node1Info, node2Info;

unsigned long previousMillis = 0;
const long interval = 1000;

byte node = 0;

unsigned long packetsSentToNode1 = 0;
unsigned long packetsReceivedFromNode1 = 0;
unsigned long packetsSentToNode2 = 0;
unsigned long packetsReceivedFromNode2 = 0;

bool isDigitMinusOrDot(char c) {
  return isdigit(c) || c == '-' || c == '.';
}

bool isValidData(const String& data) {
  // Node
  int nodeIndex = data.indexOf("\"node\":\"") + 8;
  if (data.substring(nodeIndex, nodeIndex + 2) != "SL" || !isdigit(data[nodeIndex + 2])) {
    Serial.println("Validation failed at Node");
    return false;
  }

  // binLevel (floating-point number)
  int binLevelIndex = data.indexOf("\"binLevel\":") + 11;
  int nextCommaIndex = data.indexOf(',', binLevelIndex);
  String binLevelValue = data.substring(binLevelIndex, nextCommaIndex);
  for (char c : binLevelValue) {
    if (!isDigitMinusOrDot(c)) {
      Serial.println("Validation failed at binLevel");
      return false;
    }
  }

  // toa (assuming it's numeric)
  int toaIndex = data.indexOf("\"toa\":") + 6;
  nextCommaIndex = data.indexOf(',', toaIndex);
  String toaValue = data.substring(toaIndex, nextCommaIndex);
  for (char c : toaValue) {
    if (!isdigit(c)) {
      Serial.println("Validation failed at toa");
      return false;
    }
  }

  // packet (assuming it's numeric)
  int packetIndex = data.indexOf("\"packet\":") + 9;
  nextCommaIndex = data.indexOf(',', packetIndex);
  String packetValue = data.substring(packetIndex, nextCommaIndex);
  for (char c : packetValue) {
    if (!isdigit(c)) {
      Serial.println("Validation failed at packet");
      return false;
    }
  }

  // config (alphanumeric check)
  int configIndex = data.indexOf("\"config\":\"") + 10;
  int configEndIndex = data.indexOf('"', configIndex);
  String configValue = data.substring(configIndex, configEndIndex);
  for (char c : configValue) {
    if (!isalnum(c)) {
      Serial.println("Validation failed at config");
      return false;
    }
  }

  // rssi (numeric, including negative)
  int rssiIndex = data.indexOf("\"rssi\":") + 7;
  int rssiEndIndex = data.indexOf(',', rssiIndex);
  String rssiValue = data.substring(rssiIndex, rssiEndIndex);
  for (char c : rssiValue) {
    if (!isDigitMinusOrDot(c)) {
      Serial.println("Validation failed at rssi");
      return false;
    }
  }

  // snr (numeric, including negative)
  int snrIndex = data.indexOf("\"snr\":") + 6;
  int snrEndIndex = data.indexOf('}', snrIndex);       // Adjusted to find the end of the object
  if (snrEndIndex == -1) snrEndIndex = data.length();  // In case '}' is not found
  String snrValue = data.substring(snrIndex, snrEndIndex);
  for (char c : snrValue) {
    if (!isDigitMinusOrDot(c)) {
      Serial.println("Validation failed at snr");
      return false;
    }
  }

  Serial.println("Data is valid");
  return true;  // All checks passed, data is valid
}


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

  processIncomingMessage(sender, Incoming);

  // String nodeIdentifier = Incoming.substring(0, Incoming.indexOf(','));

  // // Increment the received packet counter for the appropriate node
  // if (nodeIdentifier == "SL1") {
  //   packetsReceivedFromNode1++;
  // } else if (nodeIdentifier == "SL2") {
  //   packetsReceivedFromNode2++;
  // }

  // unsigned long receiveTime = millis();
  // unsigned long sendTime = Incoming.substring(2, Incoming.indexOf(',')).toInt();
  // unsigned long timeOfArrival = receiveTime - sendTime;

  // Serial.print("Time of Arrival: ");
  // Serial.println(timeOfArrival);
  // //----------------------------------------
}

void processIncomingMessage(byte sender, String message) {
  // Parse the message
  // Example format: SL1,binLevel,sendTime,Packet #,Config
  // Add parsing logic here...

  // Determine which node sent the message
  NodeInfo* nodeInfo = (sender == 0x02) ? &node1Info : &node2Info;

  int firstCommaIndex = message.indexOf(',');
  String nodeIdentifier = message.substring(0, firstCommaIndex);

  // if (nodeIdentifier != "SL1" || nodeIdentifier != "SL2") {
  //   return;
  // }

  // Extract binLevel
  int secondCommaIndex = message.indexOf(',', firstCommaIndex + 1);
  float binLevel = message.substring(firstCommaIndex + 1, secondCommaIndex).toFloat();

  // Extract sendTime
  int thirdCommaIndex = message.indexOf(',', secondCommaIndex + 1);
  unsigned long sendTime = message.substring(secondCommaIndex + 1, thirdCommaIndex).toInt();

  // Extract packetNumber
  int fourthCommaIndex = message.indexOf(',', thirdCommaIndex + 1);
  unsigned long packetNumber = message.substring(thirdCommaIndex + 1, fourthCommaIndex).toInt();

  // Extract config
  String config = message.substring(fourthCommaIndex + 1);

  // Calculate ToA
  unsigned long receiveTime = millis();
  unsigned long timeOfArrival = receiveTime - sendTime;
  nodeInfo->totalToA += timeOfArrival;

  // Update RSSI and SNR totals
  nodeInfo->totalRssi += LoRa.packetRssi();
  nodeInfo->totalSnr += LoRa.packetSnr();

  // Check for config change and packet count
  if (config != nodeInfo->lastConfig || packetNumber < nodeInfo->lastPacketNumber) {
    // New config or packet sequence restarted
    nodeInfo->packetsReceived = 1;
    nodeInfo->lastConfig = config;
  } else {
    nodeInfo->packetsReceived++;
  }
  nodeInfo->lastPacketNumber = packetNumber;

  // Print information (for debugging, can be removed later)
  Serial.println("Received message from node: " + String(sender == 0x02 ? "1" : "2"));
  Serial.println("Bin Level: " + String(binLevel));
  Serial.println("Time of Arrival: " + String(timeOfArrival));
  Serial.println("Packet Number: " + String(packetNumber));
  Serial.println("Config: " + config);
  Serial.println("HERE");
  String jsonData = "{\"node\":\"" + String(nodeIdentifier) + "\",\"binLevel\":" + String(binLevel) + ",\"toa\":" + String(timeOfArrival) + ",\"packet\":" + String(packetNumber) + ",\"config\":\"" + config + "\",\"rssi\":" + String(LoRa.packetRssi()) + ",\"snr\":" + String(LoRa.packetSnr()) + "}";
  if (isValidData(jsonData)) {
    Serial.println("Adding to queue: " + jsonData);
    httpQueue.push(jsonData);
    Serial.println("Queued data for HTTP POST: " + jsonData);
  } else {
    Serial.println("Invalid data detected, not queuing: " + jsonData);
  }
}

void httpPostTask(void* parameter) {
  Serial.println("HTTP Post Task Running");
  for (;;) {
    if (!httpQueue.empty()) {
      String jsonData = httpQueue.front();
      httpQueue.pop();

      Serial.println("Sending data: " + jsonData);  // Debug print

      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(webAppUrl);
        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.POST(jsonData);

        if (httpResponseCode > 0) {
          String response = http.getString();
          Serial.println(httpResponseCode);
          Serial.println(response);
        } else {
          Serial.println("Error on sending POST: " + String(httpResponseCode));
        }
        http.end();
      } else {
        Serial.println("WiFi not connected");
      }
    }
    vTaskDelay(300 / portTICK_PERIOD_MS);  // Delay to prevent task from using 100% CPU
  }
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

  WiFi.setAutoReconnect(true);

  WiFi.begin(ssid, password);
  Serial.println("");

  //Wait for WIFI connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Gateway IP address: ");
  Serial.println(WiFi.gatewayIP());

  LoRa.setPins(ss, rst, dio0);

  Serial.println("Start LoRa init...");
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;  // if failed, do nothing
  }
  Serial.println("LoRa init succeeded.");
  xTaskCreate(
    httpPostTask,   /* Task function */
    "HTTPPostTask", /* Name of task */
    10000,          /* Stack size of task */
    NULL,           /* Parameter of the task */
    1,              /* Priority of the task */
    NULL);          /* Task handle to keep track of created task */
  Serial.println("HTTP Post Task Created");
  //----------------------------------------
}

void loop() {
  // unsigned long currentMillis = millis();

  // if (currentMillis - previousMillis >= interval) {
  //   previousMillis = currentMillis;

  //   node++;
  //   if (node > 2) node = 1;

  //   Message = "SDS" + String(node);

  //   if (node == 1) {
  //     Serial.println();
  //     Serial.print("Send message to ESP32 Node " + String(node));
  //     Serial.println(" : " + Message);
  //     sendMessage(Message, Destination_ESP32_Node_1);
  //   }

  //   if (node == 2) {
  //     Serial.println();
  //     Serial.print("Send message to ESP32 Node " + String(node));
  //     Serial.println(" : " + Message);
  //     sendMessage(Message, Destination_ESP32_Node_2);
  //   }

  //   if (packetsSentToNode1 >= 40 || packetsSentToNode2 >= 40) {
  //     displayPDR();
  //   }
  // }
  onReceive(LoRa.parsePacket());
  // delay(200);
}