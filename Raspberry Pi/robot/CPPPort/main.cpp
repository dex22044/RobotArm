#include <iostream>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <fstream>
#include <chrono>
#include <cmath>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>
#include <cassert>
#include <thread>
#include <queue>
#include <wiringPi.h>
#include <wiringSerial.h>

#include "StepperMotor.h"
#include "Servo.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/opt.h>
    #include <libswscale/swscale.h>
}

using namespace std;
using namespace cv;

void ErrorMessageBox(const char* msg) {
    printf("Critical error: %s\n", msg);
}

static float map_val(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int remoteSockfd;
int udpSockfd;
vector<StepperMotor*> connectedMotors;
Servo* servo1;
Servo* servo2;

typedef struct ServoData {
  int64_t angle1;
  int64_t angle2;
  int64_t angle3;
  int64_t angle4;
} ServoData;

queue<pair<int, char*>> TXQueue;
queue<pair<int, char*>> RXQueue;
AVCodecContext* codecCtx = NULL;

bool isConnected = false;

void MotorsProcessingThreadProcessor() {
    while(true) {
        for(StepperMotor* motor : connectedMotors) {
            motor->Step();
        }
    }
}

int perduinoSerial = -1;
int step = 0;

void NetworkIOThread2Processor() {
    ServoData* sdata = new ServoData();
    float pkt[8];
    char* cmd = new char[256];
	while (true) {
        if(!isConnected) continue;
		int len = recv(udpSockfd, (char*)pkt, 32, 0);
        if(len != 32) continue;
        
        step++;
        if(step == 10) {
            sdata->angle1 = min(180, max(0, (int)map_val(pkt[3], 0, 1, 90, 20)));
            sdata->angle2 = min(180, max(0, (int)map_val(pkt[7], 1.57, -1.57, 0, 180)));
            int len = sprintf(cmd, "%d %d %d \n", min(180, max(0, (int)map_val(pkt[3], 0, 1, 90, 20))),
                            min(180, max(0, (int)map_val(pkt[7], 1.57, -1.57, 0, 180))),
                            -(int)(atan2(-pkt[0], -pkt[2]) / 3.14159 / 2.0 * 6500.0));
            float c = (int)(atan2(-pkt[0], -pkt[2]) / 3.14159 / 2.0 * 6500.0);
            //int len = sprintf(cmd, "90 90 %d \n",
            //                    -(int)c);
            printf("%s\n", cmd);
            write(perduinoSerial, cmd, len);
            step = 0;
        }

        if(pkt[4] == 0) {
            servo1->Write(90);
            servo2->Write(50);
        }
        if(pkt[4] == 1) {
            servo1->Write((int)pkt[5]);
            servo2->Write((int)pkt[6]);
            //printf("Servo 1: %d 2: %d\n", (int)pkt[5], (int)pkt[6]);
        }
	}
}

void NetworkIOThread1Processor() {
    while(true) {
        if(!isConnected) continue;
        while(TXQueue.size() == 0) { usleep(5000); }
        while(!TXQueue.empty()) {
            pair<int, char*> pkt = TXQueue.front();
            TXQueue.pop();
            if(!isConnected) continue;
            if(pkt.second != NULL and pkt.first > 0) {
                send(remoteSockfd, (char*)&pkt.first, 4, 0);
                send(remoteSockfd, pkt.second, pkt.first, 0);
                free(pkt.second);
            }
            usleep(3000);
        }
    }
}

int main() {
    VideoCapture cam(0);
    if(!cam.isOpened()) {
        cout << "Camera fail\n";
        exit(-1);
    }
    
    wiringPiSetupGpio();

    perduinoSerial = serialOpen("/dev/ttyS0", 115200);
    
    servo1 = new Servo(18);
    servo2 = new Servo(13);
    
    pwmSetMode(PWM_MODE_MS);
    pwmSetClock(192);
    pwmSetRange(2000);
    
    servo1->Write(90);
    servo2->Write(50);

    pinMode(16, OUTPUT);
    pinMode(26, OUTPUT);

    digitalWrite(16, 0);
    digitalWrite(26, 0);

	AVFrame* camFrame = av_frame_alloc();
	camFrame->width = cam.get(CAP_PROP_FRAME_WIDTH);
	camFrame->height = cam.get(CAP_PROP_FRAME_HEIGHT);
	camFrame->format = AV_PIX_FMT_RGB24;
	uint64_t len = camFrame->width * camFrame->height;
	uint8_t* camFrameData = (uint8_t*)av_malloc(len * 3);
	avpicture_fill((AVPicture*)camFrame, camFrameData, AV_PIX_FMT_BGR24, camFrame->width, camFrame->height);

	AVFrame* encFrame = av_frame_alloc();
	encFrame->width = camFrame->width;
	encFrame->height = camFrame->height;
	encFrame->format = AV_PIX_FMT_YUVJ420P;
	uint64_t len2 = avpicture_get_size(AV_PIX_FMT_YUVJ420P, encFrame->width, encFrame->height);
	uint8_t* encFrameData = (uint8_t*)av_malloc(len2);
	avpicture_fill((AVPicture*)encFrame, encFrameData, AV_PIX_FMT_YUVJ420P, encFrame->width, encFrame->height);

    SwsContext* sws_ctx = sws_getContext(camFrame->width,
                                        camFrame->height,
                                        (AVPixelFormat)camFrame->format,
                                        encFrame->width,
                                        encFrame->height,
                                        (AVPixelFormat)encFrame->format,
                                        SWS_BICUBIC,
                                        NULL, NULL, NULL);

    std::thread networkIOThread2(NetworkIOThread2Processor);
    std::thread MotorsProcessingThread(MotorsProcessingThreadProcessor);

    Mat currFrame;
    int pts = 0;

    int currAngle = 0;
    
    auto lastTime = std::chrono::high_resolution_clock::now();

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    sockaddr_in saListen;
    saListen.sin_family = AF_INET;
    saListen.sin_addr.s_addr = INADDR_ANY;
    saListen.sin_port = htons(8022);
    assert(bind(sockfd, (sockaddr*)&saListen, sizeof(sockaddr_in)) != -1);
    assert(listen(sockfd, 1) == 0);

    udpSockfd = socket(PF_INET, SOCK_DGRAM, 0);
    saListen.sin_port = htons(8023);
    assert(bind(udpSockfd, (sockaddr*)&saListen, sizeof(sockaddr_in)) != -1);

    sockaddr saRemote = { 0 };
    socklen_t saRemoteLen = 0;

    while(true) {
        if(!isConnected) {
            if(codecCtx != NULL) avcodec_free_context(&codecCtx);
            AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
            codecCtx = avcodec_alloc_context3(codec);

            codecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
            codecCtx->bit_rate = 0.4 * 1024 * 1024;
            codecCtx->time_base = { 1, 30 };
            codecCtx->width = encFrame->width;
            codecCtx->height = encFrame->height;
            codecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
            codecCtx->gop_size = 10;
            codecCtx->level = 31;
            codecCtx->flags |= AV_CODEC_FLAG_QSCALE;
            codecCtx->global_quality = FF_QP2LAMBDA * 275.0;
            //if (av_opt_set(codecCtx->priv_data, "profile", "baseline", 0) < 0) { ErrorMessageBox("Failed to set X264 profile."); exit(-1); };
            //if (av_opt_set(codecCtx->priv_data, "preset", "ultrafast", 0) < 0) { ErrorMessageBox("Failed to set X264 preset."); exit(-1); };
            //if (av_opt_set(codecCtx->priv_data, "tune", "zerolatency", 0) < 0) { ErrorMessageBox("Failed to set X264 tune."); exit(-1); };
            if (avcodec_open2(codecCtx, codec, NULL) < 0) { ErrorMessageBox("Failed to open X264 stream."); exit(-1); };

            remoteSockfd = accept(sockfd, &saRemote, &saRemoteLen);
            ((sockaddr_in*)&saRemote)->sin_port = htons(8023);

            isConnected = true;
        }

        cam >> currFrame;
        memcpy(camFrameData, currFrame.data, len * 3);
        sws_scale(sws_ctx, camFrame->data, camFrame->linesize, 0, camFrame->height, encFrame->data, encFrame->linesize);

        {
            AVPacket pkt = { 0 };
            av_init_packet(&pkt);
            pkt.pts = pts++;
            encFrame->quality = codecCtx->global_quality;
            encFrame->pts = pkt.pts;
            int isEncoded = 0;
            avcodec_encode_video2(codecCtx, &pkt, encFrame, &isEncoded);
            if(isEncoded) {
                //pair<int, char*> pkt2 = make_pair(0, (char*)NULL);
                //pkt2.second = (char*)malloc(pkt.size);
                //memcpy(pkt2.second, pkt.data, pkt.size);
                //pkt2.first = pkt.size;
                //while(TXQueue.size() >= 32){}
                //TXQueue.push(pkt2);
                send(remoteSockfd, (char*)&pkt.size, 4, 0);
                send(remoteSockfd, pkt.data, pkt.size, 0);
                //sendto(udpSockfd, pkt.data, pkt.size, 0, &saRemote, saRemoteLen);
                //auto duration = std::chrono::high_resolution_clock::now() - lastTime;
                //long long millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
                //printf("Image size: %d bytes; frametime: %dms\n", pkt.size, millis);
                //lastTime = std::chrono::high_resolution_clock::now();
            }
            av_free_packet(&pkt);
        }
    }

    cam.release();
}
