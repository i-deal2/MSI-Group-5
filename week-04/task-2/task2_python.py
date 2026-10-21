import serial
import time
import math
from collections import deque

# ================= CONFIGURATION =================
SERIAL_PORT = "COM3"  # Change to your Arduino COM port (e.g., "COM3", "COM4")
BAUD_RATE = 9600

# Authorized USB RFID card UIDs (from your console output: 0009522418)
AUTHORIZED_UIDS = {
    "0009522418",
}

MOTION_DURATION = 5.0   # Seconds to perform the circular motion
SMOOTHING_WINDOW = 4       # Moving average window size (smoothing)
MIN_AXIS_SPAN = 2500       # Minimum movement range on X and Y axes
MIN_RADIUS = 800           # Minimum radius from center to count as rotation
# =================================================


def detect_circular_motion(ser, duration=MOTION_DURATION):
    """
    Clears old buffered serial data, records live MPU6050 (ax, ay) data for
    `duration` seconds, applies a moving average, centers the coordinates,
    and checks if a circular motion was performed.
    """
    # Discard old ACC data accumulated while waiting for the RFID scan
    ser.reset_input_buffer()

    window_x = deque(maxlen=SMOOTHING_WINDOW)
    window_y = deque(maxlen=SMOOTHING_WINDOW)
    smoothed_points = []

    start_time = time.time()
    while time.time() - start_time < duration:
        if ser.in_waiting > 0:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if line.startswith("ACC:"):
                try:
                    parts = line[4:].split(",")
                    ax = int(parts[0])
                    ay = int(parts[1])

                    # Apply moving average smoothing
                    window_x.append(ax)
                    window_y.append(ay)
                    avg_x = sum(window_x) / len(window_x)
                    avg_y = sum(window_y) / len(window_y)
                    smoothed_points.append((avg_x, avg_y))
                except (ValueError, IndexError):
                    continue
        time.sleep(0.005)

    if len(smoothed_points) < 15:
        print("  [!] Not enough MPU6050 data received from Arduino.")
        return False

    # 1. Center coordinates dynamically (removes static tilt offset like -9288, -7380)
    xs = [p[0] for p in smoothed_points]
    ys = [p[1] for p in smoothed_points]
    cx = sum(xs) / len(xs)
    cy = sum(ys) / len(ys)

    span_x = max(xs) - min(xs)
    span_y = max(ys) - min(ys)

    # Ensure the sensor moved enough in both X and Y directions
    if span_x < MIN_AXIS_SPAN or span_y < MIN_AXIS_SPAN:
        print(f"  [!] Insufficient circular movement (X span: {int(span_x)}, Y span: {int(span_y)}).")
        return False

    # 2. Check quadrant progression and angular sweep around the center (cx, cy)
    quadrants_visited = set()
    total_angle_change = 0.0
    prev_angle = None

    for x, y in smoothed_points:
        dx = x - cx
        dy = y - cy
        if math.hypot(dx, dy) < MIN_RADIUS:
            continue

        if dx >= 0 and dy >= 0:
            quadrants_visited.add(1)
        elif dx < 0 and dy >= 0:
            quadrants_visited.add(2)
        elif dx < 0 and dy < 0:
            quadrants_visited.add(3)
        else:
            quadrants_visited.add(4)

        angle = math.atan2(dy, dx)
        if prev_angle is not None:
            delta = angle - prev_angle
            while delta > math.pi:
                delta -= 2 * math.pi
            while delta < -math.pi:
                delta += 2 * math.pi
            total_angle_change += delta
        prev_angle = angle

    swept_degrees = abs(math.degrees(total_angle_change))
    print(f"  [*] Quadrants visited: {len(quadrants_visited)}/4 | Angular sweep: {swept_degrees:.0f}°")

    return len(quadrants_visited) == 4 or (len(quadrants_visited) >= 3 and swept_degrees >= 220)


def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        time.sleep(2)  # Wait for Arduino reset
        ser.reset_input_buffer()
        print("====================================================")
        print("System Ready. Click inside this console window,")
        print("then tap your RFID card on the USB reader.")
        print("====================================================")
    except serial.SerialException as e:
        print(f"Serial Error: Could not open {SERIAL_PORT}. Details: {e}")
        return

    try:
        while True:
            # STEP 1: Scan RFID tag (USB RFID reader types UID + Enter into console)
            clean_uid = input("\n[1] Waiting for RFID scan: ").strip().upper()
            if not clean_uid:
                continue

            print(f"    Scanned UID: '{clean_uid}'")

            # STEP 2: Check if UID is authorized
            if clean_uid in AUTHORIZED_UIDS:
                print(f"[2] UID '{clean_uid}' is AUTHORIZED.")

                # Reset to locked state before checking motion
                ser.write(b'D')

                # STEP 3: Prompt user to perform a circular motion
                print(f"[3] Perform a CIRCULAR MOTION with the MPU6050 now ({MOTION_DURATION} seconds)...")

                # STEP 4 & 5: Analyze MPU6050 motion data & control Servo + LEDs
                if detect_circular_motion(ser, duration=MOTION_DURATION):
                    print("Motion Confirmed -> Access GRANTED (Servo 180°, Green LED ON).")
                    ser.write(b'A')
                else:
                    print("Motion Denied -> Access blocked (Servo 90°, Red LED ON).")  #[cite: 1]
                    ser.write(b'D')  #[cite: 1]
            else:
                print(f"Unauthorized UID '{clean_uid}' -> Access denied.")  #[cite: 1]
                ser.write(b'D')  #[cite: 1]

            time.sleep(0.01)  #[cite: 1]

    except KeyboardInterrupt:
        print("\nProgram terminated by user.")  #[cite: 1]
    finally:
        ser.close()  #[cite: 1]


if __name__ == "__main__":
    main()  #[cite: 1]
