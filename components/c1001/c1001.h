#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
// No need to include DFRobot_HumanDetection library anymore
// We've implemented direct UART communication
#include <Stream.h> // Arduino Stream class

namespace esphome {
namespace c1001 {

class UARTToStream : public Stream {
 public:
  UARTToStream(uart::UARTDevice *parent) : parent_(parent) {}

  // Standard adapter methods - ESP32 dual-core can handle these operations without chunking
  virtual int available() override { 
    return parent_->available(); 
  }
  
  virtual int read() override { 
    return parent_->read();
  }
  
  virtual int peek() override { 
    return parent_->available() ? parent_->peek() : -1; 
  }
  
  virtual size_t write(uint8_t data) override { 
    return parent_->write(data);
  }
  
  virtual size_t write(const uint8_t *buffer, size_t size) override { 
    if (size == 0 || buffer == nullptr) return 0;
    parent_->write_array(buffer, size);
    return size;
  }

 protected:
  uart::UARTDevice *parent_;
};

class C1001Component : public PollingComponent, public uart::UARTDevice {
 public:
  C1001Component() = default;
  ~C1001Component();

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  
  // Handle communication errors
  void on_uart_error();
  
  // Method to request a reset of the initialization
  void reset_initialization();
  
  // Direct command helper using DFRobot protocol format
  bool send_command(uint8_t con, uint8_t cmd, uint8_t data_len = 1, uint8_t* data = nullptr, uint8_t* response_buffer = nullptr);
  
  // Helper to calculate checksum
  uint8_t calculate_checksum(uint8_t len, uint8_t* buf);

  void set_respiration_sensor(sensor::Sensor *respiration_sensor) { respiration_sensor_ = respiration_sensor; }
  void set_heart_rate_sensor(sensor::Sensor *heart_rate_sensor) { heart_rate_sensor_ = heart_rate_sensor; }
  void set_presence_sensor(sensor::Sensor *presence_sensor) { presence_sensor_ = presence_sensor; }
  void set_movement_sensor(sensor::Sensor *movement_sensor) { movement_sensor_ = movement_sensor; }
  void set_person_detected_binary_sensor(binary_sensor::BinarySensor *person_detected) {
    person_detected_ = person_detected;
  }
  
  // Sleep metrics access methods - moved to public section
  void set_sleep_state_sensor(sensor::Sensor *sleep_state_sensor) { sleep_state_sensor_ = sleep_state_sensor; }
  void set_in_bed_sensor(sensor::Sensor *in_bed_sensor) { in_bed_sensor_ = in_bed_sensor; }
  void set_sleep_quality_sensor(sensor::Sensor *sleep_quality_sensor) { sleep_quality_sensor_ = sleep_quality_sensor; }
  void set_sleep_quality_rating_sensor(sensor::Sensor *sleep_quality_rating_sensor) { sleep_quality_rating_sensor_ = sleep_quality_rating_sensor; }
  void set_awakening_count_sensor(sensor::Sensor *awakening_count_sensor) { awakening_count_sensor_ = awakening_count_sensor; }
  void set_deep_sleep_duration_sensor(sensor::Sensor *deep_sleep_duration_sensor) { deep_sleep_duration_sensor_ = deep_sleep_duration_sensor; }
  void set_light_sleep_duration_sensor(sensor::Sensor *light_sleep_duration_sensor) { light_sleep_duration_sensor_ = light_sleep_duration_sensor; }
  void set_awake_duration_sensor(sensor::Sensor *awake_duration_sensor) { awake_duration_sensor_ = awake_duration_sensor; }
  void set_turnover_count_sensor(sensor::Sensor *turnover_count_sensor) { turnover_count_sensor_ = turnover_count_sensor; }
  void set_average_respiration_sensor(sensor::Sensor *average_respiration_sensor) { average_respiration_sensor_ = average_respiration_sensor; }
  void set_average_heart_rate_sensor(sensor::Sensor *average_heart_rate_sensor) { average_heart_rate_sensor_ = average_heart_rate_sensor; }
  void set_apnea_events_sensor(sensor::Sensor *apnea_events_sensor) { apnea_events_sensor_ = apnea_events_sensor; }
  void set_large_body_movement_sensor(sensor::Sensor *large_body_movement_sensor) { large_body_movement_sensor_ = large_body_movement_sensor; }
  void set_minor_body_movement_sensor(sensor::Sensor *minor_body_movement_sensor) { minor_body_movement_sensor_ = minor_body_movement_sensor; }
  void set_sleep_score_sensor(sensor::Sensor *sleep_score_sensor) { sleep_score_sensor_ = sleep_score_sensor; }
  
