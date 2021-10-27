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
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <epoxy/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include "motorInterface.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstdio>

bool SetSocketBlockingEnabled(int fd, bool blocking)
{
   if (fd < 0) return false;

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) return false;
   flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
}

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

GtkListBox* pointsListBox;

using namespace std;
using namespace cv;

VideoCapture remoteCam;
PSMoveTracker* localCam;
PSMoveFusion* localCamFusion;

vector<PSMove*> connectedMoveControllers;
PSMove* selectedController;
FILE* voiceSocket = NULL;

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

void ComputeAngles();

glm::vec3 targPos;
glm::vec3 movePos;
glm::vec3 moveCenter;
float angle1 = 0, angle2 = 0, angle3 = 0;
vector<glm::vec3> savedPoints;
int mode = 0;
int angle4 = 0, angle5 = 0;
int angleChangeMode = 0;

void AddPoint(glm::vec3 point) {
    savedPoints.push_back(glm::vec3(point));
    char textbuf[64];
    snprintf(textbuf, 64, "Точка %d (%2.2f, %2.2f, %2.2f)", savedPoints.size(), point.x, point.y, point.z);
    GtkLabel* label = GTK_LABEL(gtk_label_new(textbuf));
    gtk_widget_show(GTK_WIDGET(label));
    gtk_list_box_insert(pointsListBox, GTK_WIDGET(label), -1);
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, savedPoints.size() - 1));
}

glm::vec3 GetSelectedPoint() {
    int idx = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(pointsListBox));
    return savedPoints[idx];
}

void SelectPoint(int pointIdx) {
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, pointIdx));
}

void SelectPrevPoint() {
    printf("prev point\n");
    int idx = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(pointsListBox));
    idx--;
    if(idx < 0) idx = savedPoints.size() - 1;
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, idx));
}

void SelectNextPoint() {
    printf("next point\n");
    int idx = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(pointsListBox));
    idx++;
    if(idx > savedPoints.size() - 1) idx = 0;
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, idx));
}

void voiceUpdaterThreadProc() {
    char speechRecv = -1;
    while(true) {
        if(fread(&speechRecv, 1, 1, voiceSocket) > 0) {
            printf("%c\n", speechRecv);
            mode = 2;
            if(speechRecv == '3') SelectPoint(0);
        }
    }
}

void localVideoUpdateThreadProc() {
    uchar* pixels = gdk_pixbuf_get_pixels(localCamPixbuf);
    int width = gdk_pixbuf_get_width(localCamPixbuf);
    int height = gdk_pixbuf_get_height(localCamPixbuf);
    char trigger = 0;
    int buttons = 0;
    int prevButtons = 0;
    int cnt = 0;
    while(running) {
        psmove_tracker_update_image(localCam);
        psmove_tracker_update(localCam, NULL);
        psmove_tracker_annotate(localCam);
        PSMoveTrackerRGBImage image = psmove_tracker_get_image(localCam);

        cnt++;
        if(cnt >= 1 && selectedController != NULL) {
            while(psmove_poll(selectedController));
            trigger = psmove_get_trigger(selectedController);
            psmove_fusion_get_position(localCamFusion, selectedController, &movePos.x, &movePos.y, &movePos.z);
            unsigned int buttons = psmove_get_buttons(selectedController);
			if (buttons & Btn_MOVE && !(prevButtons & Btn_MOVE)) {
				if(mode == 1) mode = 0;
                else mode = 1;
				moveCenter.x = movePos.x;
				moveCenter.y = movePos.y;
				moveCenter.z = movePos.z;
			}

            movePos.x -= moveCenter.x;
            movePos.y -= moveCenter.y;
            movePos.z -= moveCenter.z;
            movePos.y *= 3;

            if(mode == 2) {
                glm::vec3 pt = GetSelectedPoint();
                movePos.x = pt.x;
                movePos.y = pt.y;
                movePos.z = pt.z;
            }

            if(buttons & Btn_CROSS && !(prevButtons & Btn_CROSS)) {
                AddPoint(movePos);
            }

            if(buttons & Btn_CIRCLE && !(prevButtons & Btn_CIRCLE)) {
                mode = 2;
            }

            if(buttons & Btn_SQUARE && !(prevButtons & Btn_SQUARE)) SelectPrevPoint();
            if(buttons & Btn_TRIANGLE && !(prevButtons & Btn_TRIANGLE)) SelectNextPoint();
            if(buttons & Btn_PS && !(prevButtons & Btn_PS)) angleChangeMode ^= 1;

            if(angleChangeMode) {
                if(buttons & Btn_START) angle4 += 3;
                if(buttons & Btn_SELECT) angle4 -= 3;
                if(angle4 < 0) angle4 = 0;
                if(angle4 > 180) angle4 = 180;
            }
            else {
                if(buttons & Btn_START) angle5+= 3;
                if(buttons & Btn_SELECT) angle5 -= 3;
                if(angle5 < 0) angle5 = 0;
                if(angle5 > 180) angle5 = 180;
            }

			prevButtons = buttons;
            
            movePos.y = -movePos.y;

            ComputeAngles();

            printf("%f\n", angle3);
            
            if(mode != 0) writeMotors(angle1, angle2, angle4, angle5, map_val(trigger, 0, 255, 0, 110), map_val(angle3, 0, 360, 0, 200));
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

GLdouble triangleVerts[] = {
    -1.0, -1.0, 1.0,
    0.0, 1.0, 1.0,
    1.0, -1.0, 1.0
};

void ComputeAngles() {
    float a = 215;
    float b = 160;
    float c = clamp(sqrt(movePos.x * movePos.x + movePos.y * movePos.y + movePos.z * movePos.z) * 10, 100.0f, 350.0f);

    float alpha = acos((b * b + c * c - a * a) / (2 * b * c));
    float gamma = acos((a * a + b * b - c * c) / (2 * a * b));

    float angle = atan2(clamp(sqrt(movePos.x * movePos.x + movePos.z * movePos.z), 10.0f, 1000.0f), movePos.y);

    angle1 = 360 - alpha / DEG2RAD - angle / DEG2RAD + 90;
    angle2 = 180 - (alpha + gamma) / DEG2RAD - angle / DEG2RAD + 90;

    angle2 += 90;
    
    angle1 -= 180;

    angle3 = -atan2(-movePos.x, movePos.z) / DEG2RAD;

    //printf("Servoes: %d %d %d\n", (int)angle1, (int)angle2, (int)angle3);
}

#pragma region some opengl shit that will be probably deleted later
GLuint vbo, vao, vecShader, shader;

void setVec2Uniform(char* name, GLfloat x, GLfloat y) {
    glUniform2f(glGetUniformLocation(shader, name), x, y);
}

void init_buffer_objects() {
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER, sizeof(triangleVerts),
        triangleVerts, GL_STATIC_DRAW
    );

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 0); // unbind VBO
    glBindVertexArray(0); // unbind VAO
}

static void drawTriangle() {
    glUseProgram(shader);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINE_STRIP, 0, 3);
    glDisableVertexAttribArray(0);
}

