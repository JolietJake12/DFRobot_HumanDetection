#include "c1001.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

namespace esphome {
namespace c1001 {

static const char *const TAG = "c1001";
// Maximum recovery attempts before a full reset
static const uint8_t MAX_CONSECUTIVE_ERRORS = 20;
// Timeout in milliseconds before considering the sensor dead
static const uint32_t SENSOR_TIMEOUT_MS = 120000;
// ESP32 may still need minimal delays at critical points
static const uint32_t MIN_OP_DELAY_MS = 5;

// Create enum to track initialization state
enum C1001InitState {
  INIT_NONE = 0,
  INIT_CREATED = 1,
  INIT_BEGIN_DONE = 2,
  INIT_SLEEP_MODE_DONE = 3,
  INIT_LED_DONE = 4,
  INIT_RESET_DONE = 5,
  INIT_COMPLETE = 6
};

void C1001Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up C1001 component with direct UART communication...");
  
  // Ensure UART is flushed before starting
  this->flush();
  delay(100);
  
  // We're using direct UART communication, so we don't need the stream adapter
  // and sensor objects anymore - we've implemented direct commands
  
  // Just mark that we're ready to start initialization
  this->init_state_ = INIT_CREATED;
  
  // Initialize error recovery counters
  this->consecutive_errors_ = 0;
  this->last_successful_read_ = millis();
  
  // Reset Arduino core WDT just to be safe
  delay(1);
  
  ESP_LOGI(TAG, "C1001 setup started - initialization will continue during update cycles");
}

void C1001Component::reset_initialization() {
  ESP_LOGW(TAG, "Resetting initialization process");
  this->init_state_ = INIT_CREATED;
  this->sensor_initialized_ = false;
  this->consecutive_errors_ = 0;
}

void C1001Component::on_uart_error() {
  ESP_LOGW(TAG, "UART Error detected");
  this->consecutive_errors_++;
  
  // If we have too many consecutive errors, reset initialization
  if (this->consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS) {
    ESP_LOGE(TAG, "Too many consecutive UART errors (%d), resetting initialization", 
             this->consecutive_errors_);
    this->reset_initialization();
  }
}

// Direct binary commands for the sensor using proper DFRobot protocol
// Command sections
#define CMD_START_BYTES        0x53, 0x59  // Start bytes for every command
#define CMD_END_BYTES          0x54, 0x43  // End bytes for every command

// Configuration registers
#define REG_CONFIG             0x01  // Configuration register
#define REG_WORK_MODE          0x02  // Work mode register
#define REG_BASIC_HUMAN        0x80  // Basic human detection
#define REG_BREATH             0x81  // Breathing detection
#define REG_SLEEP              0x84  // Sleep data register
#define REG_HEART              0x85  // Heart rate detection

// Command codes
#define CMD_SET_LED            0x03  // Set LED state (0=OFF, 1=ON)
#define CMD_GET_LED            0x83  // Get LED state
#define CMD_RESET              0x02  // Reset sensor
#define CMD_SET_WORK_MODE      0xA8  // Set work mode
#define CMD_GET_WORK_MODE      0xA8  // Get work mode

// Work modes
#define MODE_FALL              0x01  // Fall detection mode
#define MODE_SLEEP             0x02  // Sleep detection mode

// Data commands - Basic human detection
#define CMD_GET_PRESENCE       0x81  // Human presence (0=absent, 1=present)
#define CMD_GET_MOVEMENT       0x82  // Movement state (0=none, 1=slight, 2=intense)
#define CMD_GET_BREATHING      0x82  // Breathing rate
#define CMD_GET_HEART_RATE     0x82  // Heart rate

// Sleep data commands
#define CMD_GET_IN_BED             0x81  // In-bed status (0=out of bed, 1=in bed)
#define CMD_GET_SLEEP_STATE        0x82  // Sleep state (0=deep, 1=light, 2=awake, 3=none)
#define CMD_GET_WAKE_DURATION      0x83  // Wake duration in minutes
#define CMD_GET_LIGHT_SLEEP        0x84  // Light sleep duration in minutes
#define CMD_GET_DEEP_SLEEP         0x85  // Deep sleep duration in minutes
#define CMD_GET_SLEEP_QUALITY      0x86  // Sleep quality score (0-100)
#define CMD_GET_SLEEP_DISTURBANCE  0x8E  // Sleep disturbance (0=<4hrs, 1=>12hrs, 2=abnormal, 3=none)
#define CMD_GET_SLEEP_COMPOSITE    0x8D  // Composite sleep data
#define CMD_GET_SLEEP_STATISTICS   0x8F  // Sleep statistics
#define CMD_GET_SLEEP_QUALITY_RATING 0x90  // Sleep quality rating (0=none, 1=good, 2=avg, 3=poor)
#define CMD_GET_ABNORMAL_STRUGGLE  0x91  // Abnormal struggle (0=none, 1=normal, 2=abnormal)

// Response parsing state machine states
#define STATE_WAIT_START1      0
#define STATE_WAIT_START2      1
#define STATE_WAIT_CONFIG      2
#define STATE_WAIT_CMD         3
#define STATE_WAIT_LEN_H       4
#define STATE_WAIT_LEN_L       5
#define STATE_READ_DATA        6
#define STATE_CHECK_SUM        7
#define STATE_WAIT_END1        8
#define STATE_WAIT_END2        9

// Calculate checksum - sum all bytes in buffer and take lower 8 bits
uint8_t C1001Component::calculate_checksum(uint8_t len, uint8_t* buf) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < len; i++) {
    sum += buf[i];
  }
  return sum & 0xFF;
}

