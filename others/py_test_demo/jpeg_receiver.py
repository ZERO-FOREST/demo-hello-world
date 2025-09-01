import socket
import os
import struct

def receive_jpeg(host='0.0.0.0', port=12345, output_file='received.jpg'):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((host, port))
    sock.listen(1)
    print(f"Listening on {host}:{port}...")
    
    while True:
        conn, addr = sock.accept()
        print(f"Connected by {addr}")
        
        data = b''
        while True:
            packet = conn.recv(65535)
            if not packet:
                break
            data += packet
        
        if data:
            # 假设数据是纯JPEG，保存
            with open(output_file, 'wb') as f:
                f.write(data)
            print(f"Received JPEG from {addr} and saved to {output_file}")
            # 发送ACK
            conn.send(b'\x06')
        else:
            # 发送NACK
            conn.send(b'\x15')
        
        conn.close()

if __name__ == "__main__":
    receive_jpeg()