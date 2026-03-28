#include <esp_now.h>
#include <WiFi.h>

// 🔥 CONFIGURE PER NODE (CHANGE MACs)
uint8_t myNeighbors[][6] = {
  {0xFC,0x01,0x2C,0xEB,0xC2,0x40},
  {0xFC,0x01,0x2C,0xEC,0x85,0x80}
};
int totalNeighbors = 2;

// 🔹 PINS
#define LED_PIN 2
#define PIR_PIN 16

// 🔹 PWM (ESP32-C6)
int brightness = 0;
bool ledOn = false;
unsigned long ledStart = 0;

// 🔹 PIR
bool lastMotionState = LOW;

// 🔹 DUPLICATE HANDLING
#define MAX_MSG_HISTORY 10
String msgHistory[MAX_MSG_HISTORY];
int msgIndex = 0;

bool isDuplicate(String id) {
  for (int i = 0; i < MAX_MSG_HISTORY; i++) {
    if (msgHistory[i] == id) return true;
  }
  return false;
}

void storeMsg(String id) {
  msgHistory[msgIndex] = id;
  msgIndex = (msgIndex + 1) % MAX_MSG_HISTORY;
}

// 🔹 Extract TTL
int getTTL(String msg) {
  int i = msg.indexOf("\"ttl\":");
  if (i == -1) return 0;
  return msg.substring(i + 6).toInt();
}

// 🔹 Extract ID
String getID(String msg) {
  int start = msg.indexOf("\"id\":\"");
  if (start == -1) return "";
  start += 6;
  int end = msg.indexOf("\"", start);
  return msg.substring(start, end);
}

// 🔹 RECEIVE
void onReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {

  String msg = "";
  for (int i = 0; i < len; i++) msg += (char)data[i];

  Serial.println("📥 " + msg);

  String msgID = getID(msg);

  // 🔥 FIXED duplicate handling
  if (isDuplicate(msgID)) return;
  storeMsg(msgID);

  // 🔥 TURN ON LED
  brightness = 255;
  ledcWrite(LED_PIN, brightness);
  ledOn = true;
  ledStart = millis();

  int ttl = getTTL(msg);

  if (ttl > 0) {
    ttl--;

    String forwardMsg = "{";
    forwardMsg += "\"id\":\"" + msgID + "\",";
    forwardMsg += "\"node\":\"" + WiFi.macAddress() + "\",";
    forwardMsg += "\"motion\":1,";
    forwardMsg += "\"ttl\":" + String(ttl);
    forwardMsg += "}";

    // 🔥 Slight delay to reduce collisions
    delay(random(10, 50));

    for (int i = 0; i < totalNeighbors; i++) {
      if (memcmp(myNeighbors[i], info->src_addr, 6) != 0) {
        esp_now_send(myNeighbors[i], (uint8_t *)forwardMsg.c_str(), forwardMsg.length());
      }
    }

    Serial.println("📤 Forward TTL=" + String(ttl));
  }
}

// 🔹 SEND STATUS
void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "📡 Sent OK" : "❌ Send Fail");
}

// 🔹 SEND MOTION
void sendMotion() {
  String msgID = String(millis());

  String msg = "{";
  msg += "\"id\":\"" + msgID + "\",";
  msg += "\"node\":\"" + WiFi.macAddress() + "\",";
  msg += "\"motion\":1,";
  msg += "\"ttl\":2";
  msg += "}";

  for (int i = 0; i < totalNeighbors; i++) {
    esp_now_send(myNeighbors[i], (uint8_t *)msg.c_str(), msg.length());
  }

  Serial.println("🚨 Motion Sent ID=" + msgID);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIR_PIN, INPUT);

  // ✅ PWM (ESP32-C6 FIX)
  ledcAttach(LED_PIN, 5000, 8);

  // 🔥 CRITICAL
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(1);

  Serial.print("My MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW Init Failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);
  esp_now_register_send_cb(onSent);

  // 🔥 Add peers
  for (int i = 0; i < totalNeighbors; i++) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, myNeighbors[i], 6);
    peer.channel = 1;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) == ESP_OK) {
      Serial.println("✅ Peer added");
    } else {
      Serial.println("❌ Peer add failed");
    }
  }

  Serial.println("🚀 LumiSense READY");
}

void loop() {
  int motion = digitalRead(PIR_PIN);

  // 🔥 ONLY send on motion start
  if (motion == HIGH && lastMotionState == LOW) {
    Serial.println("🚨 Motion detected!");
    sendMotion();
  }

  lastMotionState = motion;

  // 🔥 Smooth dimming (30 sec)
  if (ledOn) {
    unsigned long elapsed = millis() - ledStart;

    if (elapsed < 30000) {
      brightness = map(elapsed, 0, 30000, 255, 0);
      ledcWrite(LED_PIN, brightness);
    } else {
      ledcWrite(LED_PIN, 0);
      ledOn = false;
      Serial.println("💡 OFF");
    }
  }

  delay(10);
}