float red = 0;

const char* vtxShaderSource = ""
"#version 330 core\n"
"uniform vec2 pos1;\n"
"uniform vec2 pos2;\n"
"uniform vec2 pos3;\n"
"layout(location=0) in vec3 vertexPos;\n"
"void main() {\n"
"if(gl_VertexID == 0) gl_Position = vec4(pos1, 1.0, 1.0);\n"
"if(gl_VertexID == 1) gl_Position = vec4(pos2, 1.0, 1.0);\n"
"if(gl_VertexID == 2) gl_Position = vec4(pos3, 1.0, 1.0);\n"
"}";

const char* fragShaderSource = ""
"#version 330 core\n"
"out vec3 color;\n"
"void main() { color = vec3(1, 1, 1); }";

#pragma region shaders...
bool checkShaderCompileStatus(GLuint obj) {
  GLint status;
  glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
  if(status == GL_FALSE) {
    GLint length;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log((unsigned long)length);
    glGetShaderInfoLog(obj, length, &length, &log[0]);
    std::cerr << &log[0];
    return true;
  }
  return false;
}

bool checkProgramLinkStatus(GLuint obj) {
  GLint status;
  glGetProgramiv(obj, GL_LINK_STATUS, &status);
  if(status == GL_FALSE) {
    GLint length;
    glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log((unsigned long)length);
    glGetProgramInfoLog(obj, length, &length, &log[0]);
    std::cerr << &log[0];
    return true;
  }
  return false;
}

GLuint prepareProgram(bool *errorFlagPtr) {
  *errorFlagPtr = false;

  GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
  vecShader = vertexShaderId;
  const GLchar * const vertexShaderSourcePtr = vtxShaderSource;
  glShaderSource(vertexShaderId, 1, &vertexShaderSourcePtr, nullptr);
  glCompileShader(vertexShaderId);

  *errorFlagPtr = checkShaderCompileStatus(vertexShaderId);
  if(*errorFlagPtr) return 0;

  GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
  const GLchar * const fragmentShaderSourcePtr = fragShaderSource;
  glShaderSource(fragmentShaderId, 1, &fragmentShaderSourcePtr, nullptr);
  glCompileShader(fragmentShaderId);

  *errorFlagPtr = checkShaderCompileStatus(fragmentShaderId);
  if(*errorFlagPtr) return 0;

  GLuint programId = glCreateProgram();
  glAttachShader(programId, vertexShaderId);
  glAttachShader(programId, fragmentShaderId);
  glLinkProgram(programId);

  *errorFlagPtr = checkProgramLinkStatus(programId);
  if(*errorFlagPtr) return 0;

  glDeleteShader(vertexShaderId);
  glDeleteShader(fragmentShaderId);

  return programId;
}
#pragma endregion