// Send a command using the proper DFRobot protocol format and wait for response
bool C1001Component::send_command(uint8_t con, uint8_t cmd, uint8_t data_len, uint8_t* data, uint8_t* response_buffer) {
  // Clear buffer
  while (this->available()) {
    this->read();
    yield();
  }
  
  // Format according to DFRobot protocol:
  // [0x53, 0x59, con, cmd, len_h, len_l, data..., checksum, 0x54, 0x43]
  uint8_t cmd_buffer[20]; // Buffer for command (enough for standard commands)
  uint8_t cmd_len = 6 + data_len + 3; // Base length + data length + checksum + end bytes
  
  // Start bytes
  cmd_buffer[0] = 0x53;
  cmd_buffer[1] = 0x59;
  
  // Command info
  cmd_buffer[2] = con;
  cmd_buffer[3] = cmd;
  
  // Data length (big-endian)
  cmd_buffer[4] = (data_len >> 8) & 0xFF;
  cmd_buffer[5] = data_len & 0xFF;
  
  // Data bytes
  if (data != nullptr && data_len > 0) {
    memcpy(&cmd_buffer[6], data, data_len);
  } else if (data_len > 0) {
    // If no data provided but length > 0, fill with default value (0x0F)
    for (uint8_t i = 0; i < data_len; i++) {
      cmd_buffer[6 + i] = 0x0F;
    }
  }
  
  // Calculate checksum over all bytes up to now
  cmd_buffer[6 + data_len] = calculate_checksum(6 + data_len, cmd_buffer);
  
  // End bytes
  cmd_buffer[7 + data_len] = 0x54;
  cmd_buffer[8 + data_len] = 0x43;
  
  // Print the complete command for debugging
  char debug_str[64];
  strcpy(debug_str, "Sending: ");
  for (uint8_t i = 0; i < cmd_len; i++) {
    char hex[5];
    sprintf(hex, "%02X:", cmd_buffer[i]);
    strcat(debug_str, hex);
  }
  // Remove the last colon
  debug_str[strlen(debug_str)-1] = '\0';
  ESP_LOGD(TAG, "%s", debug_str);
  
  // Send full command
  for (uint8_t i = 0; i < cmd_len; i++) {
    this->write(cmd_buffer[i]);
    delay(2); // Small delay between bytes
    yield();
  }
  
  // Wait for response with timeout
  uint32_t start = millis();
  uint32_t timeout = 2000; // 2 second timeout
  
  // Collect all bytes for debugging
  uint8_t recv_buffer[64] = {0};
  uint8_t recv_pos = 0;
  bool found_start = false;
  bool found_end = false;
  
  // Simple response detection - just look for start and end markers
  while (millis() - start < timeout) {
    if (this->available() > 0) {
      uint8_t byte = this->read();
      
      // Store in buffer for debugging
      if (recv_pos < sizeof(recv_buffer) - 1) {
        recv_buffer[recv_pos++] = byte;
      }
      
      // Look for start pattern (0x53, 0x59)
      if (byte == 0x53) {
        found_start = true;
      }
      
      // Look for end pattern (0x54, 0x43)
      if (byte == 0x43 && recv_pos > 1 && recv_buffer[recv_pos-2] == 0x54) {
        found_end = true;
      }
      
      // If we've found both start and end markers, we have a complete response
      if (found_start && found_end) {
        // Detect data bytes - typically position 6 for first data byte
        uint8_t data_byte = 0;
        if (recv_pos > 8) { // Ensure we have enough bytes
          // Simple response - just grab the likely data byte
          data_byte = recv_buffer[6]; // First data byte is usually what we want
        }
        
        // Log the response
        char resp_str[128] = "Received: ";
        for (uint8_t i = 0; i < recv_pos; i++) {
          char hex[5];
          sprintf(hex, "%02X:", recv_buffer[i]);
          strcat(resp_str, hex);
        }
        // Remove the last colon
        resp_str[strlen(resp_str)-1] = '\0';
        ESP_LOGD(TAG, "%s - Data: %02X", resp_str, data_byte);
        
        // Copy to response buffer if provided
        if (response_buffer != nullptr && recv_pos > 0) {
          memcpy(response_buffer, recv_buffer, recv_pos);
        }
        
        return true;
      }
    }
    delay(5); // Small delay to avoid CPU hogging
    yield();
  }
  
  // If we get here, we timed out
  if (recv_pos > 0) {
    // Log what we received before timeout
    char resp_str[128] = "Partial response: ";
    for (uint8_t i = 0; i < recv_pos; i++) {
      char hex[5];
      sprintf(hex, "%02X:", recv_buffer[i]);
      strcat(resp_str, hex);
    }
    // Remove the last colon
    resp_str[strlen(resp_str)-1] = '\0';
    ESP_LOGW(TAG, "%s (timeout after %d ms)", resp_str, timeout);
  } else {
    ESP_LOGW(TAG, "No response received (timeout after %d ms)", timeout);
  }
  
  return false;
}

