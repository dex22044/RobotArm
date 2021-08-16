#include "PSMoveWindow.h"

PSMoveWindow::PSMoveWindow()
{
	window = SDL_CreateWindow("PSMove manager", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 550, 0);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

	cascadiaCodeFont = TTF_OpenFont("C:/CascadiaCode.ttf", 12);

	PSMoveTrackerSettings settings = { 0 };
	psmove_tracker_settings_set_default(&settings);
	settings.color_mapping_max_age = 0;
	settings.exposure_mode = Exposure_LOW;
	tracker = psmove_tracker_new_with_settings(&settings);
	fusion = psmove_fusion_new(tracker, 1, 1000);

	camTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 640, 480);
}

void PSMoveWindow::Update()
{
	if (selectedController != NULL) psmove_tracker_disable(tracker, selectedController);
	selectedController = NULL;
	moves.clear();
	int controllerCount = psmove_count_connected();
	bool foundSelected = false;
	moves.reserve(controllerCount);
	for (int i = 0; i < controllerCount; i++) {
		PSMove* move = psmove_connect_by_id(i);
		if (move == NULL) continue;
		moves.push_back(make_pair(move, psmove_connection_type(move)));
	}
}

void PSMoveWindow::Draw()
{
	SDL_SetRenderDrawColor(renderer, 127, 127, 127, 255);
	SDL_RenderClear(renderer);

	{
		mousePressed = SDL_GetMouseState(&mouseX, &mouseY);
		mousePressed = mousePressed && SDL_GetMouseFocus() == window;
		if (mouseX >= 0 && mouseX <= 50 && mouseY >= 0 && mouseY <= 15) {
			if (!mousePressed) {
				SDL_SetRenderDrawColor(renderer, 114, 114, 114, 255);
				prevRefreshBtnState = false;
			}
			else {
				SDL_SetRenderDrawColor(renderer, 56, 56, 56, 255);
				if(!prevRefreshBtnState) Update();
				prevRefreshBtnState = true;
			}
		}
		else {
			SDL_SetRenderDrawColor(renderer, 196, 196, 196, 255);
		}

		SDL_Rect r3 = { 0 };
		r3.x = 0;
		r3.y = 0;
		r3.w = 50;
		r3.h = 15;

		SDL_RenderFillRect(renderer, &r3);

		SDL_Color black = { 0 };
		black.a = 255;

		SDL_Rect r2 = { 0 };
		SDL_Surface* text1 = TTF_RenderText_Blended(cascadiaCodeFont, "Refresh", black);
		SDL_Texture* text1Tex = SDL_CreateTextureFromSurface(renderer, text1);
		SDL_QueryTexture(text1Tex, NULL, NULL, &r2.w, &r2.h);
		r2.x = 0;
		r2.y = 0;
		SDL_RenderCopy(renderer, text1Tex, NULL, &r2);
		SDL_DestroyTexture(text1Tex);
		SDL_FreeSurface(text1);
	}

	for (int i = 0; i < moves.size(); i++) {
		pair<PSMove*, PSMove_Connection_Type> m = moves[i];
		if(m.first != selectedController) SDL_SetRenderDrawColor(renderer, 222, 225, 83, 255);
		else SDL_SetRenderDrawColor(renderer, 255, 127, 127, 255);
		SDL_Rect r = { 0 };
		r.x = 100 * i;
		r.y = 10;
		r.w = 100;
		r.h = 60;
		SDL_RenderFillRect(renderer, &r);

		SDL_Color black = { 0 };
		black.a = 255;

		char* text = (char*)"";
		switch (m.second) {
		case Conn_USB: text = (char*)"USB"; break;
		case Conn_Bluetooth: text = (char*)"BT"; break;
		case Conn_Unknown: text = (char*)"Unknown"; break;
		}

		SDL_Rect r2 = { 0 };

		SDL_Surface* text1 = TTF_RenderText_Blended(cascadiaCodeFont, (string("Move ") + to_string(i + 1)).c_str(), black);
		SDL_Texture* text1Tex = SDL_CreateTextureFromSurface(renderer, text1);
		SDL_QueryTexture(text1Tex, NULL, NULL, &r2.w, &r2.h);
		r2.x = r.x + 5;
		r2.y = r.y + 5;
		SDL_RenderCopy(renderer, text1Tex, NULL, &r2);
		SDL_DestroyTexture(text1Tex);
		SDL_FreeSurface(text1);

		SDL_Surface* text2 = TTF_RenderText_Blended(cascadiaCodeFont, text, black);
		SDL_Texture* text2Tex = SDL_CreateTextureFromSurface(renderer, text2);
		SDL_QueryTexture(text2Tex, NULL, NULL, &r2.w, &r2.h);
		r2.x = r.x + 5;
		r2.y = r.y + 20;
		SDL_RenderCopy(renderer, text2Tex, NULL, &r2);
		SDL_DestroyTexture(text2Tex);
		SDL_FreeSurface(text2);

		if (m.second != Conn_Bluetooth) {
			if (mouseX >= r.x + 50 && mouseX <= r.x + 50 + 45 && mouseY >= r.y + 5 && mouseY <= r.y + 5 + 15) {
				if (!mousePressed) {
					SDL_SetRenderDrawColor(renderer, 114, 114, 114, 255);
				}
				else {
					SDL_SetRenderDrawColor(renderer, 56, 56, 56, 255);

					if (psmove_connection_type(m.first) == Conn_USB) {
						SDL_Window* pairingWindow = SDL_CreateWindow("PSMove manager", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 100, 100, SDL_WINDOW_SHOWN);
						SDL_Renderer* pairingWindowRenderer = SDL_CreateRenderer(pairingWindow, -1, SDL_RENDERER_SOFTWARE);

						SDL_SetRenderDrawColor(pairingWindowRenderer, 255, 255, 255, 255);
						SDL_RenderClear(pairingWindowRenderer);

						SDL_Surface* text3 = TTF_RenderText_Blended(cascadiaCodeFont, "Pairing...", black);
						SDL_Texture* text3Tex = SDL_CreateTextureFromSurface(pairingWindowRenderer, text3);
						SDL_QueryTexture(text3Tex, NULL, NULL, &r2.w, &r2.h);
						r2.x = 10;
						r2.y = 10;
						SDL_RenderCopy(pairingWindowRenderer, text3Tex, NULL, &r2);
						SDL_DestroyTexture(text3Tex);
						SDL_FreeSurface(text3);

						SDL_RenderPresent(pairingWindowRenderer);

						if (psmove_pair(m.first) == PSMove_True) {
							SDL_RenderClear(pairingWindowRenderer);
							SDL_Surface* text3 = TTF_RenderText_Blended(cascadiaCodeFont, "Pairing successful", black);
							SDL_Texture* text3Tex = SDL_CreateTextureFromSurface(pairingWindowRenderer, text3);
							SDL_QueryTexture(text3Tex, NULL, NULL, &r2.w, &r2.h);
							r2.x = 10;
							r2.y = 10;
							SDL_RenderCopy(pairingWindowRenderer, text3Tex, NULL, &r2);
							SDL_DestroyTexture(text3Tex);
							SDL_FreeSurface(text3);
						}
						else {
							SDL_RenderClear(pairingWindowRenderer);
							SDL_Surface* text3 = TTF_RenderText_Blended(cascadiaCodeFont, "Pairing error", black);
							SDL_Texture* text3Tex = SDL_CreateTextureFromSurface(pairingWindowRenderer, text3);
							SDL_QueryTexture(text3Tex, NULL, NULL, &r2.w, &r2.h);
							r2.x = 10;
							r2.y = 10;
							SDL_RenderCopy(pairingWindowRenderer, text3Tex, NULL, &r2);
							SDL_DestroyTexture(text3Tex);
							SDL_FreeSurface(text3);
						}

						SDL_RenderPresent(pairingWindowRenderer);

						Sleep(3000);

						SDL_DestroyRenderer(pairingWindowRenderer);
						SDL_DestroyWindow(pairingWindow);
					}
				}
			}
			else {
				SDL_SetRenderDrawColor(renderer, 196, 196, 196, 255);
			}

			SDL_Rect r3 = { 0 };
			r3.x = r.x + 48;
			r3.y = r.y + 5;
			r3.w = 45;
			r3.h = 15;

			SDL_RenderFillRect(renderer, &r3);

			SDL_Surface* text3 = TTF_RenderText_Blended(cascadiaCodeFont, "Pair", black);
			SDL_Texture* text3Tex = SDL_CreateTextureFromSurface(renderer, text3);
			SDL_QueryTexture(text3Tex, NULL, NULL, &r2.w, &r2.h);
			r2.x = r.x + 60;
			r2.y = r.y + 5;
			SDL_RenderCopy(renderer, text3Tex, NULL, &r2);
			SDL_DestroyTexture(text3Tex);
			SDL_FreeSurface(text3);
		}

		if (m.second == Conn_Bluetooth) {
			if (mouseX >= r.x + 50 && mouseX <= r.x + 50 + 45 && mouseY >= r.y + 20 && mouseY <= r.y + 20 + 15) {
				if (!mousePressed) {
					SDL_SetRenderDrawColor(renderer, 114, 114, 114, 255);
				}
				else {
					SDL_SetRenderDrawColor(renderer, 56, 56, 56, 255);
					if(selectedController != NULL) psmove_tracker_disable(tracker, selectedController);
					selectedController = m.first;
					while (psmove_tracker_enable(tracker, m.first) != Tracker_CALIBRATED) continue;
				}
			}
			else {
				SDL_SetRenderDrawColor(renderer, 196, 196, 196, 255);
			}

			SDL_Rect r3 = { 0 };
			r3.x = r.x + 50;
			r3.y = r.y + 20;
			r3.w = 45;
			r3.h = 15;

			SDL_RenderFillRect(renderer, &r3);

			SDL_Surface* text4 = TTF_RenderText_Blended(cascadiaCodeFont, "Select", black);
			SDL_Texture* text4Tex = SDL_CreateTextureFromSurface(renderer, text4);
			SDL_QueryTexture(text4Tex, NULL, NULL, &r2.w, &r2.h);
			r2.x = r.x + 52;
			r2.y = r.y + 20;
			SDL_RenderCopy(renderer, text4Tex, NULL, &r2);
			SDL_DestroyTexture(text4Tex);
			SDL_FreeSurface(text4);
		}
	}

	psmove_tracker_update_image(tracker);
	psmove_tracker_update(tracker, NULL);
	psmove_tracker_annotate(tracker);
	PSMoveTrackerRGBImage img = psmove_tracker_get_image(tracker);
	SDL_UpdateTexture(camTex, NULL, img.data, img.width * 3);

	SDL_Rect r = { 0 };
	r.x = 0;
	r.y = 70;
	r.w = 640;
	r.h = 480;

	SDL_RenderCopy(renderer, camTex, NULL, &r);

	SDL_RenderPresent(renderer);
}