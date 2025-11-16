#include <WiFi.h>
#include <coap-simple.h>

// ==== WiFi Credentials ====
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ==== Pin Configuration ====
#define PIR_PIN 15
#define GREEN_LED 23
#define RED_LED 22
#define BUZZER_PIN 21

// ==== Constants ====
#define ALERT_DURATION 5000   // 5 seconds

// ==== Object Initialization ====
WiFiUDP udp;
Coap coap(udp);

// ==== System State ====
struct HomeSecurityState {
  bool systemArmed;
  bool motionDetected;
  bool alarmActive;
  unsigned long alertStart;
  String statusText;
} state = { true, false, false, 0, "SAFE & MONITORING" };

// ==== Actuator Functions ====
void activateMonitoringState() {
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  state.alarmActive = false;
  state.statusText = "SAFE & MONITORING";

  Serial.println("🟢 Alert Cleared — Back to Monitoring");
}

void activateAlertMode() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);

  state.alarmActive = true;
  state.alertStart = millis();
  state.statusText = "⚠️ INTRUSION DETECTED";

  Serial.println("🔴 Motion Detected! Alert Triggered!");
}

// ==== Motion Checking ====
void checkMotion() {
  int motion = digitalRead(PIR_PIN);
  state.motionDetected = (motion == HIGH);

  if (state.systemArmed && state.motionDetected && !state.alarmActive) {
    activateAlertMode();
  }

  // Auto reset after 5 seconds
  if (state.alarmActive && millis() - state.alertStart > ALERT_DURATION) {
    activateMonitoringState();
  }
}

// ==== CoAP Endpoints ====

// /status - GET
void callback_status(CoapPacket &packet, IPAddress ip, int port) {
  char response[256];
  snprintf(response, sizeof(response),
    "{\"armed\":%s,\"motion\":%s,\"alarm\":%s,\"status\":\"%s\"}",
    state.systemArmed ? "true" : "false",
    state.motionDetected ? "true" : "false",
    state.alarmActive ? "true" : "false",
    state.statusText.c_str()
  );

  coap.sendResponse(ip, port, packet.messageid, response, strlen(response),
                    COAP_CONTENT, COAP_APPLICATION_JSON,
                    packet.token, packet.tokenlen);
}

// /arm - POST (ON/OFF)
void callback_arm(CoapPacket &packet, IPAddress ip, int port) {
  String payload = String((char*)packet.payload);
  payload.trim();

  if (payload == "ON") {
    state.systemArmed = true;
    activateMonitoringState();
    coap.sendResponse(ip, port, packet.messageid, "System Armed", 12,
                      COAP_CHANGED, COAP_TEXT_PLAIN,
                      packet.token, packet.tokenlen);
  }
  else if (payload == "OFF") {
    state.systemArmed = false;
    activateMonitoringState();
    coap.sendResponse(ip, port, packet.messageid, "System Disarmed", 14,
                      COAP_CHANGED, COAP_TEXT_PLAIN,
                      packet.token, packet.tokenlen);
  }
}

// /reset - POST
void callback_reset(CoapPacket &packet, IPAddress ip, int port) {
  activateMonitoringState();
  coap.sendResponse(ip, port, packet.messageid, "Reset OK", 8,
                    COAP_CHANGED, COAP_TEXT_PLAIN,
                    packet.token, packet.tokenlen);
}

// ==== Setup ====
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== 🏠 ESP32 Home Invasion Detection System ===");
  Serial.println("🟢 Home Alert System Active — Monitoring...");

  pinMode(PIR_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  activateMonitoringState();

  // WiFi Connection
  Serial.printf("🔌 Connecting to WiFi: %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }

  Serial.println("\n📶 WiFi Connected!");
  Serial.print("🌐 IP Address: ");
  Serial.println(WiFi.localIP());

  // CoAP Setup
  coap.server(callback_status, "status");
  coap.server(callback_arm, "arm");
  coap.server(callback_reset, "reset");
  coap.start();

  Serial.println("\n📡 CoAP Server Ready (port 5683)");
}

// ==== Main Loop ====
void loop() {
  coap.loop();

  // Check motion frequently
  checkMotion();

  // Print status every 1 second
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 1000) {
    Serial.printf("📍 Status → %s | Motion: %d | Alarm: %d\n",
                  state.statusText.c_str(),
                  state.motionDetected,
                  state.alarmActive);
    lastStatus = millis();
  }
}
