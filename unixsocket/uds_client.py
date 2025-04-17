import socket

SERVER_ADDRESS = '/tmp/mysocket.sock'

# Create a UDS socket
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

# Connect the socket to the server
sock.connect(SERVER_ADDRESS)

try:
    sock.sendall(b"Hello from client!")
    data = sock.recv(1024)
    print("Received:", data.decode())
finally:
    sock.close()
