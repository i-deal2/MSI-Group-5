# Experiment 5: Smart Surveillance System Using ESP32-CAM

## Software Setup
1. In Arduino IDE, install the ESP32 Board Package via Board Manager 
2. In Tools, Select: 
• Board: ESP32 Wrover Module 
• Partition Scheme: Huge App (3MB No OTA) 
• Upload Speed: 115200
### Additional Setup for phase 4
4. Download additional libraries in [GitHub](https://github.com/yoursunny/esp32cam)
5. Install required libraries inside Python (PyCharm):
   ```bash
   pip install opencv-python==4.10.0.84
## Rules Before Starting
1. Connect USB to ESP32-CAM 
2. Hold IO0 to GND, then power the board (flash mode)
3. Disconnect from step 2 to run the code
