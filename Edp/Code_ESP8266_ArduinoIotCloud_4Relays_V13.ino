#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DEBUG_ESP_PORT Serial
#endif

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <map>
#include <time.h>

#define WIFI_SSID "Shivakumar"
#define WIFI_PASS "Shiva@2006"

#define APP_KEY "cdd62baa-83bf-425c-9645-d6d50e2e34f5"
#define APP_SECRET "ccb77b3e-d677-47ad-9cf3-631abe7fa548-1f1bb1bd-89e5-4d98-92b6-cae0c4c0f86d"

#define device_ID_1 "69b059badafb005af4d1b459"
#define device_ID_2 "69b05a8ddafb005af4d1b4fd"
#define device_ID_3 "69b05d1817b32c0941d0eaaa"
#define device_ID_4 "69b0619adf8a3a0123456789"

#define RelayPin1 5
#define RelayPin2 4
#define RelayPin3 14
#define RelayPin4 12

#define SwitchPin1 2
#define SwitchPin2 0
#define SwitchPin3 13
#define SwitchPin4 3

#define wifiLed 16

#define BAUD_RATE 115200
#define DEBOUNCE_TIME 250

typedef struct {
  int relayPIN;
  int flipSwitchPIN;
} deviceConfig_t;

std::map<String, deviceConfig_t> devices = {
  {device_ID_1, {RelayPin1, SwitchPin1}},
  {device_ID_2, {RelayPin2, SwitchPin2}},
  {device_ID_3, {RelayPin3, SwitchPin3}},
  {device_ID_4, {RelayPin4, SwitchPin4}}
};

typedef struct {
  String deviceId;
  bool lastState;
  unsigned long lastChange;
} flipSwitchConfig_t;

std::map<int, flipSwitchConfig_t> flipSwitches;

void setupRelays() {
  for (auto &device : devices) {
    pinMode(device.second.relayPIN, OUTPUT);
    digitalWrite(device.second.relayPIN, HIGH);
  }
}

void setupSwitches() {
  for (auto &device : devices) {
    flipSwitchConfig_t config;

    config.deviceId = device.first;
    config.lastState = true;
    config.lastChange = 0;

    int pin = device.second.flipSwitchPIN;

    flipSwitches[pin] = config;

    pinMode(pin, INPUT_PULLUP);
  }
}

bool onPowerState(String deviceId, bool &state) {

  Serial.printf("%s: %s\n", deviceId.c_str(), state ? "ON" : "OFF");

  digitalWrite(devices[deviceId].relayPIN, !state);

  return true;
}

void handleSwitches() {

  unsigned long now = millis();

  for (auto &sw : flipSwitches) {

    if (now - sw.second.lastChange > DEBOUNCE_TIME) {

      bool state = digitalRead(sw.first);

      if (state != sw.second.lastState) {

        sw.second.lastChange = now;

        String deviceId = sw.second.deviceId;

        int relayPin = devices[deviceId].relayPIN;

        bool newState = !digitalRead(relayPin);

        digitalWrite(relayPin, newState);

        SinricProSwitch &mySwitch = SinricPro[deviceId];
        mySwitch.sendPowerStateEvent(!newState);

        sw.second.lastState = state;
      }
    }
  }
}

void connectWiFi() {

  Serial.print("Connecting to WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  digitalWrite(wifiLed, LOW);
}

void setupTime() {

  Serial.print("Syncing time");

  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);

  while (now < 100000) {

    Serial.print(".");
    delay(500);

    now = time(nullptr);
  }

  Serial.println("\nTime synced");
}

void setupSinricPro() {

  for (auto &device : devices) {

    SinricProSwitch &mySwitch = SinricPro[device.first];

    mySwitch.onPowerState(onPowerState);
  }

  SinricPro.onConnected([](){ Serial.println("Connected to SinricPro"); });
  SinricPro.onDisconnected([](){ Serial.println("Disconnected from SinricPro"); });

  SinricPro.begin(APP_KEY, APP_SECRET);

  SinricPro.restoreDeviceStates(true);
}

void setup() {

  Serial.begin(BAUD_RATE);

  pinMode(wifiLed, OUTPUT);
  digitalWrite(wifiLed, HIGH);

  setupRelays();
  setupSwitches();

  connectWiFi();
  setupTime();
  setupSinricPro();
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi lost... reconnecting");

    connectWiFi();
  }

  SinricPro.handle();

  handleSwitches();
}