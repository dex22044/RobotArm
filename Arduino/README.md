# Код для Arduino (RobotArm)

Код для [приёмника](https://github.com/dex22044/RobotArm/Arduino/Receiver) и для [передатчика](https://github.com/dex22044/RobotArm/Arduino/Sender).

## Передатчик
Для загрузки прошивки передатчика нужна библиотека RF24, которую можно скачать из менеджера библиотек Arduino IDE.

### Подключение nRF24L01+
Arduino | nRF24L01+
--------|----------
+5V | VCC
GND | GND
D9 | CSN
D10 | CE
D11 | M0
D12 | M1
D13 | SCK

## Приёмник
Для загрузки прошивки приёмника нужно ядро GyverCore, про установку которого можно поискать в интернете. После этого в настройках нужно выбрать "Clock frequency" на "External 20MHz". Также, нужны библиотеки RF24 и GyverTimers, которые можно скачать из менеджера библиотек Arduino IDE.