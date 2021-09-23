//Old tracking code
#include <iostream>
#include <string>
#include <cassert>
#include <gtk/gtk.h>
#include <psmoveapi.h>
#include <psmove_tracker.h>
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
GtkColorChooserWidget* psMoveColorSelector;
GtkButton* psMoveDetectBtn;

using namespace std;
using namespace cv;

VideoCapture remoteCam;
VideoCapture localCam;

bool running = true;
int cr = 0, cg = 0, cb = 0;
int tr = 0, tg = 0, tb = 0;
float x = 0, y = 0, z = 0;
Mat rangedImg;
Mat rangedFilteredImg;
bool capFlag = false;
cv::Mat localCamImg;
cv::Mat rgbLocalCamImg;
cv::Mat hsvLocalCamImg;

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

struct PSMoveTracker_DistanceParameters {
    float height;
    float center;
    float hwhm;
    float shape;
};

static struct PSMoveTracker_DistanceParameters pseye_distance_parameters = {
    /* height = */ 517.281f,
    /* center = */ 1.297338f,
    /* hwhm = */ 3.752844f,
    /* shape = */ 0.4762335f,
};

float distance_from_radius(float radius)
{
    double height = (double)pseye_distance_parameters.height;
    double center = (double)pseye_distance_parameters.center;
    double hwhm = (double)pseye_distance_parameters.hwhm;
    double shape = (double)pseye_distance_parameters.shape;
    double x = (double)radius;

    double a = pow((x - center) / hwhm, 2.);
    double b = pow(2., 1. / shape) - 1.;
    double c = 1. + a * b;
    double distance = height / pow(c, shape);

    return (float)distance;
}

void localVideoUpdateThreadProc() {
    uchar* pixels = gdk_pixbuf_get_pixels(localCamPixbuf);
    int width = gdk_pixbuf_get_width(localCamPixbuf);
    int height = gdk_pixbuf_get_height(localCamPixbuf);
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;
    float xfov = 75;
    float yfov = xfov / 4.0 * 3.0;
    while(running) {
        localCam >> localCamImg;
        cvtColor(localCamImg, rgbLocalCamImg, COLOR_RGB2BGR);

        inRange(rgbLocalCamImg,
                cv::Scalar(max(tr - 100, 0), max(tg - 100, 0), max(tb - 100, 0)),
                cv::Scalar(min(tr + 100, 255), min(tg + 100, 255), min(tb + 100, 255)),
                rangedImg);
        erode(rangedImg, rangedFilteredImg, getStructuringElement(MORPH_RECT, Size(3, 3)));
        dilate(rangedFilteredImg, rangedFilteredImg, getStructuringElement(MORPH_RECT, Size(3, 3)));
        findContours(rangedFilteredImg, contours, hierarchy, RETR_LIST, CHAIN_APPROX_NONE);
        Rect maxRect = boundingRect(contours[0]);
        for(int i = 0; i < contours.size(); i++) {
            Rect boundingBox = boundingRect(contours[i]);
            if(boundingBox.width > maxRect.width && boundingBox.height > maxRect.height) maxRect = boundingBox;
        }
        rectangle(rgbLocalCamImg, maxRect.tl(), maxRect.br(), Scalar(255, 0, 0), 2);
        char* buf = new char[128];
        float radius = max(maxRect.width, maxRect.height) / 2.0;
        float xdeg = map_val(maxRect.x + maxRect.width / 2.0, 0, 640, -xfov / 2.0, xfov / 2.0);
        float ydeg = map_val(maxRect.y + maxRect.height / 2.0, 0, 640, -yfov / 2.0, yfov / 2.0);
        float dist = distance_from_radius(radius);
        snprintf(buf, 128, "%1.1fpx; %1.2fcm", radius, dist);
        putText(rgbLocalCamImg, buf, maxRect.tl() - Point(0, 23), FONT_HERSHEY_SIMPLEX, 0.5f,
                Scalar(255, 0, 0), 1);

        snprintf(buf, 128, "x: %1.2fdeg; y: %1.2fdeg", xdeg, ydeg);
        putText(rgbLocalCamImg, buf, maxRect.tl() - Point(0, 10), FONT_HERSHEY_SIMPLEX, 0.5f,
                Scalar(255, 0, 0), 1);

        x = dist * sin(xdeg * DEG2RAD) * cos(ydeg * DEG2RAD);
        y = dist * sin(ydeg * DEG2RAD);
        z = dist * cos(xdeg * DEG2RAD) * cos(ydeg * DEG2RAD);

        snprintf(buf, 128, "xyz: %3.1f %3.1f %3.1f", x, y, z);
        putText(rgbLocalCamImg, buf, maxRect.tl() - Point(0, -3), FONT_HERSHEY_SIMPLEX, 0.5f,
                Scalar(255, 0, 0), 1);

        memcpy(pixels, rgbLocalCamImg.data, width * height * 3);
        capFlag = true;
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
    GdkRGBA col = {0};
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(psMoveColorSelector), &col);
    cr = col.red * 255;
    cg = col.green * 255;
    cb = col.blue * 255;
    psmove_set_leds(selectedController, cr, cg, cb);
    psmove_update_leds(selectedController);
}

