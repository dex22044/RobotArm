all:
	g++ -o app.bin -std=c++17 \
	main.cpp motorInterface.cpp \
	-I /usr/local/include/psmoveapi \
	-I /usr/include/gtk-3.0 \
	-I /usr/local/include/opencv4 \
	-L /usr/local/lib \
	-L /usr/local/lib/aarch64-linux-gnu/ \
	-l gtk-3 -l psmoveapi -l opencv_core -l opencv_videoio -l opencv_imgproc \
	`pkg-config --cflags --libs gtk+-3.0`