static gboolean render(GtkGLArea *area, GdkGLContext *context)
{
    float a = 215;
    float b = 160;

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glm::vec2 pos1 = glm::vec2(sin((angle1 - 90)* DEG2RAD) * b,
                    cos(((angle1 - 90) * DEG2RAD)) * b);

    glm::vec2 pos2 = glm::vec2(sin(angle2 * DEG2RAD) * a,
                    cos(angle2 * DEG2RAD) * a)
                    + pos1;

    setVec2Uniform("pos1", 0.0f, 0.0f);
    setVec2Uniform("pos2", pos1.x / 320.0, pos1.y / 180.0);
    setVec2Uniform("pos3", pos2.x / 320.0, pos2.y / 180.0);
    drawTriangle();

    return TRUE;
}

static void on_realize(GtkGLArea *area)
{
    gtk_gl_area_make_current (area);

    if (gtk_gl_area_get_error (area) != NULL)
        return;

    GError *error = NULL;
    
    init_buffer_objects();    
    shader = prepareProgram((bool*)&error);
        
}

void setup_glarea(GtkWidget* glarea)
{
  // create a GtkGLArea instance
  GtkWidget *gl_area = glarea;

  // connect to the "render" signal
  g_signal_connect(gl_area, "render", G_CALLBACK(render), NULL);
  g_signal_connect(gl_area, "realize", G_CALLBACK(on_realize), NULL);
}
#pragma endregion

int main(int argc, char** argv) {
    #pragma region Video devices
    remoteCam.open("/dev/v4l/by-id/usb-ARKMICRO_USB2.0_PC_CAMERA-video-index0");
    if(!remoteCam.isOpened()) {
        cout << "RC receiver is not detected\n";
        //exit(-1);
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
    #pragma endregion

    localCamFusion = psmove_fusion_new(localCam, 5, 500);

    initInterface();

    remoteCamPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, remoteCam.get(CAP_PROP_FRAME_WIDTH), remoteCam.get(CAP_PROP_FRAME_HEIGHT));
    localCamPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, 640, 480);

    psmove_init(PSMOVE_CURRENT_VERSION);

    #pragma region GTK initialization shit
	gtk_init(&argc, &argv);
    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "Windows.glade", NULL);

    GObject* window = gtk_builder_get_object(builder, "window");
    remoteCamBox = GTK_IMAGE(gtk_builder_get_object(builder, "remoteCamBox"));
    localCamBox = GTK_IMAGE(gtk_builder_get_object(builder, "localCamBox"));
    psMoveSelectorComboBox = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "psMoveSelectorComboBox"));
    psMoveRefreshBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveRefreshBtn"));
    psMoveSelectControllerBtn = GTK_BUTTON(gtk_builder_get_object(builder, "psMoveSelectControllerBtn"));
    GtkGLArea* glArea = GTK_GL_AREA(gtk_builder_get_object(builder, "glArea"));
    pointsListBox = GTK_LIST_BOX(gtk_builder_get_object(builder, "pointsListBox"));
    setup_glarea(GTK_WIDGET(glArea));

    gtk_widget_show(GTK_WIDGET(window));

    voiceSocket = popen("/usr/bin/python3 /home/dex22044/RobotArm/PC/RobotClient-Final/speech.py", "r");
    printf("ass\n");

    AddPoint(glm::vec3(-3.6, -4.46, 11.47));

    g_signal_connect(window, "destroy", G_CALLBACK(quit), NULL);
    g_signal_connect(psMoveRefreshBtn, "clicked", G_CALLBACK(psMoveRefreshControllers), NULL);
    g_signal_connect(psMoveSelectControllerBtn, "clicked", G_CALLBACK(psMoveSelectController), NULL);
    #pragma endregion

    thread remoteVideoUpdateThread(remoteVideoUpdateThreadProc);
    thread localVideoUpdateThread(localVideoUpdateThreadProc);
    thread voiceUpdaterThread(voiceUpdaterThreadProc);
    

    while(running) {
        gtk_image_set_from_pixbuf(remoteCamBox, remoteCamPixbuf);
        gtk_image_set_from_pixbuf(localCamBox, localCamPixbuf);

        gtk_gl_area_queue_render(glArea);

        while (gtk_events_pending()) gtk_main_iteration();

        usleep(10000);
    }

    return 0;
}