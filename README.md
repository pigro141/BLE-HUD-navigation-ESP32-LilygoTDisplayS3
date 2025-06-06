# BLE HUD for LilyGO T-Display-S3

![Device Preview](image/gif.gif)


## 🚀 Overview

A Bluetooth Low Energy (BLE) Heads-Up Display (HUD) for the **LilyGO T-Display-S3**, showing real-time navigation directions, speed, voltage, and connection status. This project is based on the work of [Alexander Lavrushko](https://github.com/alexanderlavrushko/BLE-HUD-navigation-ESP32) and adapted specifically for the LilyGO T-Display-S3 platform.

---

## 🛠 Hardware Requirements

* **ESP32** SoC (LilyGO T-Display-S3 integrates this)
* **TFT Display** - Integrated 1.9" ST7789 (on LilyGO T-Display-S3)
* Optional: Voltage divider for battery monitoring

### 🔗 Purchase LilyGO T-Display-S3

* 🛒 [Buy on Aliexpress (Affiliate Link)](https://s.click.aliexpress.com/e/_EIARYJM)
* 🛒 [Buy on Alibaba (Affiliate Link)](https://www.alibaba.com/x/AzeWnt?ck=pdp)

---

## 📲 Compatible iOS Mobile App

Currently supported on iOS via **Sygic GPS Navigation & Maps**: [Download](https://apps.apple.com/us/app/sygic-gps-navigation-maps/id585193266)

### 🔓 How to Enable BLE HUD Feature in the App

1. Open app: Menu → Settings → Info → About
2. Tap 3 times on any item to unlock hidden menu
3. Navigate: About → BLE HUD → Start
4. Grant Bluetooth permission


---

## 🔧 Setup & Installation

### Prerequisites

* Arduino IDE with ESP32 board support
* Clone this repo or download the `.ino` sketch and other files

### Libraries Required

* `TFT_eSPI` (configured for LilyGO)
* `BLEDevice`, `BLEServer`, `BLEUtils`, `BLE2902` (included with ESP32 core)

### LilyGO TFT Configuration

Use the official LilyGO fork for correct `TFT_eSPI` config:
[https://github.com/Xinyuan-LilyGO/T-Display-S3](https://github.com/Xinyuan-LilyGO/T-Display-S3)

Ensure you use `User_Setups/SetupXX_TDisplay_S3.h` or configure `User_Setup.h` accordingly.

### Flash Instructions

1. Select board: `ESP32 Dev Module` or `T-Display-S3`
2. Select correct port
3. Upload the sketch via Arduino IDE

---

## 📡 BLE Communication Protocol

The ESP32 expects navigation data in this format:

```
[1, <speed>, <direction>, <message>]
```

* `speed`: uint8 (0–255 km/h)
* `direction`: Enum (see `DirectionConstants.h`)
* `message`: Null-terminated string

---

## 💻 Usage

1. Power on device
2. Wait for BLE connection from Sygic app
3. HUD displays directions and speed automatically
4. Enter deep sleep via long press (if enabled)

---

## 📸 Screenshots

![Device Preview](image/image1.jpg)

---

## 🙌 Acknowledgements

* [Alexander Lavrushko](https://github.com/alexanderlavrushko) — original BLE HUD codebase

---

## 📜 License

Licensed under the MIT License — see `LICENSE` file for details.