void C1001Component::update() {
  ESP_LOGV(TAG, "Running update");
  
  // If we aren't fully initialized yet
  if (this->init_state_ != INIT_COMPLETE) {
    static uint8_t retry_count = 0;
    bool success = false;
    uint8_t dummy_byte = 0x0F;
    uint8_t response[20] = {0};
    
    // Direct binary protocol implementation using the DFRobot format
    switch (this->init_state_) {
      case INIT_CREATED: {
        ESP_LOGD(TAG, "Attempting direct sensor initialization [attempt: %d]", retry_count + 1);
        
        // Flush buffer and try a basic command to check if sensor is alive
        // Use LED query command as a basic test
        this->flush();
        
        // Debug the bytes we're sending - this is the new send_command
        ESP_LOGD(TAG, "Calling send_command with REG_CONFIG=%d, CMD_GET_LED=%d", REG_CONFIG, CMD_GET_LED);
        success = this->send_command(REG_CONFIG, CMD_GET_LED, 1, &dummy_byte, response);
        
        if (success) {
          ESP_LOGI(TAG, "Sensor is responding - proceeding with initialization");
          this->init_state_ = INIT_BEGIN_DONE;
          retry_count = 0;
          return;
        } else {
          retry_count++;
          ESP_LOGW(TAG, "No response from sensor, will retry next update");
          return;
        }
      }
      
      case INIT_BEGIN_DONE: {
        ESP_LOGD(TAG, "Setting sleep mode");
        
        // First query current mode
        ESP_LOGD(TAG, "Calling send_command to query work mode");
        success = this->send_command(REG_WORK_MODE, CMD_GET_WORK_MODE, 1, &dummy_byte, response);
        if (!success) {
          retry_count++;
          ESP_LOGW(TAG, "Work mode query failed, will retry next update");
          return;
        }
        
        // If not already in sleep mode (0x01), set it
        ESP_LOGD(TAG, "Current mode: %02X (sleep mode is: %02X)", response[6], MODE_SLEEP);
        if (response[6] != MODE_SLEEP) {
          uint8_t sleep_mode = MODE_SLEEP;
          ESP_LOGD(TAG, "Setting sleep mode with send_command");
          success = this->send_command(REG_WORK_MODE, CMD_SET_WORK_MODE, 1, &sleep_mode, response);
          if (!success) {
            retry_count++;
            ESP_LOGW(TAG, "Sleep mode command failed, will retry next update");
            return;
          }
        }
        
        ESP_LOGI(TAG, "Sleep mode set successfully");
        this->init_state_ = INIT_SLEEP_MODE_DONE;
        retry_count = 0;
        return;
      }
      
      case INIT_SLEEP_MODE_DONE: {
        ESP_LOGD(TAG, "Configuring LED");
        
        // Configure LED (0x01 = ON)
        uint8_t led_on = 0x01;
        ESP_LOGD(TAG, "Calling send_command to set LED");
        success = this->send_command(REG_CONFIG, CMD_SET_LED, 1, &led_on, response);
        if (!success) {
          retry_count++;
          ESP_LOGW(TAG, "LED command failed, will retry next update");
          return;
        }
        
        ESP_LOGI(TAG, "LED configured successfully");
        this->init_state_ = INIT_LED_DONE;
        retry_count = 0;
        return;
      }
      
      case INIT_LED_DONE: {
        ESP_LOGD(TAG, "Resetting sensor");
        
        // Reset sensor - uses REG_CONFIG
        ESP_LOGD(TAG, "Calling send_command to reset sensor");
        success = this->send_command(REG_CONFIG, CMD_RESET, 1, &dummy_byte, response);
        if (!success) {
          retry_count++;
          ESP_LOGW(TAG, "Reset command failed, will retry next update");
          return;
        }
        
        // Wait a bit after reset
        delay(100);
        yield();
        
        ESP_LOGI(TAG, "Sensor reset successful");
        this->init_state_ = INIT_COMPLETE;
        this->sensor_initialized_ = true;
        this->consecutive_errors_ = 0;
        this->last_successful_read_ = millis();
        ESP_LOGI(TAG, "C1001 initialization complete!");
        retry_count = 0;
        return;
      }
        
      default: {
        // Safety fallback
        ESP_LOGW(TAG, "Unknown initialization state: %d", this->init_state_);
        this->init_state_ = INIT_CREATED;
        retry_count = 0;
        break;
      }
    }
    
    // Return after handling initialization
    return;
  }
  
  // Only proceed with normal updates if initialization is complete
  if (!this->sensor_initialized_) {
    ESP_LOGW(TAG, "Sensor not initialized, skipping update");
    return;
  }
  
  // Check if we've gone too long without a successful read
  uint32_t now = millis();
  if (now - this->last_successful_read_ > SENSOR_TIMEOUT_MS) {
    ESP_LOGE(TAG, "Sensor timeout - no successful read in %u ms", 
             now - this->last_successful_read_);
    this->reset_initialization();
    return;
  }
  
  // We'll use a more sophisticated approach to prioritize HR and respiration readings
  // while still cycling through other metrics at lower frequency
  static uint8_t read_step = 0;
  static uint8_t vital_count = 0;
  bool success = false;
  uint8_t dummy_byte = 0x0F;
  uint8_t response[20] = {0};
  
  // Define current step based on priority pattern:
  // Vital signs (HR + Resp) are read at 3x frequency of other readings
  uint8_t current_step;
  if (vital_count < 2) {
    // Read vital signs (breathing or heart rate) 2 out of 3 cycles
    vital_count++;
    
    // Alternate between breathing and heart rate
    if (vital_count % 2 == 1) {
      current_step = 2;  // Get breathing value
    } else {
      current_step = 3;  // Get heart rate value
    }
  } else {
    // Every 3rd cycle, read a non-vital metric
    vital_count = 0;
    
    // Use a reduced range for non-vital metrics if we're only interested in vitals
    // This cycles through presence, movement, and sleep composite data
    current_step = read_step;
    read_step = (read_step + 1) % 14;
    
    // Skip vital signs steps (2 and 3) during the regular cycle
    // as they're already read in the priority cycle
    if (current_step == 2 || current_step == 3) {
      current_step = (current_step + 1) % 14;
    }
  }
  
  // Process the current step
  switch (current_step) {
    case 0: {
      // Get human presence data using the proper protocol
      ESP_LOGD(TAG, "Reading presence data with REG_BASIC_HUMAN=%d, CMD_GET_PRESENCE=%d", REG_BASIC_HUMAN, CMD_GET_PRESENCE);
      success = this->send_command(REG_BASIC_HUMAN, CMD_GET_PRESENCE, 1, &dummy_byte, response);
      
      if (success) {
        // Extract presence value from response[6]
        int raw_presence = response[6];
        ESP_LOGD(TAG, "Raw presence value: %d", raw_presence);
        
        // Based on observations: high values (~95) when nobody is present, 
        // low values (<50) when someone is present
        // This suggests the raw value is inverted from what we'd expect
        bool is_present = (raw_presence < 50);  // Threshold based on observations
        
        if (this->presence_sensor_ != nullptr) {
          // Report the raw value for analysis
          this->presence_sensor_->publish_state(raw_presence);
        }
        if (this->person_detected_ != nullptr) {
          // Publish inverted interpretation
          this->person_detected_->publish_state(is_present);
          ESP_LOGI(TAG, "Person detected: %s (raw value: %d)", is_present ? "YES" : "NO", raw_presence);
        }
      }
      break;
    }
    
    case 1: {
      // Get movement data
      ESP_LOGD(TAG, "Reading movement data with REG_BASIC_HUMAN=%d, CMD_GET_MOVEMENT=%d", REG_BASIC_HUMAN, CMD_GET_MOVEMENT);
      success = this->send_command(REG_BASIC_HUMAN, CMD_GET_MOVEMENT, 1, &dummy_byte, response);
      
      if (success) {
        // Extract movement value from response[6]
        int movement = response[6];
        ESP_LOGD(TAG, "Movement value: %d", movement);
        
        if (movement >= 0 && movement <= 2 && this->movement_sensor_ != nullptr) {
          this->movement_sensor_->publish_state(movement);
        }
      }
      break;
    }
    
    case 2: {
      // Get breathing value - HIGH PRIORITY MEASUREMENT
      ESP_LOGD(TAG, "Reading breathing data with REG_BREATH=%d, CMD_GET_BREATHING=%d", REG_BREATH, CMD_GET_BREATHING);
      success = this->send_command(REG_BREATH, CMD_GET_BREATHING, 1, &dummy_byte, response);
      
      if (success) {
        // Extract breathing value from response[6]
        // Official spec: Breath Measurement Range: 10-25 breaths per minute
        uint8_t raw_breathing = response[6];
        
        // Check if value is realistic for BPM or needs scaling
        float breathing;
        
        // Apply scaling based on sensor specification range of 10-25 BPM
        if (raw_breathing < 8) {
          // Too low to be physiologically realistic, scale up
          // Map 0-10 raw values to the 10-15 BPM range (lower half of spec)
          breathing = 10.0f + ((float)raw_breathing / 10.0f) * 5.0f;
          ESP_LOGD(TAG, "Scaled low respiration from raw %d to %.1f BPM", raw_breathing, breathing);
        } else if (raw_breathing > 25 && raw_breathing < 100) {
          // Between official range max and likely scale value, map to official range
          breathing = 10.0f + ((float)(raw_breathing - 25) / 75.0f) * 15.0f;
          ESP_LOGD(TAG, "Scaled mid respiration from raw %d to %.1f BPM", raw_breathing, breathing);
        } else if (raw_breathing >= 100) {
          // Likely on a different scale entirely (0-255), map to official range
          breathing = 10.0f + ((float)raw_breathing / 255.0f) * 15.0f;
          ESP_LOGD(TAG, "Scaled high respiration from raw %d to %.1f BPM", raw_breathing, breathing);
        } else {
          // Already within the official range of 10-25 BPM
          breathing = raw_breathing;
          ESP_LOGD(TAG, "Respiration value (direct): %.1f BPM", breathing);
        }
        
        // Check against official spec range (10-25 BPM)
        if (breathing >= 10.0f && breathing <= 25.0f && this->respiration_sensor_ != nullptr) {
          this->respiration_sensor_->publish_state(breathing);
        } else {
          ESP_LOGW(TAG, "Respiration value outside specified range (10-25 BPM): %.1f BPM (raw: %d)", 
                  breathing, raw_breathing);
          // Still publish if within more generous limits, just with a warning
          if (breathing >= 8.0f && breathing <= 30.0f && this->respiration_sensor_ != nullptr) {
            this->respiration_sensor_->publish_state(breathing);
          }
        }
      }
      break;
    }
    
    case 3: {
      // Get heart rate data - HIGH PRIORITY MEASUREMENT
      ESP_LOGD(TAG, "Reading heart rate data with REG_HEART=%d, CMD_GET_HEART_RATE=%d", REG_HEART, CMD_GET_HEART_RATE);
      success = this->send_command(REG_HEART, CMD_GET_HEART_RATE, 1, &dummy_byte, response);
      
      if (success) {
        // Extract heart rate value from response[6]
        // Official spec: Heart Rate Measurement Range: 60-100 beats per minute
        uint8_t raw_heart = response[6];
        
        // Check if value is realistic for BPM or needs scaling
        float heart;
        
        // Apply scaling based on sensor specification range of 60-100 BPM
        if (raw_heart < 30) {
          // Too low to be physiologically realistic, scale up
          // Map 0-30 raw values to the 60-75 BPM range (lower half of spec)
          heart = 60.0f + ((float)raw_heart / 30.0f) * 15.0f;
          ESP_LOGD(TAG, "Scaled low heart rate from raw %d to %.1f BPM", raw_heart, heart);
        } else if (raw_heart > 100 && raw_heart < 150) {
          // Between official range max and likely scale threshold
          heart = 60.0f + ((float)(raw_heart - 30) / 120.0f) * 40.0f;
          ESP_LOGD(TAG, "Scaled mid heart rate from raw %d to %.1f BPM", raw_heart, heart);
        } else if (raw_heart >= 150) {
          // Likely on a different scale entirely (0-255), map to official range
          heart = 60.0f + ((float)raw_heart / 255.0f) * 40.0f;
          ESP_LOGD(TAG, "Scaled high heart rate from raw %d to %.1f BPM", raw_heart, heart);
        } else if (raw_heart >= 30 && raw_heart < 60) {
          // Below spec but potentially valid, apply gentle scaling
          heart = 60.0f - (60.0f - raw_heart) * 0.5f;  // Scale up but preserve some of the difference
          ESP_LOGD(TAG, "Adjusted below-range heart rate from raw %d to %.1f BPM", raw_heart, heart);
        } else {
          // Already within the official range of 60-100 BPM
          heart = raw_heart;
          ESP_LOGD(TAG, "Heart rate value (direct): %.1f BPM", heart);
        }
        
        // Check against official spec range (60-100 BPM)
        if (heart >= 60.0f && heart <= 100.0f && this->heart_rate_sensor_ != nullptr) {
          this->heart_rate_sensor_->publish_state(heart);
        } else {
          ESP_LOGW(TAG, "Heart rate value outside specified range (60-100 BPM): %.1f BPM (raw: %d)", 
                  heart, raw_heart);
          // Still publish if within more generous heart rate limits, just with a warning
          if (heart >= 40.0f && heart <= 120.0f && this->heart_rate_sensor_ != nullptr) {
            this->heart_rate_sensor_->publish_state(heart);
          }
        }
      }
      break;
    }
    
    case 4: {
      // Get in-bed status
      ESP_LOGD(TAG, "Reading in-bed status with REG_SLEEP=%d, CMD_GET_IN_BED=%d", REG_SLEEP, CMD_GET_IN_BED);
      success = this->send_command(REG_SLEEP, CMD_GET_IN_BED, 1, &dummy_byte, response);
      
      if (success) {
        // Extract in-bed value from response[6]
        int in_bed = response[6];
        this->in_bed_ = in_bed;
        ESP_LOGD(TAG, "In-bed status: %d (0=out of bed, 1=in bed)", in_bed);
        
        if (this->in_bed_sensor_ != nullptr) {
          this->in_bed_sensor_->publish_state(in_bed);
        }
      }
      break;
    }
    
    case 5: {
      // Get sleep state 
      ESP_LOGD(TAG, "Reading sleep state with REG_SLEEP=%d, CMD_GET_SLEEP_STATE=%d", REG_SLEEP, CMD_GET_SLEEP_STATE);
      success = this->send_command(REG_SLEEP, CMD_GET_SLEEP_STATE, 1, &dummy_byte, response);
      
      if (success) {
        // Extract sleep state value from response[6]
        int sleep_state = response[6];
        this->sleep_state_ = sleep_state;
        ESP_LOGD(TAG, "Sleep state: %d (0=Deep, 1=Light, 2=Awake, 3=None)", sleep_state);
        
        if (this->sleep_state_sensor_ != nullptr) {
          this->sleep_state_sensor_->publish_state(sleep_state);
        }
      }
      break;
    }
    
    case 6: {
      // Get sleep quality score
      ESP_LOGD(TAG, "Reading sleep quality with REG_SLEEP=%d, CMD_GET_SLEEP_QUALITY=%d", REG_SLEEP, CMD_GET_SLEEP_QUALITY);
      success = this->send_command(REG_SLEEP, CMD_GET_SLEEP_QUALITY, 1, &dummy_byte, response);
      
      if (success) {
        // Extract sleep quality value from response[6]
        int sleep_quality = response[6];
        this->sleep_quality_score_ = sleep_quality;
        ESP_LOGD(TAG, "Sleep quality score: %d (0-100)", sleep_quality);
        
        if (this->sleep_quality_sensor_ != nullptr) {
          this->sleep_quality_sensor_->publish_state(sleep_quality);
        }
      }
      break;
    }
    
    case 7: {
      // Get sleep quality rating
      ESP_LOGD(TAG, "Reading sleep quality rating with REG_SLEEP=%d, CMD_GET_SLEEP_QUALITY_RATING=%d", REG_SLEEP, CMD_GET_SLEEP_QUALITY_RATING);
      success = this->send_command(REG_SLEEP, CMD_GET_SLEEP_QUALITY_RATING, 1, &dummy_byte, response);
      
      if (success) {
        // Extract sleep quality rating from response[6]
        int rating = response[6];
        this->sleep_quality_rating_ = rating;
        ESP_LOGD(TAG, "Sleep quality rating: %d (0=None, 1=Good, 2=Average, 3=Poor)", rating);
        
        if (this->sleep_quality_rating_sensor_ != nullptr) {
          this->sleep_quality_rating_sensor_->publish_state(rating);
        }
      }
      break;
    }
    
    case 8: {
      // Get abnormal struggle status
      ESP_LOGD(TAG, "Reading abnormal struggle status with REG_SLEEP=%d, CMD_GET_ABNORMAL_STRUGGLE=%d", REG_SLEEP, CMD_GET_ABNORMAL_STRUGGLE);
      success = this->send_command(REG_SLEEP, CMD_GET_ABNORMAL_STRUGGLE, 1, &dummy_byte, response);
      
      if (success) {
        // Extract abnormal struggle status from response[6]
        int struggle = response[6];
        ESP_LOGD(TAG, "Abnormal struggle: %d (0=None, 1=Normal, 2=Abnormal)", struggle);
        
        if (this->abnormal_struggle_sensor_ != nullptr) {
          // Only consider it "on" if it's in abnormal state (2)
          this->abnormal_struggle_sensor_->publish_state(struggle == 2);
        }
      }
      break;
    }
    
    case 9: {
      // Get sleep composite data (includes many metrics in one call)
      ESP_LOGD(TAG, "Reading sleep composite data with REG_SLEEP=%d, CMD_GET_SLEEP_COMPOSITE=%d", REG_SLEEP, CMD_GET_SLEEP_COMPOSITE);
      success = this->send_command(REG_SLEEP, CMD_GET_SLEEP_COMPOSITE, 1, &dummy_byte, response);
      
      if (success) {
        // Extract composite values - these are multiple bytes starting at response[6]
        // Format from sSleepComposite struct:
        // presence, sleepState, averageRespiration, averageHeartbeat, turnoverNumber, largeBodyMove, minorBodyMove, apneaEvents
        uint8_t raw_avg_respiration = response[8];
        uint8_t raw_avg_heartbeat = response[9];
        this->turnover_count_ = response[10];
        this->large_body_movement_ = response[11];
        this->minor_body_movement_ = response[12];
        this->apnea_events_ = response[13];
        
        // Apply scaling to respiration rate based on official spec range (10-25 BPM)
        if (raw_avg_respiration < 8) {
          // Too low to be physiologically realistic, scale up
          this->average_respiration_ = 10.0f + ((float)raw_avg_respiration / 10.0f) * 5.0f;
          ESP_LOGD(TAG, "Scaled low average respiration from raw %d to %.1f BPM", 
                  raw_avg_respiration, this->average_respiration_);
        } else if (raw_avg_respiration > 25 && raw_avg_respiration < 100) {
          // Between official range max and likely scale value, map to official range
          this->average_respiration_ = 10.0f + ((float)(raw_avg_respiration - 25) / 75.0f) * 15.0f;
          ESP_LOGD(TAG, "Scaled mid average respiration from raw %d to %.1f BPM", 
                  raw_avg_respiration, this->average_respiration_);
        } else if (raw_avg_respiration >= 100) {
          // Likely on a different scale entirely (0-255), map to official range
          this->average_respiration_ = 10.0f + ((float)raw_avg_respiration / 255.0f) * 15.0f;
          ESP_LOGD(TAG, "Scaled high average respiration from raw %d to %.1f BPM", 
                  raw_avg_respiration, this->average_respiration_);
        } else {
          // Already within the official range of 10-25 BPM
          this->average_respiration_ = raw_avg_respiration;
        }
        
        // Apply scaling to heart rate based on official spec range (60-100 BPM)
        if (raw_avg_heartbeat < 30) {
          // Too low to be physiologically realistic, scale up
          this->average_heartbeat_ = 60.0f + ((float)raw_avg_heartbeat / 30.0f) * 15.0f;
          ESP_LOGD(TAG, "Scaled low average heart rate from raw %d to %.1f BPM", 
                  raw_avg_heartbeat, this->average_heartbeat_);
        } else if (raw_avg_heartbeat > 100 && raw_avg_heartbeat < 150) {
          // Between official range max and likely scale threshold
          this->average_heartbeat_ = 60.0f + ((float)(raw_avg_heartbeat - 30) / 120.0f) * 40.0f;
          ESP_LOGD(TAG, "Scaled mid average heart rate from raw %d to %.1f BPM", 
                  raw_avg_heartbeat, this->average_heartbeat_);
        } else if (raw_avg_heartbeat >= 150) {
          // Likely on a different scale entirely (0-255), map to official range
          this->average_heartbeat_ = 60.0f + ((float)raw_avg_heartbeat / 255.0f) * 40.0f;
          ESP_LOGD(TAG, "Scaled high average heart rate from raw %d to %.1f BPM", 
                  raw_avg_heartbeat, this->average_heartbeat_);
        } else if (raw_avg_heartbeat >= 30 && raw_avg_heartbeat < 60) {
          // Below spec but potentially valid, apply gentle scaling
          this->average_heartbeat_ = 60.0f - (60.0f - raw_avg_heartbeat) * 0.5f;
          ESP_LOGD(TAG, "Adjusted below-range average heart rate from raw %d to %.1f BPM", 
                  raw_avg_heartbeat, this->average_heartbeat_);
        } else {
          // Already within the official range of 60-100 BPM
          this->average_heartbeat_ = raw_avg_heartbeat;
        }
        
        ESP_LOGD(TAG, "Sleep composite: avg_resp=%.1f (raw=%d), avg_heart=%.1f (raw=%d), turnovers=%d, large_move=%d%%, minor_move=%d%%, apnea=%d",
                 this->average_respiration_, raw_avg_respiration, 
                 this->average_heartbeat_, raw_avg_heartbeat,
                 this->turnover_count_, this->large_body_movement_, 
                 this->minor_body_movement_, this->apnea_events_);
        
        // Publish all the values with range validation
        if (this->average_respiration_sensor_ != nullptr) {
          if (this->average_respiration_ >= 0 && this->average_respiration_ <= 40) {
            this->average_respiration_sensor_->publish_state(this->average_respiration_);
          } else {
            ESP_LOGW(TAG, "Average respiration out of range: %.1f BPM (raw: %d)", 
                    this->average_respiration_, raw_avg_respiration);
          }
        }
        
        if (this->average_heart_rate_sensor_ != nullptr) {
          if (this->average_heartbeat_ >= 40 && this->average_heartbeat_ <= 150) {
            this->average_heart_rate_sensor_->publish_state(this->average_heartbeat_);
          } else {
            ESP_LOGW(TAG, "Average heart rate out of range: %.1f BPM (raw: %d)", 
                    this->average_heartbeat_, raw_avg_heartbeat);
          }
        }
        
        if (this->turnover_count_sensor_ != nullptr) {
          this->turnover_count_sensor_->publish_state(this->turnover_count_);
        }
        
        if (this->large_body_movement_sensor_ != nullptr) {
          // Large body movement should be a percentage (0-100)
          if (this->large_body_movement_ <= 100) {
            this->large_body_movement_sensor_->publish_state(this->large_body_movement_);
          } else {
            ESP_LOGW(TAG, "Large body movement out of percentage range: %d%%", 
                    this->large_body_movement_);
          }
        }
        
        if (this->minor_body_movement_sensor_ != nullptr) {
          // Minor body movement should be a percentage (0-100)
          if (this->minor_body_movement_ <= 100) {
            this->minor_body_movement_sensor_->publish_state(this->minor_body_movement_);
          } else {
            ESP_LOGW(TAG, "Minor body movement out of percentage range: %d%%",
                    this->minor_body_movement_);
          }
        }
        
        if (this->apnea_events_sensor_ != nullptr) {
          this->apnea_events_sensor_->publish_state(this->apnea_events_);
        }
      }
      break;
    }
    
    case 10: {
      // Get sleep durations
      // Wake duration
      ESP_LOGD(TAG, "Reading wake duration with REG_SLEEP=%d, CMD_GET_WAKE_DURATION=%d", REG_SLEEP, CMD_GET_WAKE_DURATION);
      success = this->send_command(REG_SLEEP, CMD_GET_WAKE_DURATION, 1, &dummy_byte, response);
      
      if (success) {
        // For durations, it's 16-bit (2 bytes) - combine response[6] and response[7]
        uint16_t wake_duration = (response[6] << 8) | response[7];
        ESP_LOGD(TAG, "Wake duration: %d minutes", wake_duration);
        
        if (this->awake_duration_sensor_ != nullptr) {
          this->awake_duration_sensor_->publish_state(wake_duration);
        }
      }
      break;
    }
    
    case 11: {
      // Light sleep duration
      ESP_LOGD(TAG, "Reading light sleep duration with REG_SLEEP=%d, CMD_GET_LIGHT_SLEEP=%d", REG_SLEEP, CMD_GET_LIGHT_SLEEP);
      success = this->send_command(REG_SLEEP, CMD_GET_LIGHT_SLEEP, 1, &dummy_byte, response);
      
      if (success) {
        // For durations, it's 16-bit (2 bytes) - combine response[6] and response[7]
        uint16_t light_sleep = (response[6] << 8) | response[7];
        ESP_LOGD(TAG, "Light sleep duration: %d minutes", light_sleep);
        
        if (this->light_sleep_duration_sensor_ != nullptr) {
          this->light_sleep_duration_sensor_->publish_state(light_sleep);
        }
      }
      break;
    }
    
    case 12: {
      // Deep sleep duration
      ESP_LOGD(TAG, "Reading deep sleep duration with REG_SLEEP=%d, CMD_GET_DEEP_SLEEP=%d", REG_SLEEP, CMD_GET_DEEP_SLEEP);
      success = this->send_command(REG_SLEEP, CMD_GET_DEEP_SLEEP, 1, &dummy_byte, response);
      
      if (success) {
        // For durations, it's 16-bit (2 bytes) - combine response[6] and response[7]
        uint16_t deep_sleep = (response[6] << 8) | response[7];
        ESP_LOGD(TAG, "Deep sleep duration: %d minutes", deep_sleep);
        
        if (this->deep_sleep_duration_sensor_ != nullptr) {
          this->deep_sleep_duration_sensor_->publish_state(deep_sleep);
        }
      }
      break;
    }
    
    case 13: {
      // Sleep disturbance
      ESP_LOGD(TAG, "Reading sleep disturbance with REG_SLEEP=%d, CMD_GET_SLEEP_DISTURBANCE=%d", REG_SLEEP, CMD_GET_SLEEP_DISTURBANCE);
      success = this->send_command(REG_SLEEP, CMD_GET_SLEEP_DISTURBANCE, 1, &dummy_byte, response);
      
      if (success) {
        // Extract sleep disturbance from response[6]
        int disturbance = response[6];
        ESP_LOGD(TAG, "Sleep disturbance: %d (0=<4hrs, 1=>12hrs, 2=abnormal, 3=none)", disturbance);
        
        if (this->sleep_disturbance_sensor_ != nullptr) {
          // Only consider it "on" if there's a disturbance (not 3=none)
          this->sleep_disturbance_sensor_->publish_state(disturbance != 3);
        }
      }
      break;
    }
  }
  
  ESP_LOGD(TAG, "Update complete - priority step: %d (vital count: %d)", current_step, vital_count);
  
  // Track success/failure
  if (success) {
    this->last_successful_read_ = millis();
    this->consecutive_errors_ = 0;
  } else {
    this->consecutive_errors_++;
    ESP_LOGW(TAG, "Failed to read sensor data, consecutive errors: %d", 
             this->consecutive_errors_);
    
    // If we have too many consecutive errors, reset initialization
    if (this->consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS) {
      ESP_LOGE(TAG, "Too many consecutive sensor errors, resetting initialization");
      this->reset_initialization();
      return;
    }
  }
  
  ESP_LOGD(TAG, "Update complete - read step: %d", read_step);
}

