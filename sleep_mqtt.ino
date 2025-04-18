/**！
 * @file sleep_mqtt.ino
 * @brief This is an example of sleep detection using human millimeter wave radar with MQTT integration for ESP32.
 * 
 * ---------------------------------------------------------------------------------------------------
 *    board   |             MCU                | Leonardo/Mega2560/M0 | ESP32 |
 *     VCC    |            3.3V/5V             |        VCC           |  VCC  |
 *     GND    |              GND               |        GND           |  GND  |
 *     RX     |              TX                |     Serial1 TX1      |  D2   |
 *     TX     |              RX                |     Serial1 RX1      |  D3   |
 * ---------------------------------------------------------------------------------------------------
 * 
 * @copyright  Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author [tangjie](jie.tang@dfrobot.com)
 * @version  V1.0
 * @date  2024-06-03
 * @url https://github.com/DFRobot/DFRobot_HumanDetection
 */

#include "DFRobot_HumanDetection.h"
#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "*******";
const char* password = "*******";

// MQTT Broker settings
const char* mqtt_server = "******";
const int mqtt_port = 1883;
const char* mqtt_user = "*******";  // if authentication is required
const char* mqtt_password = "*****";  // if authentication is required
const char* client_id = "ESP32_SleepSensor";  // MQTT client ID
const char* topic_base = "sleepsensor/";  // Base topic for publishing

// Publish intervals (milliseconds)
const unsigned long essential_publish_interval = 10000;  // 10 seconds for essential data
const unsigned long detailed_publish_interval = 60000;   // 60 seconds for detailed data
unsigned long last_essential_publish = 0;
unsigned long last_detailed_publish = 0;

WiFiClient espClient;
PubSubClient mqtt(espClient);

DFRobot_HumanDetection hu(&Serial1);

