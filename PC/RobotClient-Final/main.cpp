#include <iostream>
#include <string>
#include <cassert>
#include <gtk/gtk.h>
#include <psmoveapi.h>
#include <psmove_tracker.h>
#include <psmove_fusion.h>
#include <vector>
#include <thread>
#include <string>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include "motorInterface.h"

#define DEG2RAD 0.01745329251

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

using namespace std;
using namespace cv;

VideoCapture remoteCam;
PSMoveTracker* localCam;
PSMoveFusion* localCamFusion;

vector<PSMove*> connectedMoveControllers;
PSMove* selectedController;

bool running = true;
bool capFlag = false;

float map_val(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

struct ColorRGB {
    unsigned char r, g, b;
};

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

int cnt = 0;

void localVideoUpdateThreadProc() {
    uchar* pixels = gdk_pixbuf_get_pixels(localCamPixbuf);
    int width = gdk_pixbuf_get_width(localCamPixbuf);
    int height = gdk_pixbuf_get_height(localCamPixbuf);
    float x = 0, y = 0, z = 0;
    while(running) {
        psmove_tracker_update_image(localCam);
        psmove_tracker_update(localCam, NULL);
        psmove_tracker_annotate(localCam);
        PSMoveTrackerRGBImage image = psmove_tracker_get_image(localCam);

        cnt++;
        if(cnt >= 1 && selectedController != NULL) {
            psmove_fusion_get_position(localCamFusion, selectedController, &x, &y, &z);
            writeMotors(map_val(x, -10, 10, 0, 180), map_val(y, -10, 10, 0, 180), 0);
            cnt = 0;
        }

        memcpy(pixels, image.data, width * height * 3);
        capFlag = true;
    }
}

void quit() {
    running = false;
    exit(0);
}

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
    if(selectedController != NULL) psmove_tracker_disable(localCam, selectedController);
    psmove_tracker_enable(localCam, connectedMoveControllers[idx]);
    selectedController = connectedMoveControllers[idx];
}

int main(int argc, char** argv)
{
    remoteCam.open("/dev/v4l/by-id/usb-ARKMICRO_USB2.0_PC_CAMERA-video-index0");
    if(!remoteCam.isOpened()) {
        cout << "RC receiver is not detected\n";
        exit(-1);
    }

    PSMoveTrackerSettings settings;
    psmove_tracker_settings_set_default(&settings);
    settings.color_mapping_max_age = 0;
	settings.exposure_mode = Exposure_LOW;
	settings.camera_mirror = PSMove_False;
    localCam = psmove_tracker_new_with_camera_and_settings(0, &settings);
    if(localCam == NULL) {
        cout << "PS3Eye is not detected\n";
        exit(-1);
    }

    localCamFusion = psmove_fusion_new(localCam, 5, 500);

    initInterface();

    remoteCamPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, remoteCam.get(CAP_PROP_FRAME_WIDTH), remoteCam.get(CAP_PROP_FRAME_HEIGHT));
    localCamPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, 640, 480);

    psmove_init(PSMOVE_CURRENT_VERSION);

	gtk_init(&argc, &argv);
    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "Windows.glade", NULL);

    GObject* window = gtk_builder_get_object(builder, "window");
    remoteCamBox = GTK_IMAGE(gtk_builder_get_object(builder, "remoteCamBox"));
    localCamBox = GTK_IMAGE(gtk_builder_get_object(builder, "localCamBox"));
    psMoveSelectorComboBox = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "psMoveSelectorComboBox"));
    psMoveRefreshBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveRefreshBtn"));
    psMoveSelectControllerBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveSelectControllerBtn"));

    gtk_widget_show(GTK_WIDGET(window));

    g_signal_connect(window, "destroy", G_CALLBACK(quit), NULL);
    g_signal_connect(psMoveRefreshBtn, "clicked", G_CALLBACK(psMoveRefreshControllers), NULL);
    g_signal_connect(psMoveSelectControllerBtn, "clicked", G_CALLBACK(psMoveSelectController), NULL);
    
    thread remoteVideoUpdateThread(remoteVideoUpdateThreadProc);
    thread localVideoUpdateThread(localVideoUpdateThreadProc);

    int positionUpdateCounter = 0;

    while(running) {
        gtk_image_set_from_pixbuf(remoteCamBox, remoteCamPixbuf);
        gtk_image_set_from_pixbuf(localCamBox, localCamPixbuf);
        while (gtk_events_pending()) gtk_main_iteration();
        usleep(10000);
    }

    return 0;
}