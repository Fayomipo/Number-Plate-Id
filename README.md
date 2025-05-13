# LoRa License Plate Recognition System

A complete system for license plate detection, recognition, and wireless transmission using LoRa technology.

![License Plate Recognition System](https://via.placeholder.com/800x400?text=License+Plate+Recognition+System)

## 📋 Overview

This project creates an end-to-end license plate recognition system with two main components:

1. **Detection Unit**: A Raspberry Pi with camera that detects and recognizes license plates, then transmits the data via LoRa.
2. **Display Unit**: An Arduino-based receiver that displays the recognized license plates on an LCD screen.

The system is designed for applications like parking management, vehicle access control, security checkpoints, and traffic monitoring in areas with limited internet connectivity.

## ✨ Features

### Detection Unit (Raspberry Pi)
- Real-time license plate detection using OpenCV
- Optical Character Recognition (OCR) with pytesseract
- Nigerian license plate format validation
- Long-range wireless transmission via LoRa
- Multithreaded processing for improved performance
- Intelligent duplicate detection to prevent repeated transmissions
- Comprehensive error handling and logging

### Display Unit (Arduino)
- Reliable LoRa packet reception
- 16x2 LCD display with multiple display modes
- Persistent storage of license plate history in EEPROM
- System statistics tracking (uptime, signal quality, etc.)
- User-friendly interface with automatic screen switching
- Robust error handling and initialization

## 🔧 Hardware Requirements

### Detection Unit
- Raspberry Pi (3 or newer recommended)
- Camera module compatible with Raspberry Pi
- SX1278 LoRa module
- Appropriate power supply

### Display Unit
- Arduino (Uno, Nano, or similar)
- SX1278 LoRa module
- 16x2 I2C LCD display
- Appropriate power supply

## 📦 Software Dependencies

### Detection Unit
- Python 3.6+
- OpenCV (`cv2`)
- Imutils
- NumPy
- Pytesseract
- SX127x LoRa library

### Display Unit
- Arduino IDE
- LoRa library
- Wire library
- LiquidCrystal_I2C library

## 📥 Installation

### Detection Unit Setup

1. Install the required Python packages:
   ```bash
   pip install opencv-python imutils numpy pytesseract
   ```

2. Install Tesseract OCR:
   ```bash
   sudo apt-get install tesseract-ocr
   ```

3. Clone the LoRa library:
   ```bash
   git clone https://github.com/mayeranalytics/pySX127x.git /home/pi/LoRa
   ```

4. Connect the SX1278 LoRa module to the Raspberry Pi according to the pinout in the code.

5. Connect the camera module and enable it in Raspberry Pi configuration.

### Display Unit Setup

1. Install the required Arduino libraries using the Arduino Library Manager:
   - LoRa by Sandeep Mistry
   - LiquidCrystal I2C

2. Connect the SX1278 LoRa module to the Arduino according to the pinout in the code.

3. Connect the I2C LCD display to the Arduino's I2C pins.

## 🚀 Usage

### Running the Detection Unit

1. Copy the `license-plate-lora.py` file to your Raspberry Pi.

2. Run the script:
   ```bash
   python license-plate-lora.py
   ```

3. Position the camera where it has a clear view of incoming vehicles.

### Setting up the Display Unit

1. Open `license-plate-lora.cpp` in the Arduino IDE.

2. Verify and upload the code to your Arduino board.

3. The display will show "LoRa Receiver" and "Waiting..." until it receives data.

## 📐 System Architecture

```
┌─────────────────┐                      ┌──────────────────┐
│  Detection Unit │                      │   Display Unit   │
│  (Raspberry Pi) │                      │    (Arduino)     │
│                 │                      │                  │
│  ┌───────────┐  │     LoRa Radio       │  ┌────────────┐  │
│  │  Camera   │──┼─┐  Transmission  ┌───┼──│LoRa Module │  │
│  └───────────┘  │ │                │   │  └────────────┘  │
│                 │ │  ┌──────────┐  │   │                  │
│  ┌───────────┐  │ └──│LoRa     │──┘   │  ┌────────────┐  │
│  │OpenCV+OCR │──┼────│Module   │      │  │LCD Display │  │
│  └───────────┘  │    └──────────┘     │  └────────────┘  │
└─────────────────┘                      └──────────────────┘
```

## ⚙️ Configuration

### Detection Unit

The main configuration parameters in `license-plate-lora.py`:

```python
# Camera settings can be adjusted here
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
cap.set(cv2.CAP_PROP_FPS, 30)

# LoRa parameters can be modified for your region
lora.set_freq(915.0)  # Set to appropriate frequency for your region
```

### Display Unit

The main configuration parameters in `license-plate-lora.cpp`:

```cpp
// LoRa module pins
#define SS_PIN 10
#define RST_PIN 9
#define DIO0_PIN 2

// LCD I2C address - may need to be changed based on your LCD
#define LCD_ADDRESS 0x27

// LoRa frequency - adjust for your region
#define LORA_FREQUENCY 915E6  // 915MHz
```

## 🧰 Troubleshooting

### Detection Unit Issues

1. **Camera not working**:
   - Check if camera is enabled in Raspberry Pi configuration
   - Verify camera cable connection
   - Test camera with `raspistill -o test.jpg`

2. **OCR not detecting plates**:
   - Ensure good lighting conditions
   - Adjust camera focus and position
   - Check if tesseract is installed correctly

3. **LoRa transmission issues**:
   - Verify LoRa module connections
   - Check frequency settings match between transmitter and receiver
   - Ensure antennas are properly attached

### Display Unit Issues

1. **LCD not showing anything**:
   - Check I2C address (may need to be changed in the code)
   - Verify I2C wiring
   - Run an I2C scanner sketch to find the correct address

2. **Not receiving LoRa packets**:
   - Ensure both units use the same frequency
   - Check LoRa module connections
   - Verify that transmitter is sending data
   - Reduce distance or obstacles between units

## 🔄 Performance Optimization

- Position the camera at an optimal angle (15-30 degrees from perpendicular)
- Ensure adequate lighting for reliable plate detection
- Place LoRa units at elevated positions for better range
- For high-traffic areas, adjust the duplicate detection time threshold in the code



## 🙏 Acknowledgements

- [OpenCV](https://opencv.org/)
- [Tesseract OCR](https://github.com/tesseract-ocr/tesseract)
- [pySX127x](https://github.com/mayeranalytics/pySX127x)
- [Arduino LoRa](https://github.com/sandeepmistry/arduino-LoRa)