void c1001::C1001Component::dump_config() {
  ESP_LOGCONFIG(TAG, "C1001 mmWave Human Detection Sensor:");
  LOG_UPDATE_INTERVAL(this);
  
  // Basic metrics
  ESP_LOGCONFIG(TAG, "  Basic Metrics:");
  LOG_SENSOR("    ", "Respiration Rate", this->respiration_sensor_);
  LOG_SENSOR("    ", "Heart Rate", this->heart_rate_sensor_);
  LOG_SENSOR("    ", "Presence", this->presence_sensor_);
  LOG_SENSOR("    ", "Movement", this->movement_sensor_);
  LOG_BINARY_SENSOR("    ", "Person Detected", this->person_detected_);
  
  // Sleep metrics
  ESP_LOGCONFIG(TAG, "  Sleep Metrics:");
  LOG_SENSOR("    ", "In Bed", this->in_bed_sensor_);
  LOG_SENSOR("    ", "Sleep State", this->sleep_state_sensor_);
  LOG_SENSOR("    ", "Sleep Quality Score", this->sleep_quality_sensor_);
  LOG_SENSOR("    ", "Sleep Quality Rating", this->sleep_quality_rating_sensor_);
  LOG_SENSOR("    ", "Awake Duration", this->awake_duration_sensor_);
  LOG_SENSOR("    ", "Light Sleep Duration", this->light_sleep_duration_sensor_);
  LOG_SENSOR("    ", "Deep Sleep Duration", this->deep_sleep_duration_sensor_);
  
  // Sleep analysis 
  ESP_LOGCONFIG(TAG, "  Sleep Analysis:");
  LOG_SENSOR("    ", "Average Respiration", this->average_respiration_sensor_);
  LOG_SENSOR("    ", "Average Heart Rate", this->average_heart_rate_sensor_);
  LOG_SENSOR("    ", "Turnover Count", this->turnover_count_sensor_);
  LOG_SENSOR("    ", "Large Body Movement", this->large_body_movement_sensor_);
  LOG_SENSOR("    ", "Minor Body Movement", this->minor_body_movement_sensor_);
  LOG_SENSOR("    ", "Apnea Events", this->apnea_events_sensor_);
  
  // Sleep alerts
  ESP_LOGCONFIG(TAG, "  Sleep Alerts:");
  LOG_BINARY_SENSOR("    ", "Abnormal Struggle", this->abnormal_struggle_sensor_);
  LOG_BINARY_SENSOR("    ", "Sleep Disturbance", this->sleep_disturbance_sensor_);
  
  ESP_LOGCONFIG(TAG, "  Sensor Initialized: %s", YESNO(this->sensor_initialized_));
}

c1001::C1001Component::~C1001Component() {
  // We no longer use these objects, as we're using direct UART communication
  // No need to delete anything
}

}  // namespace c1001
}  // namespace esphome