// Function to set up WiFi connection
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Function to reconnect to MQTT broker
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(client_id, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Publish a connection message
      mqtt.publish("sleepsensor/status", "online");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Function to publish essential data every 10 seconds
void publishEssentialData() {
  char msg[50];
  
  // Get bed entry status
  int inBed = hu.smSleepData(hu.eInOrNotInBed);
  snprintf(msg, sizeof(msg), "%d", inBed);
  mqtt.publish("sleepsensor/bed_status", msg);
  
  // Get comprehensive sleep status for presence, heart rate, respiration
  sSleepComposite comprehensiveState = hu.getSleepComposite();
  
  // Publish presence
  snprintf(msg, sizeof(msg), "%d", comprehensiveState.presence);
  mqtt.publish("sleepsensor/presence", msg);
  
  // Always try to get respiration and heart rate (publishing regardless of presence)
  // Use direct methods for heart rate and respiration (like in basics.ino)
  int respRate = hu.getBreatheValue();
  int heartRate = hu.getHeartRate();
  
  // Only publish valid readings (not 255 which is the error value)
  if (respRate != 0xFF) {
    snprintf(msg, sizeof(msg), "%d", respRate);
    mqtt.publish("sleepsensor/respiration", msg);
  }
  
  if (heartRate != 0xFF) {
    snprintf(msg, sizeof(msg), "%d", heartRate);
    mqtt.publish("sleepsensor/heartbeat", msg);
  }
  
  // Only add movement information if someone is present
  if (comprehensiveState.presence == 1) {
    // Add human movement information (from basics.ino)
    int movementStatus = hu.smHumanData(hu.eHumanMovement);
    snprintf(msg, sizeof(msg), "%d", movementStatus);
    mqtt.publish("sleepsensor/movement_status", msg);
    
    // Add body movement parameters (from basics.ino)
    int movementParam = hu.smHumanData(hu.eHumanMovingRange);
    snprintf(msg, sizeof(msg), "%d", movementParam);
    mqtt.publish("sleepsensor/movement_param", msg);
  }
  
  Serial.println("Essential data published to MQTT");
}

// Function to publish detailed data every minute (all sleep metrics)
void publishDetailedData() {
  char msg[100];
  
  // Get comprehensive sleep status
  sSleepComposite comprehensiveState = hu.getSleepComposite();
  
  // Publish sleep state
  int sleepState = hu.smSleepData(hu.eSleepState);
  snprintf(msg, sizeof(msg), "%d", sleepState);
  mqtt.publish("sleepsensor/sleep_state", msg);
  
  // Publish durations
  snprintf(msg, sizeof(msg), "%d", hu.smSleepData(hu.eWakeDuration));
  mqtt.publish("sleepsensor/wake_duration", msg);
  
  snprintf(msg, sizeof(msg), "%d", hu.smSleepData(hu.eLightsleep));
  mqtt.publish("sleepsensor/light_sleep_duration", msg);
  
  snprintf(msg, sizeof(msg), "%d", hu.smSleepData(hu.eDeepSleepDuration));
  mqtt.publish("sleepsensor/deep_sleep_duration", msg);
  
  // Publish movement information
  snprintf(msg, sizeof(msg), "%d", comprehensiveState.turnoverNumber);
  mqtt.publish("sleepsensor/turnover_count", msg);
  
  snprintf(msg, sizeof(msg), "%d", comprehensiveState.largeBodyMove);
  mqtt.publish("sleepsensor/large_movement_percent", msg);
  
  snprintf(msg, sizeof(msg), "%d", comprehensiveState.minorBodyMove);
  mqtt.publish("sleepsensor/minor_movement_percent", msg);
  
  snprintf(msg, sizeof(msg), "%d", comprehensiveState.apneaEvents);
  mqtt.publish("sleepsensor/apnea_events", msg);
  
  // Publish sleep quality info
  snprintf(msg, sizeof(msg), "%d", hu.smSleepData(hu.eSleepQuality));
  mqtt.publish("sleepsensor/sleep_quality", msg);
  
  // Publish sleep quality rating
  snprintf(msg, sizeof(msg), "%d", hu.smSleepData(hu.eSleepQualityRating));
  mqtt.publish("sleepsensor/quality_rating", msg);
  
  // Publish if there was abnormal struggle
  snprintf(msg, sizeof(msg), "%d", hu.smSleepData(hu.eAbnormalStruggle));
  mqtt.publish("sleepsensor/abnormal_struggle", msg);
  
  // Publish sleep disturbances
  snprintf(msg, sizeof(msg), "%d", hu.smSleepData(hu.eSleepDisturbances));
  mqtt.publish("sleepsensor/sleep_disturbances", msg);
  
  // Publish sleep statistics data (only available after sleep process is over)
  sSleepStatistics statistics = hu.getSleepStatistics();
  
  snprintf(msg, sizeof(msg), "%d", statistics.sleepQualityScore);
  mqtt.publish("sleepsensor/stats/quality_score", msg);
  
  snprintf(msg, sizeof(msg), "%d", statistics.sleepTime);
  mqtt.publish("sleepsensor/stats/sleep_time", msg);
  
  snprintf(msg, sizeof(msg), "%d", statistics.wakeDuration);
  mqtt.publish("sleepsensor/stats/wake_duration", msg);
  
  snprintf(msg, sizeof(msg), "%d", statistics.shallowSleepPercentage);
  mqtt.publish("sleepsensor/stats/shallow_sleep_percent", msg);
  
  snprintf(msg, sizeof(msg), "%d", statistics.deepSleepPercentage);
  mqtt.publish("sleepsensor/stats/deep_sleep_percent", msg);
  
  snprintf(msg, sizeof(msg), "%d", statistics.timeOutOfBed);
  mqtt.publish("sleepsensor/stats/time_out_of_bed", msg);
  
  snprintf(msg, sizeof(msg), "%d", statistics.exitCount);
  mqtt.publish("sleepsensor/stats/exit_count", msg);
  
  snprintf(msg, sizeof(msg), "%d", statistics.turnOverCount);
  mqtt.publish("sleepsensor/stats/turnover_count", msg);
  
  snprintf(msg, sizeof(msg), "%d", statistics.apneaEvents);
  mqtt.publish("sleepsensor/stats/apnea_events", msg);
  
  // Add average respiration from sleep statistics (found in sleep.ino)
  snprintf(msg, sizeof(msg), "%d", statistics.averageRespiration);
  mqtt.publish("sleepsensor/stats/avg_respiration", msg);
  
  // Add average heartbeat from sleep statistics (found in sleep.ino)
  snprintf(msg, sizeof(msg), "%d", statistics.averageHeartbeat);
  mqtt.publish("sleepsensor/stats/avg_heartbeat", msg);
  
  Serial.println("Detailed data published to MQTT");
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, /*rx =*/16, /*tx =*/17); // Updated to match the pinout in ESP-WROOM32
  
  setup_wifi();
  mqtt.setServer(mqtt_server, mqtt_port);
  
  Serial.println("Starting initialization - will take at least 10 seconds...");

  Serial.println("Start initialization");
  while (hu.begin() != 0) {
    Serial.println("init error!!!");
    delay(1000);
  }
  Serial.println("Initialization successful");

  Serial.println("Start switching work mode");
  while (hu.configWorkMode(hu.eSleepMode) != 0) {
    Serial.println("error!!!");
    delay(1000);
  }
  Serial.println("Work mode switch successful");

  Serial.print("Current work mode:");
  switch (hu.getWorkMode()) {
    case 1:
      Serial.println("Fall detection mode");
      break;
    case 2:
      Serial.println("Sleep detection mode");
      break;
    default:
      Serial.println("Read error");
  }

  hu.configLEDLight(hu.eHPLed, 0);  // Turn off HP LED switch
  hu.sensorRet();                   // Module reset, must perform sensorRet after setting data, otherwise the sensor may not be usable

  Serial.print("HP LED status:");
  switch (hu.getLEDLightState(hu.eHPLed)) {
    case 0:
      Serial.println("Off");
      break;
    case 1:
      Serial.println("On");
      break;
    default:
      Serial.println("Read error");
  }

  Serial.println();
  Serial.println();
}

