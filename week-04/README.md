# Experiment 3: Designing a Motion-Activated RFID System

## Software Requirements
- **Arduino IDE**: For flashing `.ino` files to the Arduino Uno.
- **Python 3.13**: Ensure **tcl/tk and IDLE** are checked during installation (required for Matplotlib GUI windows).
- **PyCharm**: Used as the IDE for managing and running Python scripts.

## General Rules Before Starting
1. Connect your Arduino Uno via USB.
2. Open Arduino IDE, check `Tools > Port` to identify your active COM port (e.g., `COM5`). Adjust the code based on the COM port.
3. **CRITICAL:** Close the Arduino IDE Serial Monitor or Serial Plotter before running any Python script. COM ports cannot be shared concurrently.

---

## Python Dependencies Setup in PyCharm
1. Open the repository root folder in **PyCharm**.
2. Open the built-in **Terminal** tab at the bottom of PyCharm.
3. Install required libraries inside the project's virtual environment:
   ```bash
   pip install pyserial matplotlib
