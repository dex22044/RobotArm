# Код для ПК (в случае с тестами - L4T Ubuntu) (RobotArm)

Прога, написанная на коленке и через задницу, но она хотя бы работает :grinning:

## Установка
Установка идёт в несколько шагов:
### 1. Установка CMake, git, python3, g++ и другой фигни.
CMake: откройте терминал и введите команды:
```sh
sudo apt install libssl-dev
mkdir ~/cmake && cd ~/cmake
git clone --recursive https://github.com/kitware/cmake.git
cd cmake
./configure
make -j$(nproc)
sudo make install
```
Откройте терминал и введите:
```sh
sudo apt install git g++-8 python3 build-essential cmake git pkg-config libgtk-3-dev libavcodec-dev libavformat-dev libswscale-dev libv4l-dev libxvidcore-dev libx264-dev libjpeg-dev libpng-dev libtiff-dev gfortran openexr libatlas-base-dev python3-dev python3-numpy libtbb2 libtbb-dev libdc1394-22-dev libudev-dev libbluetooth-dev libusb-dev libhidapi-dev
```
### 2. Установка OpenCV 3.300
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
      -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules \
      -WITH_GTK=ON \
      -WITH_QT=OFF ..
make -j$(nproc)
sudo make install
```
### 3. Установка PSMoveAPI.
Откройте терминал и поочерёдно введите команды:
```sh
mkdir ~/psmoveapi && cd ~/psmoveapi
git clone --recursive https://github.com/thp/psmoveapi.git
cd psmoveapi
bash scripts/install_dependencies.sh
mkdir build && cd build
cmake -DCMAKE_C_COMPILER="/usr/bin/gcc-8"
      -DCMAKE_CXX_COMPILER="/usr/bin/g++-8"
      -DPSMOVE_USE_PSEYE=ON ..
make -j$(nproc)
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
