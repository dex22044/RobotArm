#pragma region includes
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
#include "motorInterface.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cstdio>
#pragma endregion

#define DEG2RAD 0.01745329251

int prevPages = 0;

#pragma region GUI shit
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
GtkSpinButton* velocityPercentageSpinBox;
GtkLabel* statusIndicatorLabel;
GtkSpinButton* cylinderRSpinBox;
GtkSpinButton* cylinderHSpinBox;
#pragma endregion

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
char coordsTextBuf[256];

int velocityPercentage = 15;
int cylinderR = 0;
int cylinderH = 0;

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

class ArmTrajectoryPoint {
public:
    glm::vec3 pos;
    float angle4 = 0, angle5 = 0, angleGrabber = 0;
    ArmTrajectoryPoint(glm::vec3 pos, float a4, float a5, float ag) : pos(pos), angle4(a4), angle5(a5), angleGrabber(ag) {};
};

class ArmTrajectory {
public:
    vector<ArmTrajectoryPoint> points;
    bool isPoint;
    ArmTrajectory(bool isPoint) : isPoint(isPoint) {};
};

glm::vec3 movePos; // Положение PSMove
glm::vec3 moveCenter; // Система отсчёта PSMove
glm::vec3 autoPos; // Положение автономного режима
glm::vec3 armPos; // Положение руки
glm::vec3 apprArmPos; // Сглаженное положение руки
glm::vec3 finArmPos; // Сглаженное положение руки
vector<ArmTrajectory*> savedTrajectories; // Массив запомненных точек
int mode = 0; // Режим управления
float angle1 = 0, angle2 = 0, angle3 = 0; // Углы двух основных серв и шаговика
int angle4 = 0, angle5 = 0, angleGrabber = 0; // Углы остальных серв
int angleGrabberAuto = 0; // Угол для автономного управления для захвата
int angleChangeMode = 0; // Режим изменения углов серв 4 и 5
ArmTrajectory* currentTrajectory = NULL; // Текущая тректория для записи
uint64_t trajectoryStartFrame = 0;

glm::vec3 Lerp(glm::vec3 start, glm::vec3 end, float t) {
    glm::vec3 p = start;
    p += (end - start) * t;
    return p;
}

#pragma region Grab shit

glm::vec3 boltPoint = glm::vec3(-8.71, -1.79, -4.24); // Положение болта

void GrabBolt() { // Цыганские фокусы (взять болт)
    int prevMode = mode;
    mode = 3;

    angle5 = 0;
    angleGrabberAuto = 0;
    usleep(300000);
    autoPos = glm::vec3(boltPoint - glm::vec3(0, 5, 0));
    usleep(10000000);
    autoPos = glm::vec3(boltPoint);
    usleep(1000000);
    angleGrabberAuto = 100;
    usleep(1000000);
    autoPos = glm::vec3(boltPoint - glm::vec3(0, 5, 0));
    usleep(1000000);

    mode = 1;
}

void ReleaseBolt() { // Цыганские фокусы (отпустить болт)
    int prevMode = mode;
    mode = 3;



    mode = 1;
}

#pragma endregion

void voiceUpdaterThreadProc() { // Поток с петухоном и распознованием речи
    char speechRecv = -1;
    while(true) {
        if(fread(&speechRecv, 1, 1, voiceSocket) > 0) {
            if(mode == 0) continue;
            printf("%c\n", speechRecv);
            if(speechRecv == '3') GrabBolt();
        }
    }
}

#pragma region Positioning shit
void AddPoint(glm::vec3 point, float a4, float a5, float ag) {
    ArmTrajectory* trj = new ArmTrajectory(true);
    currentTrajectory->points.push_back(ArmTrajectoryPoint(glm::vec3(point), a4, a5, ag));
    savedTrajectories.push_back(trj);
    char textbuf[64];
    snprintf(textbuf, 64, "Точка %d (%2.2f, %2.2f, %2.2f)", savedTrajectories.size(), point.x, point.y, point.z);
    GtkLabel* label = GTK_LABEL(gtk_label_new(textbuf));
    gtk_widget_show(GTK_WIDGET(label));
    gtk_list_box_insert(pointsListBox, GTK_WIDGET(label), -1);
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, savedTrajectories.size() - 1));
}

ArmTrajectoryPoint GetSelectedPoint() { // Взять текущую точку
    int idx = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(pointsListBox));
    return savedTrajectories[idx]->points[0];
}

void StartTrajectory() {
    currentTrajectory = new ArmTrajectory(false);
}

void AddTrajectoryPoint(glm::vec3 point, float a4, float a5, float ag) {
    currentTrajectory->points.push_back(ArmTrajectoryPoint(glm::vec3(point), a4, a5, ag));
}

