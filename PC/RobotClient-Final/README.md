# Код для ПК (в случае с тестами - L4T Ubuntu) (RobotArm)

Прога, написанная на коленке и через задницу, но она хотя бы работает :grinning:

## Установка
Установка идёт в несколько шагов:
### 1. Установка CMake, git.
Откройте терминал и введите:
```
sudo apt install cmake git
```
### 2. Установка OpenCV 3.
Откройте терминал и поочерёдно введите команды:
```sh
mkdir ~/opencv3 && cd ~/opencv3
git clone https://github.com/opencv/opencv.git
cd opencv
git checkout 3.3.1
cd  ..
git clone https://github.com/opencv/opencv_contrib.git
cd opencv_contrib
git checkout 3.3.1
cd ..
cd opencv
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=RELEASE \
      -D CMAKE_INSTALL_PREFIX=/usr/local \
      -D WITH_V4L=ON \
      -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules ..
make -j$(nproc)
sudo make install
```
### 3. Установка PSMoveAPI.
Откройте терминал и поочерёдно введите команды:
```sh
mkdir ~/psmoveapi && cd ~/psmoveapi
git clone https://github.com/thp/psmoveapi.git
cd psmoveapi
mkdir build && cd build
cmake -D PSMOVE_USE_PSEYE=ON ..
make
sudo make install
```
### 4. Установка этой проги.
Откройте терминал и поочерёдно введите команды:
```
cd
git clone https://github.com/dex22044/RobotArm
cd RobotArm/PC/RobotClient-Final
make
bash doShit.sh
```
## Использование
Тут сложно объяснить словами, поэтому вот вам [видео](https://youtube.com/watch?v=OH_MAH_ASS).