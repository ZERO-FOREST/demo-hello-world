import socket
import os
from tkinter import Tk, filedialog

HOST = '0.0.0.0'  # Listen on all available interfaces
PORT = 6556       # Port for ESP32 to connect to

def select_image_file():
    root = Tk()
    root.withdraw()  # Hide the main window
    file_path = filedialog.askopenfilename(
        title="Select an image file",
        filetypes=[("JPEG files", "*.jpg"), ("All files", "*.*")])
    return file_path

def send_image_to_esp32():
    print(f"Server listening on {HOST}:{PORT}")
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen(1)
        conn, addr = s.accept()
        with conn:
            print(f"Connected by {addr}")

            while True:
                image_path = select_image_file()
                if not image_path:
                    print("No image selected. Exiting.")
                    break

                if not os.path.exists(image_path):
                    print(f"File not found: {image_path}")
                    continue

                print(f"Sending image: {image_path}")
                try:
                    with open(image_path, 'rb') as f:
                        image_data = f.read()
                    
                    # Send the image data
                    conn.sendall(image_data)
                    print(f"Sent {len(image_data)} bytes.")
                    print("Image sent. Waiting for next image selection or connection close.")

                except Exception as e:
                    print(f"Error sending image: {e}")
                    break # Break if there's an error during sending, connection might be lost

if __name__ == "__main__":
    send_image_to_esp32()