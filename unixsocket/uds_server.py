import socket
import os

SERVER_ADDRESS = r"C:\Users\deepa\tmp\test.sock"

# Make sure the socket does not already exist
try:
    os.unlink(SERVER_ADDRESS)
except FileNotFoundError:
    pass

# Create a UDS socket
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

# Bind the socket to the address
sock.bind(SERVER_ADDRESS)
sock.listen(1)
print("Server is listening...")

while True:
    connection, _ = sock.accept()
    try:
        data = connection.recv(1024)
        if data:
            print("Received:", data.decode())
            connection.sendall(b"Hello from server!")
    finally:
        connection.close()
