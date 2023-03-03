#include <Arduino.h>

#include <WiFi.h>
#include <ThingsBoard.h>

// Firmware title and version used to compare with remote version, to check if an update is needed.
// Title needs to be the same and version needs to be different --> downgrading is possible
constexpr char CURRENT_FIRMWARE_TITLE[] PROGMEM = "ESPino32";
constexpr char CURRENT_FIRMWARE_VERSION[] PROGMEM = "1.0.0";

// Firmware state send at the start of the firmware, to inform the cloud about the current firmware and that it was installed correctly,
// especially important when using OTA update, because the OTA update sends the last firmware state as UPDATING, meaning the device is restarting
// if the device restarted correctly and has the new given firmware title and version it should then send thoose to the cloud with the state UPDATED,
// to inform any end user that the device has successfully restarted and does actually contain the version it was flashed too
constexpr char FW_STATE_UPDATED[] PROGMEM = "UPDATED";

// Maximum amount of retries we attempt to download each firmware chunck over MQTT
constexpr uint8_t FIRMWARE_FAILURE_RETRIES PROGMEM = 5U;

// Size of each firmware chunck downloaded over MQTT,
// increased packet size, might increase download speed
constexpr uint16_t FIRMWARE_PACKET_SIZE PROGMEM = 4096U;

// PROGMEM can only be added when using the ESP32 WiFiClient,
// will cause a crash if using the ESP8266WiFiSTAClass instead.
constexpr char WIFI_SSID[] PROGMEM = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] PROGMEM = "YOUR_WIFI_PASSWORD";

constexpr char TOKEN[] PROGMEM = "YOUR_DEVICE_ACCESS_TOKEN";

constexpr char TONYSPACE_SERVER[] PROGMEM = "vsmqtt.space";
constexpr uint16_t TONYSPACE_PORT PROGMEM = 8080U;

// Maximum size packets will ever be sent or received by the underlying MQTT client,
// if the size is to small messages might not be sent or received messages will be discarded
constexpr uint32_t MAX_MESSAGE_SIZE PROGMEM = 512U;

// Baud rate for the debugging serial connection
// If the Serial output is mangled, ensure to change the monitor speed accordingly to this variable
constexpr uint32_t SERIAL_DEBUG_BAUD PROGMEM = 115200U;

// Initialize underlying client, used to establish a connection
WiFiClient espClient;

// Initialize ThingsBoard instance with the maximum needed buffer size
ThingsBoard tb(espClient, MAX_MESSAGE_SIZE);

// Statuses for updating
bool currentFWSent = false;
bool updateRequestSent = false;

void initializeWiFi(void);
const bool reconnect(void);
void updatedCallback(const bool& success);
void progressCallback(const uint32_t& currentChunk, const uint32_t& totalChuncks);
void blink(void);

const OTA_Update_Callback callback(&progressCallback, &updatedCallback, CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION, FIRMWARE_FAILURE_RETRIES, FIRMWARE_PACKET_SIZE);

void setup() {
  // Initalize serial connection for debugging
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);
  initializeWiFi();

  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  if (!reconnect()) {
    return;
  }

  if (!tb.connected()) {
    // Reconnect to the Tony space server,
    // if a connection was disrupted or has not yet been established
    Serial.printf("Connecting to: (%s) with token (%s)\n", TONYSPACE_SERVER, TOKEN);
    if (!tb.connect(TONYSPACE_SERVER, TOKEN, TONYSPACE_PORT)) {
      Serial.println(F("Failed to connect"));
      return;
    }
  }

  if (!currentFWSent) {
    currentFWSent = tb.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION) && tb.Firmware_Send_State(FW_STATE_UPDATED);
  }

  if (!updateRequestSent) {
    Serial.println(F("Firwmare Update Subscription..."));
    updateRequestSent = tb.Subscribe_Firmware_Update(callback);
  }

  blink();

  tb.loop();
}

void initializeWiFi(void) {
  Serial.println(F("Connecting to AP ..."));
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    // Delay 500ms until a connection has been succesfully established
    delay(500);
    Serial.print(F("."));
  }
  Serial.println(F("Connected to AP"));
}

const bool reconnect(void) {
  // Check to ensure we aren't connected yet
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return true;
  }

  // If we aren't establish a new connection to the given WiFi network
  initializeWiFi();
  return true;
}

void updatedCallback(const bool& success) {
  if (success) {
    Serial.println(F("Done, Reboot now"));
    esp_restart();
  }

  Serial.println(F("Downloading firmware failed"));
}

void progressCallback(const uint32_t& currentChunk, const uint32_t& totalChuncks) {
  Serial.printf("Progress %.2f%%\n", static_cast<float>(currentChunk * 100U) / totalChuncks);
}

void blink(void) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
}