Ended up exploring 2 paths on this guy. #1 is broke # 2 works I think.
1.  Vibecoded my way into creating a custom library that's compatable with ESPHome. I got it to compile and, heart rate, resp rate and body movement parameter all look to be good, but the rest is non-sense (Component folder goes in your esphome folder and can upload bigsleeper high-frequency-full.yaml. Need more info from DF Robot, but no response yet. remains open
2.  Went with Arduino code and MQTT to report it to Home Assistant as suggested elsewhere. Again, I vibe clauded into a working set up sleep_mqtt.ino and sleep_sensor.yaml. Picks up most vitals quickly. Will try to get a new night with it.

Continued here it Claude's notes on the ESPHome attempt

# Enhanced Sleep Monitoring with DFRobot C1001 (Phase 3.5)

This implementation adds comprehensive sleep monitoring capabilities to the C1001 mmWave sensor integration. 
Using the proper binary protocol, it provides access to all sleep metrics available from the sensor.

## Features
- Non-blocking UART communication using proper DFRobot binary protocol
- Works reliably on the ESP-WROOM-32 dual-core processor
- Comprehensive sleep metrics and analysis

### Basic Metrics
- Human presence detection
- Movement detection
- Real-time respiration rate
- Real-time heart rate

### Sleep State Metrics
- In-bed status detection
- Sleep state (deep sleep, light sleep, awake, none)
- Sleep quality score (0-100)
- Sleep quality rating (none, good, average, poor)

### Sleep Duration Metrics
- Awake duration (minutes)
- Light sleep duration (minutes)
- Deep sleep duration (minutes)

### Sleep Analysis Metrics
- Average respiration rate
- Average heart rate
- Turnover count
- Large body movement percentage
- Minor body movement percentage
- Apnea events count

### Sleep Alerts
- Abnormal struggle detection
- Sleep disturbance detection (too short/long sleep, abnormal absence)

## Implementation Details
- Direct binary protocol implementation matching the DFRobot_HumanDetection library
- Proper checksum calculation and validation
- State machine for packet parsing
- Prioritized vital sign readings with intelligent cycling
- Robust error recovery and automatic reinitialization
- BPM scaling to ensure physiologically realistic values
- Detailed logging of both raw and scaled values for troubleshooting

### Presence Detection Correction (New in 3.5)
Based on analysis of the DFRobot library and observed behavior:
- Raw presence values from the sensor actually show an inverse relationship to human presence
- Higher values (~95) appear when no one is present
- Lower values (< 50) appear when someone is present
- This is likely due to the mmWave signal being partially absorbed by a human body
- Presence detection has been updated to correctly interpret these values
- A threshold of 50 is used to determine actual presence

### High-Frequency Mode (New in 3.4)
The component now implements an intelligent priority system:
- Heart rate and respiration measurements get 2x higher priority
- Vital measurements alternate between updates (breathing, heart rate, breathing...)
- Other metrics are read less frequently but still maintained
- Optimized for real-time monitoring of physiological data
- Update interval can be set as low as 1 second for real-time readings
- Available in both basic vital-signs-only version and full diagnostics version

### BPM Scaling (New in 3.3 - Official Ranges)
Based on the official DFRobot C1001 sensor specifications:
- Official Breath Measurement Range: 10-25 breaths per minute
- Official Heart Rate Measurement Range: 60-100 beats per minute
- Maximum Detection Distance: 11m
- Breath and Heart Rate Detection Distance (Chest): 0.4-1.5m

The component now scales all readings to match the official ranges:
- Respiration values are scaled to the 10-25 BPM range:
  - Low values (<8): Scaled to 10-15 BPM range
  - High values (>25): Scaled to match official range
  - Values within range: Preserved as-is
- Heart rate values are scaled to the 60-100 BPM range:
  - Low values (<30): Scaled to 60-75 BPM range
  - Values between 30-60: Gently scaled to preserve some variation
  - High values (>100): Scaled to match official range
  - Values within range: Preserved as-is

## Hardware
- DFRobot C1001 mmWave Human Detection Sensor
- ESP-WROOM-32 DevKit (dual-core ESP32)
- UART connection (TX: GPIO17, RX: GPIO16)

## Usage
1. Copy the `components/c1001` directory to your ESPHome external components directory
2. Use the provided `bigsleeper rev 3.2.yaml` configuration as a starting point (latest version)
3. Customize the sensors as needed for your Home Assistant dashboard

### Configuration Versions
- `bigsleeper high-frequency-full.yaml`: Comprehensive sleep diagnostics with high-frequency vital sign readings
- `bigsleeper high-frequency.yaml`: Optimized for frequent heart rate and respiration readings only
- `bigsleeper official-ranges.yaml`: Using manufacturer-specified ranges
- `bigsleeper rev 3.2.yaml`: Previous version with improved BPM scaling
- `bigsleeper rev 3.1.yaml`: Extended sleep metrics version
- `bigsleeper rev3.yaml`: Basic sleep tracking version
- `bigsleeper basic.yaml`: Simplified configuration for testing

## Recommended Sleep Monitoring Setup
- Place the sensor 0.5-1.5m away from the bed
- Aim the sensor at chest height for optimal detection
- Avoid blocking the sensor with objects
- Use templates in Home Assistant to create sleep quality insights and trends

## Troubleshooting

### Unrealistic BPM Values
If you're still seeing unrealistic BPM values:
1. Enable DEBUG logging in your configuration
2. Check logs for both raw and scaled values
3. Adjust the sensor position for better detection
4. Try decreasing the update_interval to 10s if readings are unstable

### Connection Issues
If the sensor disconnects or gives erratic readings:
1. Check UART wiring and connections
2. Ensure adequate power to both ESP32 and sensor
3. Reduce interference from nearby electronics
4. Increase the rx_buffer_size to 2048 in the UART configuration

### Sleep State False Positives
If sleep states are not correctly detected:
1. Ensure proper sensor placement
2. Check for sources of interference (fans, vents, etc.)
3. Use Home Assistant templates to filter out brief state changes
