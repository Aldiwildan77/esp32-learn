#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <DHT.h>
#include <PubSubClient.h>

// constants
const uint64_t DEFAULT_TIME = 1000000; // 1 second
const uint8_t TRIGGER_PIN = 5;         // D5
const uint8_t ECHO_PIN = 18;           // D18
const uint8_t DHT11_PIN = 16;          // D16
const uint8_t BUZZER_PIN = 19;         // D19
const unsigned long BAUD_RATE = 115200;

// WiFi credentials
const char *ssid = "***";
const char *password = "***";

// publish subscribe client mqtt
const char *mqtt_server = "192.168.1.65";

// flip flop timer
volatile bool timerFlag = false;

// client
WiFiClient espClient;
TaskHandle_t taskFlipFlop;
TaskHandle_t taskRest;
TaskHandle_t taskUltrasonic;
PubSubClient client(espClient);

// function declaration
void timerFlipFlop(void *pvParameter);
void timerRest(void *pvParameter);
void timerUltrasonic(void *pvParameter);
void reconnectMqtt();
void mqttCallback(char *topic, byte *payload, unsigned int length);

// DHT11
DHT dht(DHT11_PIN, DHT11);
float temperature;
float humidity;

void setup()
{
  Serial.begin(BAUD_RATE);

  randomSeed(analogRead(0));

  // ! pin mode
  // ultrasonic sensor
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);

  // flip flop
  pinMode(LED_BUILTIN, OUTPUT);

  // buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  // // wifi config
  // IPAddress local_ip = IPAddress(192, 168, 1, 10);
  // IPAddress gateway_ip = IPAddress(192, 168, 1, 1);
  // IPAddress netmask = IPAddress(255, 255, 255, 0);
  // IPAddress dns_ip = IPAddress(1, 1, 1, 1);

  // // Connect to WiFi
  // bool wifi_config = WiFi.config(local_ip, gateway_ip, netmask, dns_ip);
  // if (!wifi_config)
  // {
  //   Serial.println("STA Failed to configure");
  // }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1 * 1000);
    Serial.println("Connecting to WiFi...");
  }

  IPAddress localIp = WiFi.localIP();
  Serial.println("Connected to WiFi");
  Serial.printf("IP address: %s\n", localIp.toString().c_str());

  dht.begin();

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  // WiFi.enableAP(true);
  // WiFi.softAP(ssid, password);
  // Serial.println("Connected to WiFi");

  // OTA
  // ArduinoOTA.begin();

  // Task Scheduler
  xTaskCreatePinnedToCore(
      timerFlipFlop,   /* Function to implement the task */
      "timerFlipFlop", /* Name of the task */
      10 * 1000,       /* Stack size in words */
      NULL,            /* Task input parameter */
      0,               /* Priority of the task */
      &taskFlipFlop,   /* Task handle. */
      0                /* Core where the task should run */
  );

  xTaskCreatePinnedToCore(
      timerRest,   /* Function to implement the task */
      "timerRest", /* Name of the task */
      10 * 1000,   /* Stack size in words */
      NULL,        /* Task input parameter */
      3,           /* Priority of the task */
      &taskRest,   /* Task handle. */
      1            /* Core where the task should run */
  );

  xTaskCreatePinnedToCore(
      timerUltrasonic,   /* Function to implement the task */
      "timerUltrasonic", /* Name of the task */
      10 * 1000,         /* Stack size in words */
      NULL,              /* Task input parameter */
      3,                 /* Priority of the task */
      &taskUltrasonic,   /* Task handle. */
      1                  /* Core where the task should run */
  );
}

void loop()
{
  vTaskDelete(NULL);
}

void timerFlipFlop(void *pvParameter)
{
  while (true)
  {
    // OTA
    // ArduinoOTA.handle();

    if (!client.connected())
    {
      reconnectMqtt();
    }

    client.loop();

    // DHT11
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    Serial.printf("Temperature: %.2f ºC\n", temperature);
    Serial.printf("Humidity: %.2f humi\n", humidity);

    // buzzer
    digitalWrite(BUZZER_PIN, temperature > 31);

    // mqtt
    if (!isnan(temperature))
    {
      char tempString[8];
      dtostrf(temperature, 4, 2, tempString);
      client.publish("temperature/send", tempString);
    }

    // flip flop
    timerFlag = !timerFlag;
    digitalWrite(LED_BUILTIN, timerFlag);
    vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
  }
}

// rest api timer
void timerRest(void *pvParameter)
{
  while (true)
  {
    // HTTPClient http;

    // char payload[100];
    // sprintf(payload, "Temperature: %.2f ºC; Humidity: %.2f %%\n", temperature, humidity);

    // Serial.println("Sending request to API");
    // http.begin("https://fetcher.requestcatcher.com/test");
    // http.addHeader("Content-Type", "text/plain");
    // http.POST(payload);
    // Serial.println(http.getString());
    // http.end();
    // vTaskDelay(3 * 1000 / portTICK_PERIOD_MS);
  }
}

// ultrasonic sensor
void timerUltrasonic(void *pvParameter)
{
  while (true)
  {
    // give signal first
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);

    // receive signal
    long duration = pulseIn(ECHO_PIN, HIGH);

    // calculate distance
    long distance = (duration / 2) / 29.1;

    if (distance > 0)
    {
      Serial.printf("Distance: %d cm\n", distance);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
    }

    vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
  }
}

void reconnectMqtt()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("1"))
    {
      Serial.println("connected");
      // Subscribe
      client.subscribe("temperature/result");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5 * 1000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String payloadTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    payloadTemp += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == "temperature/result")
  {
    Serial.println(payloadTemp);
  }
}