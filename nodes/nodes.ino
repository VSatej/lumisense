#include <esp_now.h>
#include <WiFi.h>

// 🔥 Define ALL nodes
uint8_t node1[6] = {0xFC,0x01,0x2C,0xEC,0xD8,0xA0};
uint8_t node2[6] = {0xFC,0x01,0x2C,0xEB,0xF5,0xB4};
uint8_t node3[6] = {0xFC,0x01,0x2C,0xEC,0x85,0x80};

// 👉 CHANGE THIS PER DEVICE (IMPORTANT)
uint8_t myNeighbors[][6] = {
  {0xFC,0x01,0x2C,0xEC,0x85,0x80}
};
int totalNeighbors = 1;

// ⏱️ 30 min interval
unsigned long lastSend = 0;
const unsigned long interval = 10000;

// 🔹 Check if sender is neighbor
bool isNeighbor(const uint8_t *mac) {
  for (int i = 0; i < totalNeighbors; i++) {
    if (memcmp(mac, myNeighbors[i], 6) == 0) return true;
  }
  return false;
}

// 🔹 Receive callback
void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  String msg = "";
  for (int i = 0; i < len; i++) msg += (char)data[i];

  Serial.print("📥 Received: ");
  Serial.println(msg);

  // Ignore non-neighbors
  if (!isNeighbor(info->src_addr)) {
    Serial.println("⛔ Ignored (not neighbor)");
    return;
  }

  // Ignore ACK loop
  if (msg.startsWith("{\"ack\"")) return;

  // Send ACK
  String reply = "{\"ack\":\"" + WiFi.macAddress() + "\"}";
  esp_now_send(info->src_addr, (uint8_t *)reply.c_str(), reply.length());

  Serial.println("✅ ACK sent");
}

// 🔹 Send callback (ESP32-C6 compatible)
void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "📡 Sent OK" : "❌ Send Fail");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW Init Failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSent);

  // Add neighbors
  for (int i = 0; i < totalNeighbors; i++) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, myNeighbors[i], 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) == ESP_OK) {
      Serial.println("✅ Peer added");
    } else {
      Serial.println("❌ Peer add failed");
    }
  }

  // Seed random generator
  randomSeed(micros());

  Serial.println("🚀 ESP-NOW Ready (Final)");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSend >= interval) {
    lastSend = currentMillis;

    // 🔥 Random intensity (0–100)
    int intensity = random(0, 101);

    // JSON message
    String msg = "{";
    msg += "\"node\":\"" + WiFi.macAddress() + "\",";
    msg += "\"intensity\":" + String(intensity);
    msg += "}";

    // Send to neighbors only
    for (int i = 0; i < totalNeighbors; i++) {
      esp_now_send(myNeighbors[i], (uint8_t *)msg.c_str(), msg.length());
    }

    Serial.print("📤 Sent JSON: ");
    Serial.println(msg);
  }

  delay(5000); // keep system responsive
}