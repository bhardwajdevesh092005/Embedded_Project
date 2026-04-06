import cv2, serial, struct, time, numpy as np

ser = serial.Serial("/dev/ttyACM0", 115200, timeout=0.1)
ser.reset_input_buffer()
ser.reset_output_buffer()
cap = cv2.VideoCapture("http://172.31.75.172:8080/video")

print("Waiting for STM32 'READY' signal...")
while True:
    if ser.in_waiting > 0:
        line = ser.readline().decode('utf-8', errors='ignore')
        print(line)
        if "READY_TO_RECEIVE" in line:
            print("✅ STM32 is READY. Starting Stream...")
            break
    time.sleep(0.01)

# Clear any leftover junk from the READY strings
ser.reset_input_buffer()

while True:
    # Flush the OpenCV buffer to get the freshest frame
    for _ in range(5):
        cap.grab()
    ret, frame = cap.read()
    if not ret: break

    # Preprocess
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    small = cv2.resize(gray, (96, 96))
    img_data = small.flatten().tobytes()

    # Clear stale responses from STM32 before sending next frame
    ser.reset_input_buffer()

    # 📤 SEND PACKET
    # Header(2) + Size(2) + Pixels(9216)
    packet = b'\xAA\x55' + struct.pack('<H', len(img_data)) + img_data
    
    print(f"Sending Frame ({len(packet)} bytes)...", end="", flush=True)
    ser.write(packet)
    ser.flush()
    print(" Sent.")
    time.sleep(0.5)

    # 📥 WAIT FOR AI RESULT (Max 5 seconds)
    start_time = time.time()
    rx_buffer = bytearray()
    
    while (time.time() - start_time) < 5.0:
        if ser.in_waiting > 0:
            rx_buffer.extend(ser.read(ser.in_waiting))
            if b'\xBB\x66' in rx_buffer:
                idx = rx_buffer.find(b'\xBB\x66')
                if idx + 2 < len(rx_buffer):
                    res = "PERSON" if rx_buffer[idx+2] == 1 else "NONE"
                    print(f"🤖 AI RESULT: {res}")
                    break
    
    # Small delay to keep the loop stable
    time.sleep(5)