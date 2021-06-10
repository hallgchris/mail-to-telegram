#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

#include "secrets.h"

const gpio_num_t REED_PIN = GPIO_NUM_26;
const uint64_t REED_PIN_MASK = 0x04000000; // 2^26

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

void configure_wifi()
{
  // attempt to connect to Wifi network:
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
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
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

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting...");

  const auto wakeup_reason = esp_sleep_get_wakeup_cause();
  print_wakeup_reason(wakeup_reason);
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    configure_wifi();
    bot.sendMessage(CHAT_ID, "Triggered by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    configure_wifi();
    bot.sendMessage(CHAT_ID, "Triggered by ext1");
    break;
  default:
    break;
  }

  touchAttachInterrupt(T3, nullptr, 40);
  esp_sleep_enable_touchpad_wakeup();

  esp_sleep_enable_ext1_wakeup(REED_PIN_MASK, ESP_EXT1_WAKEUP_ALL_LOW);

  esp_deep_sleep_start();
}

void loop() {}