void psMoveDetect() {
    if(!capFlag) return;
    ColorRGB* pixels = (ColorRGB*)rgbLocalCamImg.data;
    float minr = 1000;
    for(int i = 0; i < 640 * 480; i++) {
        int rr = abs((int)pixels[i].r - cr);
        int rg = abs((int)pixels[i].g - cg);
        int rb = abs((int)pixels[i].b - cb);
        float r = sqrt(rr * rr + rg * rg + rb * rb);
        if(r < minr) {
            tr = (int)pixels[i].r;
            tg = (int)pixels[i].g;
            tb = (int)pixels[i].b;
            minr = r;
        }
    }
    printf("%5.2f; %d %d %d\n", minr, tr, tg, tb);
}

void setGain() {
    system((string("v4l2-ctl -d \"/dev/v4l/by-id/usb-OmniVision_Technologies__Inc._USB_Camera-B4.09.24.1-video-index0\" --set-ctrl gain_automatic=0 --set-ctrl gain=")
            + to_string((int)gtk_range_get_value(GTK_RANGE(psEyeImageGainSlider)))).c_str());
}

void setExposure() {
    system((string("v4l2-ctl -d \"/dev/v4l/by-id/usb-OmniVision_Technologies__Inc._USB_Camera-B4.09.24.1-video-index0\" --set-ctrl auto_exposure=1 --set-ctrl exposure=")
            + to_string((int)gtk_range_get_value(GTK_RANGE(psEyeImageExposureSlider)))).c_str());
}

int main(int argc, char** argv)
{
    remoteCam.open("/dev/v4l/by-id/usb-ARKMICRO_USB2.0_PC_CAMERA-video-index0");
    if(!remoteCam.isOpened()) {
        cout << "RC receiver is not detected\n";
        exit(-1);
    }

    localCam.open("/dev/v4l/by-id/usb-OmniVision_Technologies__Inc._USB_Camera-B4.09.24.1-video-index0");
    if(!localCam.isOpened()) {
        cout << "PS3Eye is not detected\n";
        exit(-1);
    }

    initInterface();

    system("v4l2-ctl -d \"/dev/v4l/by-id/usb-OmniVision_Technologies__Inc._USB_Camera-B4.09.24.1-video-index0\" --set-ctrl auto_exposure=1 --set-ctrl exposure=20");

    remoteCamPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, remoteCam.get(CAP_PROP_FRAME_WIDTH), remoteCam.get(CAP_PROP_FRAME_HEIGHT));
    localCamPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, localCam.get(CAP_PROP_FRAME_WIDTH), localCam.get(CAP_PROP_FRAME_HEIGHT));

    psmove_init(PSMOVE_CURRENT_VERSION);

	gtk_init(&argc, &argv);
    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "Windows.glade", NULL);

    GObject* window = gtk_builder_get_object(builder, "window");
    remoteCamBox = GTK_IMAGE(gtk_builder_get_object(builder, "remoteCamBox"));
    localCamBox = GTK_IMAGE(gtk_builder_get_object(builder, "localCamBox"));
    psMoveSelectorComboBox = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "psMoveSelectorComboBox"));
    psMoveRefreshBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveRefreshBtn"));
    psEyeImageGainSlider = GTK_SCALE(gtk_builder_get_object(builder, "psEyeImageGainSlider"));
    psEyeImageExposureSlider = GTK_SCALE(gtk_builder_get_object(builder, "psEyeImageExposureSlider"));
    psMoveSelectControllerBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveSelectControllerBtn"));
    psMoveColorSelector = GTK_COLOR_CHOOSER_WIDGET(gtk_builder_get_object(builder, "psMoveColorSelector"));
    psMoveDetectBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveDetectBtn"));

    gtk_widget_show(GTK_WIDGET(window));

    g_signal_connect(window, "destroy", G_CALLBACK(quit), NULL);
    g_signal_connect(psMoveRefreshBtn, "clicked", G_CALLBACK(psMoveRefreshControllers), NULL);
    g_signal_connect(psEyeImageGainSlider, "value-changed", G_CALLBACK(setGain), NULL);
    g_signal_connect(psEyeImageExposureSlider, "value-changed", G_CALLBACK(setExposure), NULL);
    g_signal_connect(psMoveSelectControllerBtn, "clicked", G_CALLBACK(psMoveSelectController), NULL);
    g_signal_connect(psMoveDetectBtn, "clicked", G_CALLBACK(psMoveDetect), NULL);
    
    thread remoteVideoUpdateThread(remoteVideoUpdateThreadProc);
    thread localVideoUpdateThread(localVideoUpdateThreadProc);

    int colorUpdateFrameCounter = 0;

    while(running) {
        colorUpdateFrameCounter++;
        if(colorUpdateFrameCounter >= 10) {
            psMoveSetColor();
            writeMotors(map_val(x, -100, 100, 0, 180), map_val(y, -100, 100, 0, 180), 0);
        }

        gtk_image_set_from_pixbuf(remoteCamBox, remoteCamPixbuf);
        gtk_image_set_from_pixbuf(localCamBox, localCamPixbuf);
        if(capFlag) imshow("ranged image", rangedFilteredImg);
        while (gtk_events_pending()) gtk_main_iteration();
        usleep(10000);
    }

    return 0;
}
//End old tracking code