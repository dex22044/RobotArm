import cv2
from socket import *
import threading

serv = socket(AF_INET, SOCK_STREAM)
serv.bind(('0.0.0.0', 8096))
serv.listen(1)

controlSock = socket(AF_INET, SOCK_DGRAM)
controlSock.bind(('0.0.0.0', 8097))

cam = cv2.VideoCapture(0)

if not cam.isOpened():
    raise IOError("Cam error")

def controlThreadProc():
    while True:
        controllerData = controlSock.recv(64)
        print('Received controller data')

controlThread = threading.Thread(target=controlThreadProc)
controlThread.start()

remoteClient, ra = serv.accept()
while True:
    ret, frame = cam.read()
    frame = cv2.resize(frame, (640, 480))

    is_success, im_buf_arr = cv2.imencode(".jpg", frame, [int(cv2.IMWRITE_JPEG_QUALITY), 15])
    byte_im = im_buf_arr.tobytes()
    remoteClient.send((len(byte_im)).to_bytes(4, byteorder="little"))
    remoteClient.send(byte_im)