void SaveTrajectory() {
    savedTrajectories.push_back(currentTrajectory);
    char textbuf[64];
    snprintf(textbuf, 64, "Траектория %d", savedTrajectories.size());
    GtkLabel* label = GTK_LABEL(gtk_label_new(textbuf));
    gtk_widget_show(GTK_WIDGET(label));
    gtk_list_box_insert(pointsListBox, GTK_WIDGET(label), -1);
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, savedTrajectories.size() - 1));
}

ArmTrajectoryPoint GetTrajectoryPoint(uint64_t timestamp) {
    int idx = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(pointsListBox));
    return savedTrajectories[idx]->points[timestamp];
}

bool IsTrajectoryPlaying(uint64_t timestamp) {
    int idx = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(pointsListBox));
    return savedTrajectories[idx]->points.size() >= timestamp;
}

void SelectPoint(int pointIdx) { // Выделить точку с индексом pointIdx
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, pointIdx));
}

void SelectPrevPoint() { // Выделить предыдущую точку
    printf("prev point\n");
    int idx = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(pointsListBox));
    idx--;
    if(idx < 0) idx = savedTrajectories.size() - 1;
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, idx));
}

void SelectNextPoint() { // Выделить следующую точку
    printf("next point\n");
    int idx = gtk_list_box_row_get_index(gtk_list_box_get_selected_row(pointsListBox));
    idx++;
    if(idx > savedTrajectories.size() - 1) idx = 0;
    gtk_list_box_select_row(pointsListBox, gtk_list_box_get_row_at_index(pointsListBox, idx));
}
#pragma endregion