  void set_abnormal_struggle_sensor(binary_sensor::BinarySensor *abnormal_struggle_sensor) { abnormal_struggle_sensor_ = abnormal_struggle_sensor; }
  void set_sleep_disturbance_sensor(binary_sensor::BinarySensor *sleep_disturbance_sensor) { sleep_disturbance_sensor_ = sleep_disturbance_sensor; }

 protected:
  bool sensor_initialized_{false};
  int init_state_{0};  // Track initialization state
  uint32_t last_successful_read_{0}; // Track time of last successful read
  uint8_t consecutive_errors_{0};    // Track consecutive errors

  // Basic sensors
  sensor::Sensor *respiration_sensor_{nullptr};
  sensor::Sensor *heart_rate_sensor_{nullptr};
  sensor::Sensor *presence_sensor_{nullptr};
  sensor::Sensor *movement_sensor_{nullptr};
  binary_sensor::BinarySensor *person_detected_{nullptr};
  
  // Sleep-specific sensors
  sensor::Sensor *sleep_state_sensor_{nullptr};              // 0=Deep, 1=Light, 2=Awake, 3=None
  sensor::Sensor *in_bed_sensor_{nullptr};                   // 0=Out of bed, 1=In bed
  sensor::Sensor *sleep_quality_sensor_{nullptr};            // 0-100 score
  sensor::Sensor *sleep_quality_rating_sensor_{nullptr};     // 0=None, 1=Good, 2=Average, 3=Poor
  sensor::Sensor *awakening_count_sensor_{nullptr};          // Number of awakenings
  sensor::Sensor *deep_sleep_duration_sensor_{nullptr};      // Minutes in deep sleep
  sensor::Sensor *light_sleep_duration_sensor_{nullptr};     // Minutes in light sleep
  sensor::Sensor *awake_duration_sensor_{nullptr};           // Minutes awake
  sensor::Sensor *turnover_count_sensor_{nullptr};           // Number of turnovers
  sensor::Sensor *average_respiration_sensor_{nullptr};      // Average respiration rate
  sensor::Sensor *average_heart_rate_sensor_{nullptr};       // Average heart rate
  sensor::Sensor *apnea_events_sensor_{nullptr};             // Number of apnea events
  sensor::Sensor *large_body_movement_sensor_{nullptr};      // Percentage of large body movements
  sensor::Sensor *minor_body_movement_sensor_{nullptr};      // Percentage of minor body movements
  sensor::Sensor *sleep_score_sensor_{nullptr};              // Sleep quality score
  
  // Sleep disturbance binary sensors
  binary_sensor::BinarySensor *abnormal_struggle_sensor_{nullptr};  // Abnormal struggle state
  binary_sensor::BinarySensor *sleep_disturbance_sensor_{nullptr};  // Sleep disturbance state
  
  // Sleep composite data cache
  uint8_t sleep_state_{3};                   // Default: None
  uint8_t in_bed_{0};                        // Default: Not in bed
  float average_respiration_{0.0f};          // Using float for scaled values
  float average_heartbeat_{0.0f};            // Using float for scaled values
  uint8_t turnover_count_{0};
  uint8_t large_body_movement_{0};
  uint8_t minor_body_movement_{0};
  uint8_t apnea_events_{0};
  uint8_t sleep_quality_score_{0};
  uint8_t sleep_quality_rating_{0};
  
  // Sleep metrics access methods now in public section
};

}  // namespace c1001
}  // namespace esphome