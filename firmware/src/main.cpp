#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

static const gpio_num_t REED_PIN = GPIO_NUM_13;
static const uint64_t REED_PIN_MASK = 0x2000; // 2^13

static const uint8_t TOUCH_PIN = T3;

static const uint8_t BAT_PIN = A0;

static const gpio_num_t STATUS_PIN = GPIO_NUM_26; // ESP32 built-in LED

static const float LOW_BATTERY_THRESHOLD = 1.2 * 3; // Volts, 3 cells at 1.2V

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

enum LetterboxState
{
  Open,
  Closed
};

RTC_DATA_ATTR LetterboxState letterbox_state = Open;

void configure_wifi()
{
  // Attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // Get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
}

void print_wakeup_reason(const esp_sleep_wakeup_cause_t wakeup_reason)
{
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

float read_battery_voltage()
{
  const int r1 = 1;
  const int r2 = 1;
  const float divider_factor = (float)r1 / (r1 + r2);
  return (float)analogReadMilliVolts(BAT_PIN) / divider_factor / 1000;
}

void on_touchpad_triggered()
{
  Serial.println("Triggered by touchpad: sending battery voltage");
  configure_wifi();
  const float battery_voltage = read_battery_voltage();
  bot.sendMessage(CHAT_ID, "Battery voltage is " + String(battery_voltage) + " V");
  Serial.println("Message sent!");
}

void on_reed_triggered()
{
  Serial.print("Triggered by EXT1: ");

  if (letterbox_state == Closed)
  {
    Serial.println("Letterbox opened, sending message and waiting to go high");
    letterbox_state = Open;

    configure_wifi();
    const float battery_voltage = read_battery_voltage();

    String message = "We just got a letter!";
    if (battery_voltage <= LOW_BATTERY_THRESHOLD)
      message += " Battery low (" + String(battery_voltage) + " V)";
    bot.sendMessage(CHAT_ID, message);
    Serial.println("Message sent!");
  }
  else
  {
    Serial.println("Letterbox closed, ready for next message");
    letterbox_state = Closed;
  }
}

void setup()
{
  // Indicate on status LED when ESP32 is not in deep sleep
  pinMode(STATUS_PIN, OUTPUT);
  digitalWrite(STATUS_PIN, HIGH);

  Serial.begin(115200);
  Serial.println("Starting...");

  const auto wakeup_reason = esp_sleep_get_wakeup_cause();
  print_wakeup_reason(wakeup_reason);
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    on_touchpad_triggered();
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    on_reed_triggered();
    break;
  default:
    break;
  }

  touchAttachInterrupt(TOUCH_PIN, nullptr, 40);
  esp_sleep_enable_touchpad_wakeup();

  esp_sleep_enable_ext1_wakeup(REED_PIN_MASK, letterbox_state == Open ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);

  Serial.println("Entering deep sleep");
  esp_deep_sleep_start();
}

void loop() {}