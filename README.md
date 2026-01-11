# Tesla Sync Node 🚗⚡

An intelligent BLE tracking node built for the **Raspberry Pi Pico 2 W**. This project monitors the RSSI (Signal Strength) of a Tesla vehicle to determine if it is approaching or leaving a garage, allowing for smart home automation triggers.

## Key Features
- **High-Frequency BLE Scanning:** Real-time tracking of Tesla BLE advertisements.
- **Gaussian Weighted Trend Analysis:** Uses a Gaussian-weighted linear regression to calculate signal slope. This filters out RSSI "jitter" while remaining highly responsive to movement.
- **Web Dashboard:** A live, browser-based dashboard served directly from the Pico using Server-Sent Events (SSE).
- **Event Logging:** Automatic timestamped logs of state changes (Stationary -> Approaching -> Connected).

## Hardware Requirements
- Raspberry Pi Pico 2 W (or Pico W)
- A Tesla Vehicle with Phone Key / BLE enabled

## Software Setup

### 1. Secrets Management
Create a `secrets.h` file in the root directory (this file is ignored by Git for security):
```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "Your_WiFi_Name"
#define WIFI_PASSWORD "Your_WiFi_Password"

#endif