import os
import time
import threading
import urllib.request
from datetime import datetime

import cv2
import numpy as np

# ---------------- Settings ----------------
ESP_IP = "10.46.6.22"                      # change if the Serial Monitor shows a different IP
IMG_URL = f"http://{ESP_IP}/cam-hi.jpg"
SERVO_URL = f"http://{ESP_IP}/servo"
SAVE_DIR = "captures"                      # photos are saved here, next to this script
ABSENT_RESET_S = 1.0                       # face must be gone this long before a NEW photo is allowed
FLASH_S = 0.35                             # white camera-flash effect
BANNER_S = 2.0                             # "PHOTO CAPTURED" banner + green border

LIVE_WIN = "ESP32-CAM Face Detection"
PHOTO_WIN = "Captured Photo"

os.makedirs(SAVE_DIR, exist_ok=True)
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")


def ping_servo():
    """Tell the ESP32 'a face is in view' -> servo sweeps (it stops by itself ~1 s after the last call)."""
    try:
        urllib.request.urlopen(SERVO_URL, timeout=1).read()
        return True
    except Exception as e:
        print("Servo request failed:", e)
        return False


def beep():
    try:
        import winsound  # Windows only; silently skipped elsewhere
        threading.Thread(target=winsound.Beep, args=(1200, 150), daemon=True).start()
    except ImportError:
        pass


session_active = False   # True from the moment a photo is taken until the face has been gone for ABSENT_RESET_S
last_face_time = 0.0
captured_at = -999.0
last_name = ""

print("Connecting... Press 'q' in the video window to quit.")

while True:
    now = time.time()
    frame = None
    try:
        data = urllib.request.urlopen(IMG_URL, timeout=5).read()
        frame = cv2.imdecode(np.frombuffer(data, np.uint8), cv2.IMREAD_COLOR)
    except Exception as e:
        print("Stream error or timeout:", e)

    if frame is not None:
        clean = frame.copy()                       # untouched copy = the photo we save
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))

        if len(faces) > 0:
            last_face_time = now
            ping_servo()                           # servo starts / keeps sweeping

            if not session_active:                 # first frame of this appearance -> take ONE photo, right now
                session_active = True
                captured_at = time.time()
                stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                last_name = f"face_{stamp}.jpg"
                path = os.path.abspath(os.path.join(SAVE_DIR, last_name))
                cv2.imwrite(path, clean)
                print(f"PHOTO CAPTURED -> {path}")
                beep()

                popup = clean.copy()
                cv2.putText(popup, f"{last_name}  ({datetime.now().strftime('%H:%M:%S')})", (10, 28),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
                cv2.imshow(PHOTO_WIN, popup)       # second window pops up with the photo that was just taken
        elif session_active and (now - last_face_time) > ABSENT_RESET_S:
            session_active = False                 # face left -> next appearance can take a new photo

        # ---- draw boxes + status ----
        for (x, y, w, h) in faces:
            cv2.rectangle(frame, (x, y), (x + w, y + h), (255, 0, 0), 2)

        h_img, w_img = frame.shape[:2]
        if len(faces) > 0:
            cv2.putText(frame, f"FACE DETECTED ({len(faces)}) - SERVO ON", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
        else:
            cv2.putText(frame, "No face - idle", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (160, 160, 160), 2)

        # ---- capture indication (easy to see on a screen recording) ----
        since = time.time() - captured_at
        if since < FLASH_S:                        # white flash that fades out
            alpha = 0.85 * (1 - since / FLASH_S)
            white = np.full_like(frame, 255)
            frame = cv2.addWeighted(white, alpha, frame, 1 - alpha, 0)
        if since < BANNER_S:                       # green border + banner
            cv2.rectangle(frame, (0, 0), (w_img - 1, h_img - 1), (0, 255, 0), 12)
            cv2.rectangle(frame, (0, h_img // 2 - 40), (w_img, h_img // 2 + 40), (0, 160, 0), -1)
            cv2.putText(frame, "PHOTO CAPTURED!", (w_img // 2 - 190, h_img // 2 + 12),
                        cv2.FONT_HERSHEY_SIMPLEX, 1.3, (255, 255, 255), 3)

        cv2.imshow(LIVE_WIN, frame)

    if cv2.waitKey(5) == ord("q"):
        break

cv2.destroyAllWindows()
