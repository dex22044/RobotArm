#include <iostream>
#include <SDL.h>
#include <cassert>
#include <Windows.h>
#include <queue>
#include <thread>
#include <psmove.h>
#include <psmove_tracker.h>
#include <psmove_fusion.h>

#include "PSMoveWindow.h"
#include "GUIBaseElements.h"

#include <winsock.h>
#pragma comment(lib, "ws2_32.lib")

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

using namespace std;

static float map_val(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define DEG2RAD 0.017453292;

queue<pair<int, char*>> RXQueue;
SOCKET leSocket;
SOCKET udpSocket;

AVFrame* rawFrame;
AVFrame* decFrame;
AVCodecContext* codecCtx;
SwsContext* sws_ctx;
SDL_Texture* rawCamTex;
bool isConnected = false;

TTF_Font* font;
GUILabel* ipLabel;
GUITextBox* ipTextBox;
GUIButton* connectBtn;
GUIImage* camImgBox;
sockaddr_in remoteAddr = { 0 };

int mouseX = 0, mouseY = 0;
bool mousePressed = false;
SDL_KeyboardEvent key = { 0 };

char* frameData;

void NetworkIOThread1Processor() {
	char* buf = new char[65536];

	while (true) {
		if (!isConnected) continue;
		int len = 0;
		recv(leSocket, (char*)&len, 4, 0);
		int received = 0;
		char* data = new char[len];
		while (received < len) {
			int rl = recv(leSocket, buf, min(65536, len - received), 0);
			if (rl > 0) {
				memcpy(data + received, buf, rl);
				received += rl;
			}
		}

		AVPacket pkt = { 0 };
		av_packet_from_data(&pkt, (uint8_t*)data, len);
		int isDecoded = 0;
		avcodec_decode_video2(codecCtx, decFrame, &isDecoded, &pkt);
		if (isDecoded) {
			sws_scale(sws_ctx, decFrame->data, decFrame->linesize, 0, decFrame->height, rawFrame->data, rawFrame->linesize);
			memcpy(frameData, rawFrame->data[0], 640 * 480 * 3);
			av_frame_unref(decFrame);
		}

		free(data);
	}
}

float pkt[5];

void UselessThreadProcessor() {
	while (true) {
		if (isConnected) {
			sendto(udpSocket, (char*)pkt, 20, 0, (sockaddr*)&remoteAddr, sizeof(sockaddr_in));
		}
		Sleep(20);
	}
}

PSMoveWindow* window2;

void SecondWindowThreadProcessor() {
	while (true) {
		window2->Draw();
	}
}

int main(int argc, char** argv)
{
#pragma region Some initialization shit
	psmove_init(PSMOVE_CURRENT_VERSION);

	WSAData wsdata;
	assert(WSAStartup(MAKEWORD(2, 2), &wsdata) == 0);
	avcodec_register_all();

	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();

	SDL_Window* mainWindow = SDL_CreateWindow("RobotClient", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 700, 600, SDL_WINDOW_RESIZABLE);
	SDL_Renderer* mainRenderer = SDL_CreateRenderer(mainWindow, -1, SDL_RENDERER_ACCELERATED);
#pragma endregion
#pragma region Video decoder initialization
	rawFrame = av_frame_alloc();
	rawFrame->width = 640;
	rawFrame->height = 480;
	rawFrame->format = AV_PIX_FMT_RGB24;
	uint64_t len = rawFrame->width * rawFrame->height;
	uint8_t* camFrameData = (uint8_t*)av_malloc(len * 3);
	avpicture_fill((AVPicture*)rawFrame, camFrameData, AV_PIX_FMT_RGB24, rawFrame->width, rawFrame->height);

	decFrame = av_frame_alloc();
	decFrame->width = rawFrame->width;
	decFrame->height = rawFrame->height;
	decFrame->format = AV_PIX_FMT_YUVJ420P;

	sws_ctx = sws_getContext(decFrame->width,
		decFrame->height,
		(AVPixelFormat)decFrame->format,
		rawFrame->width,
		rawFrame->height,
		(AVPixelFormat)rawFrame->format,
		SWS_FAST_BILINEAR,
		NULL, NULL, NULL);
	
	AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
	codecCtx = avcodec_alloc_context3(codec);
	avcodec_open2(codecCtx, codec, NULL);

	rawCamTex = SDL_CreateTexture(mainRenderer, SDL_PIXELFORMAT_BGR24, SDL_TEXTUREACCESS_STREAMING, rawFrame->width, rawFrame->height);
	SDL_Texture* rawCamTex2 = SDL_CreateTexture(mainRenderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 640, 480);
	frameData = (char*)malloc(640 * 480 * 3);
#pragma endregion
#pragma region GUI initialization
	font = TTF_OpenFont("C:/CascadiaCode.ttf", 24);
	assert(font != NULL);

	ipLabel = new GUILabel();
	ipLabel->font = font;
	ipLabel->rect.x = 50;
	ipLabel->rect.y = 50;
	ipLabel->rect.w = 150;
	ipLabel->rect.h = 30;
	ipLabel->fgColor = CreateColor(0, 0, 0, 255);
	ipLabel->borderThickness = 0;
	ipLabel->text = (char*)"IP address:";

	ipTextBox = new GUITextBox();
	ipTextBox->font = font;
	ipTextBox->rect.x = ipLabel->rect.x + ipLabel->rect.w + 10;
	ipTextBox->rect.y = 50;
	ipTextBox->rect.w = 200;
	ipTextBox->rect.h = 30;
	ipTextBox->bgColor = CreateColor(127, 127, 127, 255);
	ipTextBox->hoverbgColor = CreateColor(192, 192, 192, 255);
	ipTextBox->activebgColor = CreateColor(255, 255, 255, 255);
	ipTextBox->borderColor = CreateColor(63, 63, 63, 255);
	ipTextBox->fgColor = CreateColor(20, 20, 20, 255);
	ipTextBox->borderThickness = 5;
	ipTextBox->text = (char*)"";

	connectBtn = new GUIButton();
	connectBtn->font = font;
	connectBtn->rect.x = ipTextBox->rect.x + ipTextBox->rect.w + 10;
	connectBtn->rect.y = 50;
	connectBtn->rect.w = 250;
	connectBtn->rect.h = 30;
	connectBtn->bgColor = CreateColor(127, 127, 127, 255);
	connectBtn->hoverbgColor = CreateColor(192, 192, 192, 255);
	connectBtn->activebgColor = CreateColor(255, 255, 255, 255);
	connectBtn->borderColor = CreateColor(63, 63, 63, 255);
	connectBtn->fgColor = CreateColor(20, 20, 20, 255);
	connectBtn->borderThickness = 5;
	connectBtn->text = (char*)"Connect";

	camImgBox = new GUIImage();
	camImgBox->rect.x = 0;
	camImgBox->rect.y = 100;
	camImgBox->rect.w = 700;
	camImgBox->rect.h = 500;
	camImgBox->bgColor = CreateColor(127, 127, 127, 255);
	camImgBox->hoverbgColor = CreateColor(192, 192, 192, 255);
	camImgBox->activebgColor = CreateColor(255, 255, 255, 255);
	camImgBox->borderColor = CreateColor(63, 63, 63, 255);
	camImgBox->borderThickness = 5;
	camImgBox->image = rawCamTex;
#pragma endregion
#pragma region Second window
	window2 = new PSMoveWindow();
#pragma endregion
#pragma region UDP initialization
	sockaddr_in saListen;
	saListen.sin_family = AF_INET;
	saListen.sin_addr.s_addr = INADDR_ANY;
	saListen.sin_port = htons(8023);
	udpSocket = socket(PF_INET, SOCK_DGRAM, 0);
	assert(bind(udpSocket, (sockaddr*)&saListen, sizeof(sockaddr_in)) != -1);
#pragma endregion
#pragma region Threads
	thread networkIOThread1(NetworkIOThread1Processor);
	thread uselessThread(UselessThreadProcessor);
	thread secondWindowThread(SecondWindowThreadProcessor);
#pragma endregion

	while (true) {
		SDL_Event ev;
		key.state = SDL_RELEASED;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) {
				exit(0);
			}
			if (ev.type == SDL_MOUSEMOTION) {
				mouseX = ev.motion.x;
				mouseY = ev.motion.y;
			}
			if (ev.type == SDL_MOUSEBUTTONDOWN) mousePressed = true;
			if (ev.type == SDL_MOUSEBUTTONUP) mousePressed = false;
			if (ev.type == SDL_KEYDOWN) key = ev.key;
		}

		SDL_SetRenderDrawColor(mainRenderer, 255, 255, 255, 255);
		SDL_RenderClear(mainRenderer);
		SDL_UpdateTexture(rawCamTex, NULL, frameData, 640 * 3);

		ipLabel->update(mouseX, mouseY, mousePressed, key);
		ipTextBox->update(mouseX, mouseY, mousePressed, key);
		connectBtn->update(mouseX, mouseY, mousePressed, key);
		camImgBox->update(mouseX, mouseY, mousePressed, key);

		ipLabel->draw(mainRenderer, 0);
		ipTextBox->draw(mainRenderer, 0);
		connectBtn->draw(mainRenderer, 0);
		camImgBox->draw(mainRenderer, 0);

		if (connectBtn->isPressedDelta) {
			if (!isConnected) {
#pragma region TCP Socket connect
				leSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				sockaddr_in saConn;
				saConn.sin_family = AF_INET;
				saConn.sin_addr.S_un.S_addr = inet_addr(ipTextBox->text.c_str());
				remoteAddr.sin_family = AF_INET;
				remoteAddr.sin_addr.S_un.S_addr = inet_addr(ipTextBox->text.c_str());
				if (saConn.sin_addr.S_un.S_addr == -1) {
					connectBtn->bgColor = CreateColor(127, 0, 0, 255);
					connectBtn->hoverbgColor = CreateColor(192, 0, 0, 255);
					connectBtn->activebgColor = CreateColor(255, 0, 0, 255);
					connectBtn->text = (char*)"IP error";
					continue;
				}
				saConn.sin_port = htons(8022);
				remoteAddr.sin_port = htons(8023);
				if (connect(leSocket, (sockaddr*)&saConn, sizeof(sockaddr_in)) == SOCKET_ERROR) {
					connectBtn->bgColor = CreateColor(127, 0, 0, 255);
					connectBtn->hoverbgColor = CreateColor(192, 0, 0, 255);
					connectBtn->activebgColor = CreateColor(255, 0, 0, 255);
					connectBtn->text = (char*)"Connection error";
					continue;
				}
				connectBtn->bgColor = CreateColor(0, 127, 0, 255);
				connectBtn->hoverbgColor = CreateColor(0, 192, 0, 255);
				connectBtn->activebgColor = CreateColor(0, 255, 0, 255);
				connectBtn->text = (char*)"Disconnect";
				isConnected = true;
#pragma endregion
			}
			else {
#pragma region TCP Socket disconnect
				isConnected = false;
				closesocket(leSocket);
				connectBtn->bgColor = CreateColor(127, 127, 127, 255);
				connectBtn->hoverbgColor = CreateColor(192, 192, 192, 255);
				connectBtn->activebgColor = CreateColor(255, 255, 255, 255);
				connectBtn->text = (char*)"Connect";
#pragma endregion
			}
		}

		if (window2->selectedController != NULL) {
			psmove_poll(window2->selectedController);
			float x, y, z, trigger;
			unsigned int buttons = psmove_get_buttons(window2->selectedController);
			psmove_fusion_get_position(window2->fusion, window2->selectedController, &x, &y, &z);
			trigger = psmove_get_trigger(window2->selectedController) / 255.0;
			pkt[0] = x;
			pkt[1] = y;
			pkt[2] = z;
			pkt[3] = trigger;
			if (buttons & Btn_SQUARE) pkt[4] = 1;
			else if (buttons & Btn_TRIANGLE) pkt[4] = -1;
			else pkt[4] = 0;
			printf("Position: X: %10.2f Y: %10.2f Z: %10.2f; trigger: %2.2f; motor: %1.1f\n", x, y, z, trigger, pkt[4]);
		}

		SDL_RenderPresent(mainRenderer);

		SDL_Delay(10);
	}
}