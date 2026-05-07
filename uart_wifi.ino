#include <WiFi.h>
#include <HTTPClient.h>

// =========================
// UART cho ESP32 (Sử dụng HardwareSerial 2)
// RX = GPIO16 (nối với TX của RA6M5)
// TX = GPIO17 (nối với RX của RA6M5)
// =========================
HardwareSerial RA_UART(2);

// =========================
// Wi-Fi / Server
// =========================
const char* WIFI_SSID     = "OPPO Reno10 5G";
const char* WIFI_PASSWORD = "j4xavwt4";
const char* SERVER_URL    = "http://10.101.208.188:5000/api/telemetry";

// =========================
// Globals
// =========================
String line = "";

float iaq  = 0.0f;
float tvoc = 0.0f;
float eco2 = 0.0f;
float pred  = 0.0f; // Thêm biến lưu giá trị dự đoán

unsigned long lastAlivePrint = 0;
unsigned long lastWiFiRetry  = 0;

// =========================
// Parse data line
// =========================
bool parseData(const String &s, float &iaqVal, float &tvocVal, float &eco2Val, float &predVal)
{
  int iaqPos  = s.indexOf("IAQ:");
  int tvocPos = s.indexOf(",TVOC:");
  int eco2Pos = s.indexOf(",ECO2:");
  int predPos = s.indexOf(",PRED:"); // Tìm vị trí nhãn PRED

  if (iaqPos == -1 || tvocPos == -1 || eco2Pos == -1 || predPos == -1) return false;

  String iaqStr  = s.substring(iaqPos + 4, tvocPos);
  String tvocStr = s.substring(tvocPos + 6, eco2Pos);
  String eco2Str = s.substring(eco2Pos + 6, predPos);
  String predStr = s.substring(predPos + 6); // Lấy giá trị dự đoán

  iaqVal  = iaqStr.toFloat();
  tvocVal = tvocStr.toFloat();
  eco2Val = eco2Str.toFloat();
  predVal = predStr.toFloat();

  return true;
}

// =========================
// Handle state messages
// =========================
bool handleStateMessage(const String &s)
{
  if (s.startsWith("STATE:"))
  {
    String stateText = s.substring(6);
    Serial.print("Sensor state: ");
    Serial.println(stateText);
    if (stateText.startsWith("ERR")) Serial.println("Sensor is reporting an error");
    else if (stateText == "WARMUP") Serial.println("Sensor is warming up");
    Serial.println("----------------------");
    return true;
  }
  return false;
}

// =========================
// Wi-Fi Connect
// =========================
bool connectWiFi()
{
  Serial.println("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (millis() - startMs > 15000UL)
    {
      Serial.println("\nWiFi connect timeout");
      return false;
    }
  }
  Serial.println("\nWiFi connected, IP = " + WiFi.localIP().toString());
  return true;
}

bool ensureWiFi()
{
  if (WiFi.status() == WL_CONNECTED) return true;
  if (millis() - lastWiFiRetry < 3000UL) return false;
  lastWiFiRetry = millis();
  return connectWiFi();
}

// =========================
// POST telemetry
// =========================
bool postTelemetry(float iaqVal, float tvocVal, float eco2Val, float predVal)
{
  if (!ensureWiFi()) return false;
  WiFiClient client; // Khai báo đối tượng client
  HTTPClient http;
 if (http.begin(client, SERVER_URL)) { 
    http.addHeader("Content-Type", "application/json");

    // Thêm trường "pred" vào Payload JSON
    char payload[200]; 
    snprintf(payload, sizeof(payload),
      "{\"device_id\":\"ra6m5-01\",\"sensor\":\"zmod4410\",\"iaq\":%.2f,\"tvoc\":%.2f,\"eco2\":%.2f,\"pred\":%.2f}",
      iaqVal, tvocVal, eco2Val, predVal);

    int httpCode = http.POST(payload);
    Serial.printf("POST code: %d\n", httpCode);

    if (httpCode > 0) {
      Serial.println("Response: " + http.getString());
    }
    http.end();
    return (httpCode > 0 && httpCode < 300);
  }
  return false;
}

void setup()
{
  Serial.begin(9600); // USB Serial để debug

  // Cấu hình UART2 cho ESP32: Baudrate 9600, Chế độ 8N1, RX=GPIO16, TX=GPIO17
  RA_UART.begin(9600, SERIAL_8N1, 16, 17);

  line.reserve(128);
  delay(500);
  Serial.println("\nESP32 Ready. Waiting ZMOD4410 data...");
  connectWiFi();
}

void loop()
{
  while (RA_UART.available())
  {
    char c = RA_UART.read();
    if (c == '\r') continue;
    if (c == '\n')
    {
      line.trim();
      if (line.length() > 0)
      {
        Serial.println("Raw: " + line);
        if (handleStateMessage(line)) {}
        else if (parseData(line, iaq, tvoc, eco2, pred)) // Truyền thêm biến pred
        {
          Serial.printf("IAQ=%.2f, TVOC=%.2f, ECO2=%.2f, PRED=%.2f\n", iaq, tvoc, eco2, pred);
          if (postTelemetry(iaq, tvoc, eco2, pred)) Serial.println("Upload success");
          else Serial.println("Upload failed");
          Serial.println("----------------------");
        }
        else Serial.println("Unknown UART format");
      }
      line = "";
    }
    else if (line.length() < 100) line += c;
  }

  if (millis() - lastAlivePrint > 5000UL)
  {
    lastAlivePrint = millis();
    Serial.println("[ESP32 alive]");
  }
  delay(10);
}