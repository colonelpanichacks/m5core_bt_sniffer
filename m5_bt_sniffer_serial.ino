#include <M5Stack.h>
#include <NimBLEDevice.h>
#include <FastLED.h>
#include <vector>
#include <algorithm>

// Number of LEDs and data pin
#define NUM_LEDS 10
#define DATA_PIN 15

CRGB leds[NUM_LEDS]; 

// Information about each detected device
struct DeviceInfo {
    std::string macAddress;
    int rssi;
    unsigned long firstSeen;
};

// Store the list of detected devices
std::vector<DeviceInfo> devices;

// Default MAC address prefix filter
std::string macPrefixFilter = "00:11:22";

int dotCount = 0;  
unsigned long lastDisplayUpdate = 0; 

// Blink the RGB LEDs blue
void blinkLEDBlue() {
    // Set all LEDs to blue
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Blue;
    }
    FastLED.show(); 
    delay(200);

    // Turn off all LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
}

// Display devices or scanning status on the LCD
void displayDevices() {
    M5.Lcd.clear(BLACK);
    M5.Lcd.setCursor(0, 0);

    if (devices.empty()) {
        // Show "Scanning..." animation
        M5.Lcd.setTextSize(3);  
        String scanningText = "Scanning";
        for (int i = 0; i < dotCount; ++i) {
            scanningText += ".";
        }

        int16_t x = (M5.Lcd.width() - M5.Lcd.textWidth(scanningText)) / 2;
        int16_t y = (M5.Lcd.height() / 2) - 10;
        M5.Lcd.setCursor(x, y);
        M5.Lcd.setTextColor(TFT_GREENYELLOW);  
        M5.Lcd.print(scanningText);

        dotCount = (dotCount + 1) % 4; 
    } else {
        // Sort devices by their firstSeen time (descending)
        std::sort(devices.begin(), devices.end(), [](const DeviceInfo& a, const DeviceInfo& b) {
            return a.firstSeen > b.firstSeen;
        });

        // Display device list
        M5.Lcd.setTextSize(1);  
        for (auto &device : devices) {
            M5.Lcd.setCursor(0, M5.Lcd.getCursorY() + 15);
            M5.Lcd.setTextColor(TFT_GREEN);  
            M5.Lcd.printf("%s", device.macAddress.c_str());

            M5.Lcd.setTextColor(WHITE);
            unsigned long secondsSinceFirstSeen = (millis() - device.firstSeen) / 1000;
            M5.Lcd.printf(" | RSSI: %d dBm | %lu sec ago\n", device.rssi, secondsSinceFirstSeen);
        }
    }
    M5.update();
}

// Custom BLE scanning class with filtering
class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        // Get MAC address
        std::string macAddress = advertisedDevice->getAddress().toString();
        std::transform(macAddress.begin(), macAddress.end(), macAddress.begin(), ::tolower);

        // Check if MAC address starts with the current filter
        if (macAddress.rfind(macPrefixFilter, 0) == 0) {
            unsigned long currentMillis = millis();

            // Look for existing device
            auto it = std::find_if(devices.begin(), devices.end(), [&](DeviceInfo& d) {
                return d.macAddress == macAddress;
            });

            if (it != devices.end()) {
                // Update existing device (RSSI, etc.)
                it->rssi = advertisedDevice->getRSSI();
                // Optionally print re-detection message
                M5.Lcd.setCursor(0, M5.Lcd.getCursorY() + 15);
                M5.Lcd.println("Device re-detected: " + String(macAddress.c_str()));
            } else {
                // New device
                DeviceInfo newDevice;
                newDevice.macAddress = macAddress;
                newDevice.rssi = advertisedDevice->getRSSI();
                newDevice.firstSeen = currentMillis;
                devices.push_back(newDevice);

                // Beep and blink on new device
                M5.Speaker.tone(1000, 200);
                blinkLEDBlue();
                displayDevices();

                // Print message
                M5.Lcd.setCursor(0, M5.Lcd.getCursorY() + 15);
                M5.Lcd.println("New device: " + String(macAddress.c_str()));
            }
        }
    }
};

void setup() {
    // Initialize M5Stack
    M5.begin();
    M5.Power.begin();   // Some M5Stack variants require this for stable power

    // Clear screen
    M5.Lcd.clear(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("M5Stack BLE Scanner");

    // Initialize LEDs
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS); 
    FastLED.clear();

    // Speaker is part of M5Stack, no extra #include needed
    // but you can do M5.Speaker.begin() if needed

    // Start NimBLE
    NimBLEDevice::init("");
    NimBLEScan* pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setWindow(99);
    pBLEScan->start(0, nullptr); // Continuous scanning

    // (Optional) If you need UART, just call:
    // Serial2.begin(115200, SERIAL_8N1, 16, 17);
    // But no extra #includes required—M5Stack library covers it.
}

void loop() {
    // Update the display once a second
    unsigned long currentMillis = millis();
    if (currentMillis - lastDisplayUpdate >= 1000) {
        displayDevices();
        lastDisplayUpdate = currentMillis;
    }

    delay(500);
}