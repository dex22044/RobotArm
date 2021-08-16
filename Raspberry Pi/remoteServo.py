import RPi.GPIO as GPIO
import time

GPIO.setmode(GPIO.BCM)

GPIO.setup(14, GPIO.OUT)
GPIO.setup(15, GPIO.OUT)

GPIO.output(14, False)
GPIO.output(15, False)

while True:
    angle = int(input())
    for i in range(8):
        GPIO.output(15, (angle >> i) & 1)
        GPIO.output(14, 1)
        time.sleep(0.01)
        GPIO.output(14, 0)
        time.sleep(0.01)