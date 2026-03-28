#include <esp_now.h>
#include <WiFi.h>

// 🔥 Define ALL nodes
uint8_t node1[6] = {0xFC,0x01,0x2C,0xEC,0xD8,0xA0};
uint8_t node2[6] = {0xFC,0x01,0x2C,0xEB,0xF5,0xB4};
uint8_t node3[6] = {0xFC,0x01,0x2C,0xEC,0x85,0x80};

// 👉 CHANGE THIS PER DEVICE
uint8_t myNeighbors[][6] = {
  {0xFC,0x01,0x2C,0xEC,0xD8,0xA0}  // example: only Node 1
};
int totalNeighbors = 1;

// 🔹 Check if sender is neighbor
bool isNeighbor(const uint8_t *mac) {
  for (int i = 0; i < totalNeighbors; i++) {
    if (memcmp(mac, myNeighbors[i], 6) == 0) return true;
  }
  return false;
}

// 🔹 Receive
void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  String msg = "";
  for (int i = 0; i < len; i++) msg += (char)data[i];

  Serial.print("📥 Received: ");
  Serial.println(msg);

  // ❌ Ignore non-neighbors
  if (!isNeighbor(info->src_addr)) {
    Serial.println("⛔ Ignored (not neighbor)");
    return;
  }

  // ❌ Ignore ACK loop
  if (msg.startsWith("ACK")) return;

  // ✅ Only neighbor responds
  String reply = "ACK from " + WiFi.macAddress();
  esp_now_send(info->src_addr, (uint8_t *)reply.c_str(), reply.length());

  Serial.println("✅ Neighbor reply sent");
}

// 🔹 Send callback
void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "📡 Sent OK" : "❌ Send Fail");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  esp_now_init();

  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSent);

  // Add neighbors only
  for (int i = 0; i < totalNeighbors; i++) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, myNeighbors[i], 6);
    esp_now_add_peer(&peer);
  }

  Serial.println("🚀 Ready (Neighbor Mode)");
}

void loop() {
  String msg = "MOTION from " + WiFi.macAddress();

  // Send only to neighbors
  for (int i = 0; i < totalNeighbors; i++) {
    esp_now_send(myNeighbors[i], (uint8_t *)msg.c_str(), msg.length());
  }

  Serial.println("📤 Sent to neighbors");

  delay(5000);
}