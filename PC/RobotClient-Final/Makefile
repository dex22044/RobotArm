client:
	g++ -o app.bin -std=c++17 \
	main.cpp motorInterface.cpp \
	-I /usr/local/include/psmoveapi \
	-I /usr/include/gtk-3.0 \
	-I ~/opencv3/opencv/build/include/ \
	-L ~/opencv3/opencv/build/lib/ \
	-L /usr/local/lib/aarch64-linux-gnu/ \
	-l gtk-3 -l psmoveapi -l psmoveapi_tracker -l opencv_core -l opencv_videoio -l opencv_imgproc -l opencv_highgui \
	`pkg-config --cflags --libs gtk+-3.0`