#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>

SDL_Color CreateColor(int r, int g, int b, int a) {
	SDL_Color c;
	c.r = r; c.g = g; c.b = b; c.a = a;
	return c;
}

SDL_Rect GetTextSize(char* message, TTF_Font* font, SDL_Color color) {
	SDL_Surface* sText = TTF_RenderUTF8_Solid(font, message, color);
	if (sText != nullptr) {
		SDL_Rect destcpy = { 0 };
		destcpy.x = 0;
		destcpy.y = 0;
		destcpy.w = sText->w;
		destcpy.h = sText->h;
		SDL_FreeSurface(sText);
		return destcpy;
	}
	else {
		return { 0 };
	}
}

void RenderText(SDL_Renderer* renderer, char* message, TTF_Font* font, SDL_Color color, SDL_Rect* dest) {
	SDL_Surface* sText = TTF_RenderUTF8_Blended(font, message, color);
	if (sText == nullptr) return;
	SDL_Texture* tText = SDL_CreateTextureFromSurface(renderer, sText);
	if (dest != NULL) {
		SDL_Rect* destcpy = (SDL_Rect*)malloc(16);
		memcpy(destcpy, dest, 16);
		SDL_QueryTexture(tText, NULL, NULL, &destcpy->w, &destcpy->h);
		SDL_RenderCopy(renderer, tText, NULL, destcpy);
		free(destcpy);
	}
	else {
		SDL_RenderCopy(renderer, tText, NULL, NULL);
	}
	SDL_DestroyTexture(tText);
	SDL_FreeSurface(sText);
}

class GUIElementBase {
public:
	SDL_Rect rect;
	SDL_Color bgColor;
	SDL_Color hoverbgColor;
	SDL_Color activebgColor;
	SDL_Color borderColor;
	int borderThickness;
	bool show = true;

	virtual void update(int mouseX, int mouseY, bool mousePressed, SDL_KeyboardEvent key) {

	}

	virtual void draw(SDL_Renderer* renderer, int state) {
		if (!show) return;
		switch (state) {
		case 0: SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a); break;
		case 1: SDL_SetRenderDrawColor(renderer, hoverbgColor.r, hoverbgColor.g, hoverbgColor.b, hoverbgColor.a); break;
		case 2: SDL_SetRenderDrawColor(renderer, activebgColor.r, activebgColor.g, activebgColor.b, activebgColor.a); break;
		}
		SDL_RenderFillRect(renderer, &rect);
		SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
		if (borderThickness > 0) {
			SDL_Rect r1 = { 0 };
			r1.x = rect.x - borderThickness / 2;
			r1.y = rect.y - borderThickness / 2;
			r1.w = borderThickness / 2 * 2;
			r1.h = rect.h + borderThickness / 2 * 2;
			SDL_RenderFillRect(renderer, &r1);
			r1.w = rect.w + borderThickness / 2 * 2;
			r1.h = borderThickness / 2 * 2;
			SDL_RenderFillRect(renderer, &r1);
			r1.x = rect.x - borderThickness / 2;
			r1.y = rect.y + rect.h - borderThickness / 2;
			SDL_RenderFillRect(renderer, &r1);
			r1.x = rect.x + rect.w - borderThickness / 2;
			r1.y = rect.y - borderThickness / 2;
			r1.w = borderThickness / 2 * 2;
			r1.h = rect.h + borderThickness / 2 * 2;
			SDL_RenderFillRect(renderer, &r1);
		}
	}
};

class GUIButton : public GUIElementBase {
private:
	bool isPressedPrev;
	int mouseX; int mouseY; bool mousePressed;

public:
	char* text;
	TTF_Font* font;
	SDL_Color fgColor;
	bool isPressed;
	bool isPressedDelta;

	void update(int mouseX, int mouseY, bool mousePressed, SDL_KeyboardEvent key) {
		this->mouseX = mouseX;
		this->mouseY = mouseY;
		this->mousePressed = mousePressed;
	}

	void draw(SDL_Renderer* renderer, int state) {
		if (!show) return;
		if (mouseX > rect.x && mouseX < rect.x + rect.w && mouseY > rect.y && mouseY < rect.y + rect.h) {
			isPressed = mousePressed;
			isPressedDelta = mousePressed && !isPressedPrev;
			isPressedPrev = isPressed;
			if (mousePressed) GUIElementBase::draw(renderer, 2);
			else GUIElementBase::draw(renderer, 1);
		}
		else GUIElementBase::draw(renderer, 0);
		SDL_Rect rTextRect = GetTextSize(text, font, fgColor);
		SDL_Rect textRect = { 0 };
		textRect.x = rect.x + rect.w / 2 - rTextRect.w / 2;
		textRect.y = rect.y + rect.h / 2 - rTextRect.h / 2;
		RenderText(renderer, text, font, fgColor, &textRect);
	}
};

class GUILabel : public GUIElementBase {
private:
	bool isPressedPrev;

public:
	char* text;
	TTF_Font* font;
	SDL_Color fgColor;

	void draw(SDL_Renderer* renderer, int state) {
		if (!show) return;
		SDL_Rect rTextRect = GetTextSize(text, font, fgColor);
		SDL_Rect textRect = { 0 };
		textRect.x = rect.x + rect.w / 2 - rTextRect.w / 2;
		textRect.y = rect.y + rect.h / 2 - rTextRect.h / 2;
		RenderText(renderer, text, font, fgColor, &textRect);
	}
};