void localVideoUpdateThreadProc() { // Самые цыганские фокусы
    uchar* pixels = gdk_pixbuf_get_pixels(localCamPixbuf); // Матрица с картинкой на экране
    int width = gdk_pixbuf_get_width(localCamPixbuf); // Ширина pixels
    int height = gdk_pixbuf_get_height(localCamPixbuf); // Высота pixels
    char trigger = 0; // Значение триггера PSMove
    int buttons = 0; // Значение кнопок PSMove
    int prevButtons = 0; // Значение buttons из предыдущего кадра
    int cnt = 0; // Счётчик кадров для обновления
    bool isTrajectoryPlaying = false;
    uint64_t currentFrame = 0;
    while(running) { // Это цикл, понятно?
        psmove_tracker_update_image(localCam); // Получить картинку с камеры
        psmove_tracker_update(localCam, NULL); // Отсленивание PSMove
        psmove_tracker_annotate(localCam); // Цыганские фокусы
        PSMoveTrackerRGBImage image = psmove_tracker_get_image(localCam); // Получить матрицу с картинкой с камерой

        cnt++;
        if(cnt >= 1 && selectedController != NULL) { // Это условный оператор, понятно?
            while(psmove_poll(selectedController)); // Получить данные с PSMove
            trigger = psmove_get_trigger(selectedController);
            psmove_fusion_get_position(localCamFusion, selectedController, &movePos.x, &movePos.y, &movePos.z);
            unsigned int buttons = psmove_get_buttons(selectedController);
            if(mode != 4) {
                if (buttons & Btn_MOVE && !(prevButtons & Btn_MOVE)) { // Установка системы отсчёта
                    mode++;
                    if(mode > 3) mode = 0;
                    if(mode == 1) {
                        moveCenter.x = movePos.x;
                        moveCenter.y = movePos.y;
                        moveCenter.z = movePos.z;
                    }
                }
                
                movePos.x -= moveCenter.x; // Преобразование из СО камеры в СО роборуки
                movePos.y -= moveCenter.y;
                movePos.z -= moveCenter.z;
                movePos.y *= 3;
                movePos.y = -movePos.y;

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

                angleGrabber = map_val(trigger, 0, 255, 110, 0);

                armPos.x = movePos.x;
                armPos.y = movePos.y;
                armPos.z = movePos.z;


                if(mode == 1 && buttons & Btn_CROSS && !(prevButtons & Btn_CROSS)) AddPoint(movePos, angle4, angle5, angleGrabber);

                if(mode == 3 && (buttons & Btn_CROSS) && !(prevButtons & Btn_CROSS)) StartTrajectory();
                if(mode == 3 && (buttons & Btn_CROSS)) AddTrajectoryPoint(movePos, angle4, angle5, angleGrabber);
                if(mode == 3 && !(buttons & Btn_CROSS) && (prevButtons & Btn_CROSS)) SaveTrajectory();

                if(mode == 2) isTrajectoryPlaying = false;
                if(mode == 3 && (buttons & Btn_CIRCLE) && !(prevButtons & Btn_CIRCLE)) { 
                    trajectoryStartFrame = currentFrame;
                    isTrajectoryPlaying = true;
                }

                if(isTrajectoryPlaying) {
                    ArmTrajectoryPoint pt = GetTrajectoryPoint(currentFrame - trajectoryStartFrame);
                    movePos.x = pt.pos.x;
                    movePos.y = pt.pos.y;
                    movePos.z = pt.pos.z;
                    angle4 = pt.angle4;
                    angle5 = pt.angle5;
                    angleGrabber = pt.angleGrabber;
                }
                if(isTrajectoryPlaying && !IsTrajectoryPlaying(currentFrame - trajectoryStartFrame)) isTrajectoryPlaying = false;

                if(savedTrajectories.size() > 0 && mode == 2) {
                    ArmTrajectoryPoint pt = GetSelectedPoint();
                    movePos.x = pt.pos.x;
                    movePos.y = pt.pos.y;
                    movePos.z = pt.pos.z;
                    angle4 = pt.angle4;
                    angle5 = pt.angle5;
                    angleGrabber = pt.angleGrabber;
                }


                printf("move control yes\n");
                currentFrame++;
            }
            else {
                angleGrabber = angleGrabberAuto;
                armPos.x = autoPos.x;
                armPos.y = autoPos.y;
                armPos.z = autoPos.z;
            }

            ComputeAngles();

            apprArmPos = Lerp(apprArmPos, armPos, map_val(velocityPercentage, 0, 100, 0, 0.15f));

            finArmPos.x = apprArmPos.x;
            finArmPos.y = apprArmPos.y;
            finArmPos.z = apprArmPos.z;
            
            finArmPos.x = (int)(finArmPos.x * 10) / 10.0;
            finArmPos.y = (int)(finArmPos.y * 10) / 10.0;
            finArmPos.z = (int)(finArmPos.z * 10) / 10.0;

            printf("X: %5.3f\tY: %5.3f\tZ: %5.3f\n",
                    finArmPos.x, finArmPos.y, finArmPos.z);
            
            if(mode != 0) writeMotors(angle1, angle2, angle4, angle5, angleGrabber, map_val(angle3, 0, 360, 0, 800));
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
    finArmPos *= 20;
    float y = clamp(finArmPos.y, -cylinderH / 2.0f, cylinderH / 2.0f);
    float a = 215;
    float b = 160;
    float c = clamp(sqrt(finArmPos.x * finArmPos.x + y * y + finArmPos.z * finArmPos.z), 60.0f, min(400.0f, (float)cylinderR));

    float alpha = acos((b * b + c * c - a * a) / (2 * b * c));
    float gamma = acos((a * a + b * b - c * c) / (2 * a * b));

    float angle = atan2(clamp(sqrt(finArmPos.x * finArmPos.x + finArmPos.z * finArmPos.z), 10.0f, 1000.0f), y);

    angle1 = 360 - alpha / DEG2RAD - angle / DEG2RAD + 90;
    angle2 = 180 - (alpha + gamma) / DEG2RAD - angle / DEG2RAD + 90;

    angle2 += 90;
    
    angle1 -= 180;

    angle3 = -atan2(-armPos.x, armPos.z) / DEG2RAD;

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
    velocityPercentageSpinBox = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "velocityPercentageSpinBox"));
    statusIndicatorLabel = GTK_LABEL(gtk_builder_get_object(builder, "statusIndicatorLabel"));
    cylinderRSpinBox = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "cylinderRSpinBox"));
    cylinderHSpinBox = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "cylinderHSpinBox"));
    setup_glarea(GTK_WIDGET(glArea));

    gtk_widget_show(GTK_WIDGET(window));

    voiceSocket = popen("/usr/bin/python3 speech.py", "r");
    printf("ass\n");

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

        velocityPercentage = gtk_spin_button_get_value_as_int(velocityPercentageSpinBox);
        cylinderR = gtk_spin_button_get_value_as_int(cylinderRSpinBox);
        cylinderH = gtk_spin_button_get_value_as_int(cylinderHSpinBox);

        switch(mode) {
            case 0: gtk_label_set_text(statusIndicatorLabel, "Выключен"); break;
            case 1: gtk_label_set_text(statusIndicatorLabel, "Копир. режим"); break;
            case 2: gtk_label_set_text(statusIndicatorLabel, "Точечн. режим"); break;
            case 3: gtk_label_set_text(statusIndicatorLabel, "Траект. режим"); break;
            case 4: gtk_label_set_text(statusIndicatorLabel, "Авто режим"); break;
        }

        while (gtk_events_pending()) gtk_main_iteration();

        usleep(10000);
    }

    return 0;
}