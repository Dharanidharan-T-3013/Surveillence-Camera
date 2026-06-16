

#define BLYNK_TEMPLATE_ID "TMPL30aCRmF_S"
#define BLYNK_TEMPLATE_NAME "SurveillanceCamera"
#define BLYNK_AUTH_TOKEN "hi3XBRroSLBIhcytLI5ephEd7rjYTucE"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// -----------------------------
// PIN CONFIGURATION
// -----------------------------
#define LED_PIN 2
#define GSM_RX 16
#define GSM_TX 17
#define GPS_RX 5
#define GPS_TX 4

// -----------------------------
// HARDWARE SERIAL SETUP
// -----------------------------
HardwareSerial gsmSerial(1);
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// -----------------------------
// WIFI + BLYNK CREDENTIALS
// -----------------------------
char ssid[] = "sam";
char pass[] = "11111111";

// -----------------------------
// PHONE NUMBER TO RECEIVE ALERTS
// -----------------------------
String phoneNumber = "+918667247687";

BlynkTimer timer;

// -----------------------------
// NORMAL RESET TIMER
// -----------------------------
unsigned long lastAlertTime = 0;
const unsigned long normalTimeout = 7000;  // 7 sec auto-reset

// -----------------------------
// FUNCTION DECLARATIONS
// -----------------------------
void gsmInit();
void sendSMS(String alertType);
void handleEvent(String alertType);
void handleNormal();
void checkConnections();


// -----------------------------
// SETUP
// -----------------------------
void setup() {
  Serial.begin(115200);
  gsmSerial.begin(115200, SERIAL_8N1, GSM_RX, GSM_TX);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n--------------------------------");
  Serial.println(" ESP32 Accident & Robbery Alert System");
  Serial.println("--------------------------------");

  // Connect WiFi
  Serial.print(" Connecting to WiFi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n WiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Connect Blynk
  Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);
  if (Blynk.connect()) {
    Serial.println(" Connected to Blynk Cloud!");
  } else {
    Serial.println(" Blynk Connection Failed!");
  }

  gsmInit();
  handleNormal();  // Initial state

  timer.setInterval(5000L, checkConnections);
}


// -----------------------------
// GSM INITIALIZATION
// -----------------------------
void gsmInit() {
  Serial.println(" Initializing GSM...");
  gsmSerial.println("AT");
  delay(1000);
  gsmSerial.println("AT+CMGF=1");
  delay(1000);
  Serial.println(" GSM Ready!");
}


// -----------------------------
// SEND SMS WITH GPS LOCATION
// -----------------------------
void sendSMS(String alertType) {
  String smsContent = "Alert: " + alertType;

  // Read GPS data
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  if (gps.location.isValid()) {
    smsContent += "\nLocation: https://maps.google.com/?q=";
    smsContent += String(gps.location.lat(), 6);
    smsContent += ",";
    smsContent += String(gps.location.lng(), 6);
  } else {
    smsContent += "\nLocation: Not available";
  }

  gsmSerial.println("AT+CMGF=1");
  delay(500);
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(500);
  gsmSerial.print(smsContent);
  delay(500);
  gsmSerial.write(26);  // Ctrl+Z
  delay(2000);

  Serial.println(" SMS Sent: " + smsContent);
}


// -----------------------------
// HANDLE ALERT (Accident / Robbery)
// -----------------------------
void handleEvent(String alertType) {
  lastAlertTime = millis();
  Serial.println("⚠️ " + alertType + " Detected!");

  // Blink LED
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }

  sendSMS(alertType);

  // Update Blynk Cloud
  if (alertType == "Accident") {
    Blynk.virtualWrite(V1, "🚨 Accident Detected!");
    Blynk.logEvent("accident_alert", "Accident Detected!");
  } 
  else if (alertType == "Robbery") {
    Blynk.virtualWrite(V1, "🚨 Robbery Detected!");
    Blynk.logEvent("robbery_alert", "Robbery Detected!");
  }

  Serial.println(" Alert updated to Blynk!");
}


// -----------------------------
// HANDLE NORMAL CONDITION
// -----------------------------
void handleNormal() {
  Serial.println(" Normal condition");
  digitalWrite(LED_PIN, LOW);
  Blynk.virtualWrite(V1, " Normal Condition");
}


// -----------------------------
// CHECK CONNECTIONS + AUTO NORMAL RESET
// -----------------------------
void checkConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" WiFi Reconnecting...");
    WiFi.begin(ssid, pass);
  }

  if (!Blynk.connected()) {
    Serial.println(" Blynk Reconnecting...");
    Blynk.connect();
  }

  // Auto-normal reset
  if (millis() - lastAlertTime > normalTimeout) {
    handleNormal();
    lastAlertTime = millis();
  }
}


// -----------------------------
// MAIN LOOP
// -----------------------------
void loop() {
  Blynk.run();
  timer.run();

  // Read input from Python
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();

    Serial.println(" Received: " + msg);

    // Event triggers
    if (msg == "accident") handleEvent("Accident");
    else if (msg == "robbery") handleEvent("Robbery");
    else if (msg == "normal") handleNormal();

    // SMS-only triggers from Python
    else if (msg == "accident_sms") {
      Serial.println(" Sending Accident SMS...");
      sendSMS("Accident");
    }
    else if (msg == "robbery_sms") {
      Serial.println(" Sending Robbery SMS...");
      sendSMS("Robbery");
    }
  }

  // GPS reading
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}