class GUITextBox : public GUIElementBase {
private:
	int frame;
	int mouseX; int mouseY; bool mousePressed; SDL_KeyboardEvent key;

public:
	std::string text;
	TTF_Font* font;
	SDL_Color fgColor;
	bool isActive;

	void update(int mouseX, int mouseY, bool mousePressed, SDL_KeyboardEvent key) {
		this->mouseX = mouseX;
		this->mouseY = mouseY;
		this->mousePressed = mousePressed;
		this->key = key;
	}

	void draw(SDL_Renderer* renderer, int state) {
		if (!show) {
			isActive = false;
			return;
		}
		if (mouseX > rect.x && mouseX < rect.x + rect.w && mouseY > rect.y && mouseY < rect.y + rect.h) {
			if (mousePressed) isActive = true;
			if (isActive) GUIElementBase::draw(renderer, 1);
			else GUIElementBase::draw(renderer, 0);
		}
		else {
			if (mousePressed) isActive = false;
			if (isActive) GUIElementBase::draw(renderer, 1);
			else GUIElementBase::draw(renderer, 0);
		}

		if (isActive && key.state == SDL_PRESSED) {
			if (key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_BACKSPACE) {
				if(text.size() > 0) text.pop_back();
			}
			else if(key.keysym.sym >= 'a' && key.keysym.sym <= 'z') {
				if(key.keysym.mod & KMOD_SHIFT) text += (char)(key.keysym.sym - 'a' + 'A');
				else text += (char)(key.keysym.sym);
			}
			else if (key.keysym.sym >= '0' && key.keysym.sym <= '9') {
				text += (char)(key.keysym.sym);
			}
			else if (key.keysym.sym == '.') text += '.';
		}

		if (isActive) frame += 1;
		else frame = 0;
		frame = frame % 100;
		SDL_Rect rTextRect;
		if(frame < 50 && isActive) rTextRect = GetTextSize((char*)(text + "|").c_str(), font, fgColor);
		else rTextRect = GetTextSize((char*)text.c_str(), font, fgColor);
		SDL_Rect textRect = { 0 };
		textRect.x = rect.x + borderThickness / 2 + 5;
		textRect.y = rect.y + rect.h / 2 - rTextRect.h / 2;
		if (frame < 50 && isActive) RenderText(renderer, (char*)(text + "|").c_str(), font, fgColor, &textRect);
		else RenderText(renderer, (char*)text.c_str(), font, fgColor, &textRect);
	}
};

class GUIImage : public GUIElementBase {
private:
	SDL_Rect computedImageRect = { 0 };

public:
	SDL_Texture* image;

	void draw(SDL_Renderer* renderer, int state) {
		if (!show) return;
		GUIElementBase::draw(renderer, 0);
		int width, height;
		SDL_QueryTexture(image, NULL, NULL, &width, &height);
		double rwidth = rect.w * 1.0 / width;
		double rheight = rect.h * 1.0 / height;
		if (rwidth < rheight) {
			computedImageRect.w = width * rwidth;
			computedImageRect.h = height * rwidth;
		}
		else {
			computedImageRect.w = width * rheight;
			computedImageRect.h = height * rheight;
		}
		computedImageRect.x = rect.x + rect.w / 2 - computedImageRect.w / 2;
		computedImageRect.y = rect.y + rect.h / 2 - computedImageRect.h / 2;
		SDL_RenderCopy(renderer, image, NULL, &computedImageRect);
	}
};

class GUIContainer : public GUIElementBase {
private:
	std::vector<GUIElementBase*> elements;

public:
	void addElement(GUIElementBase* elem) {
		elements.push_back(elem);
	}

	void update(int mouseX, int mouseY, bool mousePressed, SDL_KeyboardEvent key) {
		for (GUIElementBase* elem : elements) {
			elem->update(mouseX, mouseY, mousePressed, key);
		}
	}

	void draw(SDL_Renderer* renderer, int state) {
		if (!show) return;
		GUIElementBase::draw(renderer, 0);

		for (GUIElementBase* elem : elements) {
			elem->draw(renderer, 0);
		}
	}
};

class GUITabBox : public GUIElementBase {
private:
	std::vector<GUIContainer*> tabs;
	std::vector<GUIButton*> buttons;

public:
	int buttonsHeight;
	int currentSelectedIndex;

	void addTab(GUIContainer* tab, GUIButton* button) {
		tabs.push_back(tab);
		buttons.push_back(button);
	}

	void update(int mouseX, int mouseY, bool mousePressed, SDL_KeyboardEvent key) {
		for (GUIButton* btn : buttons) {
			btn->update(mouseX, mouseY, mousePressed, key);
		}
		for (GUIContainer* tab : tabs) {
			tab->update(mouseX, mouseY, mousePressed, key);
		}
	}

	void draw(SDL_Renderer* renderer, int state) {
		int cBtnX = rect.x;
		int i = 0;
		for (GUIButton* btn : buttons) {
			btn->rect.y = rect.y;
			btn->rect.h = buttonsHeight;
			btn->rect.x = cBtnX;
			cBtnX += btn->rect.w;
			btn->draw(renderer, 0);
			if (btn->isPressedDelta) {
				currentSelectedIndex = i;
			}
			i++;
		}

		GUIContainer* cont = tabs[currentSelectedIndex];
		cont->rect.x = rect.x;
		cont->rect.y = rect.y + buttonsHeight;
		cont->rect.w = rect.w;
		cont->rect.h = rect.h;
		cont->draw(renderer, 0);
	}
};