void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();

  // Only print minimal status information to serial to reduce memory usage
  int inBed = hu.smSleepData(hu.eInOrNotInBed);
  int sleepState = hu.smSleepData(hu.eSleepState);
  sSleepComposite comprehensiveState = hu.getSleepComposite();
  
  // Get movement information (added)
  int movementStatus = 0;
  int movementParam = 0;
  try {
    movementStatus = hu.smHumanData(hu.eHumanMovement);
    movementParam = hu.smHumanData(hu.eHumanMovingRange);
  } 
  catch (...) {
    // Ignore errors if these values can't be read
  }

  Serial.print("Bed: ");
  Serial.print(inBed == 1 ? "In" : "Out");
  
  Serial.print(" | Sleep: ");
  switch (sleepState) {
    case 0: Serial.print("Deep"); break;
    case 1: Serial.print("Light"); break;
    case 2: Serial.print("Awake"); break;
    case 3: Serial.print("None"); break;
    default: Serial.print("Error");
  }

  Serial.print(" | HR: ");
  Serial.print(hu.getHeartRate());
  
  Serial.print(" | Resp: ");
  Serial.print(hu.getBreatheValue());
  
  Serial.print(" | Move: ");
  switch (movementStatus) {
    case 0: Serial.print("None"); break;
    case 1: Serial.print("Still"); break;
    case 2: Serial.print("Active"); break;
    default: Serial.print("Error");
  }
  
  Serial.print(" | Move Param: ");
  Serial.println(movementParam);

  // Publish essential data every 10 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - last_essential_publish >= essential_publish_interval) {
    last_essential_publish = currentMillis;
    publishEssentialData();
  }

  // Publish detailed data every minute
  if (currentMillis - last_detailed_publish >= detailed_publish_interval) {
    last_detailed_publish = currentMillis;
    publishDetailedData();
  }

  // Shorter delay to keep the program responsive
  delay(500);
}
