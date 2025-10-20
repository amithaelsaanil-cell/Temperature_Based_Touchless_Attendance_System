#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <SPI.h>
#include <MFRC522.h>

// ==== RFID Pins ====
#define SS_PIN D4
#define RST_PIN D0

MFRC522 rfid(SS_PIN, RST_PIN);

// ==== Ultrasonic Pins ====
#define TRIG_PIN D3
#define ECHO_PIN D8

// ==== MLX90614 Sensor ====
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// ==== WiFi & ThingSpeak ====
const char* ssid = "abcg_4g";
const char* password = "12345678";
unsigned long myChannelNumber = 3115672;
const char* myWriteAPIKey = "J36UX92LN50PR4S1";
WiFiClient client;

// ==== Function to match UID to Name ====
String getName(String uid) {
  if (uid == "2925AB00") return "Alice";
  if (uid == "3B10AB00") return "Bob";
  return "Unknown";
}

// ==== Setup ====
void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  mlx.begin();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  ThingSpeak.begin(client);

  Serial.println("System Ready. Waiting for person...");
}

// ==== Send data to IFTTT ====
void sendToIFTTT(String uid, double temp, String status) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://maker.ifttt.com/trigger/Attendance_update/json/with/key/nopaIupHSnt7uehhtdN7Lzs5zNpVPkpf2rU_cF0SdPX";
    url += "?value1=" + uid;
    url += "&value2=" + String(temp, 2);
    url += "&value3=" + status;

    http.begin(client, url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("✅ Data sent to Google Sheets via IFTTT!");
    } else {
      Serial.println("❌ Error sending to IFTTT: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

// ==== Main Loop ====
void loop() {
  // Step 1: Detect person using ultrasonic
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance = duration * 0.034 / 2; // cm

  if (distance > 0 && distance < 15) { // person detected
    Serial.println("\nPerson Detected!");
    Serial.println("Place your RFID card near the reader...");

    // Step 2: Wait for RFID card for up to 5 seconds
    unsigned long startTime = millis();
    bool cardDetected = false;
    while (millis() - startTime < 5000) { // wait for up to 5 sec
      if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        cardDetected = true;
        break;
      }
      delay(100);
    }

    if (!cardDetected) {
      Serial.println("⚠️ No card detected within 5 seconds.");
      return;
    }

    // Step 3: Read UID
    // Wait for a card/keychain when a person is detected
String uid = "";
Serial.println("Place your RFID card/keychain near the reader...");

while (uid == "") {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    for (byte i = 0; i < rfid.uid.size; i++) {
      if (rfid.uid.uidByte[i] < 0x10) uid += "0"; // add leading zero
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    Serial.print("RFID UID: ");
    Serial.println(uid);
    String userName = getName(uid);
    Serial.print("User Name: ");
    Serial.println(userName);
  }
  delay(200); // small delay for stable reading
}


    // Stop RFID communication cleanly
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    // Step 4: Read temperature
    double temp = mlx.readObjectTempC();
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.println(" °C");

    // Step 5: Determine health status
    String status = (temp < 37.5) ? "Normal" : "High Temperature";

    // Step 6: Send to ThingSpeak
    ThingSpeak.setField(1, uid);
    ThingSpeak.setField(2, String(temp, 2));
    ThingSpeak.setField(3, status);

    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200) {
      Serial.println("✅ Data uploaded to ThingSpeak!");
    } else {
      Serial.println("❌ ThingSpeak upload failed, code: " + String(x));
    }

    // Step 7: Send to Google Sheets via IFTTT
    sendToIFTTT(uid, temp, status);

    delay(5000); // wait before next reading
  }
}
