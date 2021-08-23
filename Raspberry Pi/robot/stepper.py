import RPi.GPIO as GPIO
import time
#import serial

#rsport = serial.Serial('/dev/ttyUSB0', 115200)

GPIO.setmode(GPIO.BCM)

GPIO.setup(27, GPIO.OUT)
GPIO.setup(22, GPIO.OUT)

GPIO.output(27, True)
GPIO.output(22, False)


GPIO.setup(23, GPIO.OUT)
GPIO.setup(24, GPIO.OUT)

GPIO.output(23, True)
GPIO.output(24, False)


GPIO.setup(5, GPIO.OUT)
GPIO.setup(6, GPIO.OUT)

GPIO.output(5, True)
GPIO.output(6, False)


steps = 0
dir = False

servo = 90
servoDir = False

while True:
    if steps == 0:
        dir = not dir
        GPIO.output(22, dir)
        GPIO.output(24, dir)
        GPIO.output(6, dir)
        time.sleep(0.1)
    time.sleep(0.001)
    GPIO.output(27, True)
    GPIO.output(23, True)
    GPIO.output(5, True)
    time.sleep(0.001)
    GPIO.output(27, False)
    GPIO.output(23, False)
    GPIO.output(5, False)
    steps += 1
    steps = steps % 500

    #if steps == 0:
    #    if servo <= 0 or servo >= 180:
    #        servoDir = not servoDir
    #    if servoDir:
    #        servo -= 10
    #    else:
    #        servo += 10
    #    
    #    rsport.write([servo])