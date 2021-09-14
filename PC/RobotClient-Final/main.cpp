#include <iostream>
#include <string>
#include <cassert>
#include <gtk/gtk.h>
#include <psmoveapi.h>
#include <vector>
#include <thread>
#include <string>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include "motorInterface.h"

int prevPages = 0;

GdkPixbuf* remoteCamPixbuf;
GtkImage* remoteCamBox;

GdkPixbuf* localCamPixbuf;
GtkImage* localCamBox;

GtkComboBoxText* psMoveSelectorComboBox;
GtkButton* psMoveRefreshBtn;

GtkScale* psEyeImageGainSlider;
GtkScale* psEyeImageExposureSlider;

GtkButton* psMoveSelectControllerBtn;
GtkColorChooserWidget* psMoveColorSelector;

using namespace std;
using namespace cv;

VideoCapture remoteCam;
VideoCapture localCam;

bool running = true;

void remoteVideoUpdateThreadProc() {
    uchar* pixels = gdk_pixbuf_get_pixels(remoteCamPixbuf);
    int width = gdk_pixbuf_get_width(remoteCamPixbuf);
    int height = gdk_pixbuf_get_height(remoteCamPixbuf);
    cv::Mat camImg;
    cv::Mat rgbCamImg;
    while(running) {
        remoteCam >> camImg;
        cvtColor(camImg, rgbCamImg, COLOR_RGB2BGR);
        memcpy(pixels, rgbCamImg.data, width * height * 3);
    }
}

void localVideoUpdateThreadProc() {
    uchar* pixels = gdk_pixbuf_get_pixels(localCamPixbuf);
    int width = gdk_pixbuf_get_width(localCamPixbuf);
    int height = gdk_pixbuf_get_height(localCamPixbuf);
    cv::Mat camImg;
    cv::Mat rgbCamImg;
    while(running) {
        localCam >> camImg;
        cvtColor(camImg, rgbCamImg, COLOR_RGB2BGR);
        memcpy(pixels, rgbCamImg.data, width * height * 3);
    }
}

void quit() {
    running = false;
    exit(0);
}

vector<PSMove*> connectedMoveControllers;
PSMove* selectedController;

void psMoveRefreshControllers() {
    for(int i = 0; i < connectedMoveControllers.size(); i++) {
        gtk_combo_box_text_remove(psMoveSelectorComboBox, 0);
    }
    connectedMoveControllers.clear();
    for(int i = 0; i < psmove_count_connected(); i++) {
        PSMove* move = psmove_connect_by_id(i);
        connectedMoveControllers.push_back(move);
        string connType;
        switch(psmove_connection_type(move)) {
            case Conn_USB: connType = " (USB)"; break;
            case Conn_Bluetooth: connType = " (BT)"; break;
            case Conn_Unknown: connType = " (N/A)"; break;
        }
        gtk_combo_box_text_append_text(psMoveSelectorComboBox, (string("PSMove ") + to_string(i + 1) + connType).c_str());
    }
}

void psMoveSelectController() {
    int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(psMoveSelectorComboBox));
    if(idx == -1) return;
    selectedController = connectedMoveControllers[idx];
}

void psMoveSetColor() {
    if(selectedController == NULL) return;
    char r = 0, g = 0, b = 0;
    GdkRGBA col = {0};
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(psMoveColorSelector), &col);
    r = col.red * 255;
    g = col.green * 255;
    b = col.blue * 255;
    psmove_set_leds(selectedController, r, g, b);
    psmove_update_leds(selectedController);
}

void setGain() {
    localCam.set(CAP_PROP_GAIN, gtk_range_get_value(GTK_RANGE(psEyeImageGainSlider)));
}

void setExposure() {
    system((string("v4l2-ctl -d /dev/video1 --set-ctrl auto_exposure=1 --set-ctrl exposure=")
            + to_string((int)gtk_range_get_value(GTK_RANGE(psEyeImageExposureSlider)))).c_str());
}

int main(int argc, char** argv)
{
    remoteCam.open("/dev/video0");
    if(!remoteCam.isOpened()) {
        cout << "/dev/video0 is not detected\n";
        exit(-1);
    }

    localCam.open("/dev/video1");
    if(!localCam.isOpened()) {
        cout << "/dev/video1 is not detected\n";
        exit(-1);
    }

    system("v4l2-ctl -d /dev/video1 --set-ctrl auto_exposure=1 --set-ctrl exposure=20");

    remoteCamPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, remoteCam.get(CAP_PROP_FRAME_WIDTH), remoteCam.get(CAP_PROP_FRAME_HEIGHT));
    localCamPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, localCam.get(CAP_PROP_FRAME_WIDTH), localCam.get(CAP_PROP_FRAME_HEIGHT));

    psmove_init(PSMOVE_CURRENT_VERSION);

	gtk_init(&argc, &argv);
    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "Windows.glade", NULL);

    GObject* window1 = gtk_builder_get_object(builder, "window1");
    GObject* window2 = gtk_builder_get_object(builder, "window2");
    remoteCamBox = GTK_IMAGE(gtk_builder_get_object(builder, "remoteCamBox"));
    localCamBox = GTK_IMAGE(gtk_builder_get_object(builder, "localCamBox"));
    psMoveSelectorComboBox = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "psMoveSelectorComboBox"));
    psMoveRefreshBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveRefreshBtn"));
    psEyeImageGainSlider = GTK_SCALE(gtk_builder_get_object(builder, "psEyeImageGainSlider"));
    psEyeImageExposureSlider = GTK_SCALE(gtk_builder_get_object(builder, "psEyeImageExposureSlider"));
    psMoveSelectControllerBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveSelectControllerBtn"));
    psMoveColorSelector = GTK_COLOR_CHOOSER_WIDGET(gtk_builder_get_object(builder, "psMoveColorSelector"));

    gtk_widget_show(GTK_WIDGET(window1));
    gtk_widget_show(GTK_WIDGET(window2));

    g_signal_connect(window1, "destroy", G_CALLBACK(quit), NULL);
    g_signal_connect(psMoveRefreshBtn, "clicked", G_CALLBACK(psMoveRefreshControllers), NULL);
    g_signal_connect(psEyeImageGainSlider, "value-changed", G_CALLBACK(setGain), NULL);
    g_signal_connect(psEyeImageExposureSlider, "value-changed", G_CALLBACK(setExposure), NULL);
    g_signal_connect(psMoveSelectControllerBtn, "clicked", G_CALLBACK(psMoveSelectController), NULL);
    
    thread remoteVideoUpdateThread(remoteVideoUpdateThreadProc);
    thread localVideoUpdateThread(localVideoUpdateThreadProc);

    int colorUpdateFrameCounter = 0;

    while(running) {
        colorUpdateFrameCounter++;
        if(colorUpdateFrameCounter >= 10) psMoveSetColor();

        gtk_image_set_from_pixbuf(remoteCamBox, remoteCamPixbuf);
        gtk_image_set_from_pixbuf(localCamBox, localCamPixbuf);
        while (gtk_events_pending()) gtk_main_iteration();
        usleep(10000);
    }

    return 0;
}