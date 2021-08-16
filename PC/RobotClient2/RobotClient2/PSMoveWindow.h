#pragma once

#include <iostream>
#include <SDL.h>
#include <SDL_ttf.h>
#include <psmove.h>
#include <psmove_tracker.h>
#include <psmove_fusion.h>
#include <cstring>
#include <string>
#include <vector>
#include <Windows.h>

using namespace std;

class PSMoveWindow
{
public:
	SDL_Window* window;
	SDL_Renderer* renderer;

	PSMoveTracker* tracker;
	PSMove* selectedController;
	PSMoveFusion* fusion;

	TTF_Font* cascadiaCodeFont;

	vector<pair<PSMove*, PSMove_Connection_Type>> moves;

	SDL_Texture* camTex;

	int mouseX, mouseY;
	bool mousePressed;
	bool prevRefreshBtnState;

	PSMoveWindow();
	void Update();
	void Draw();
};

