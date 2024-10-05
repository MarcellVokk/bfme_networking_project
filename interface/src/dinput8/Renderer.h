#pragma once
#include <d3dx9.h>

#include <memory>
#include <map>
#include <functional>
#include <mutex>

class RenderBase;

class Renderer
{
public:
	void draw(IDirect3DDevice9 *pDevice);
	void reset(IDirect3DDevice9 *pDevice);

	void updateOverlayMarkup(const std::string& newOverlayMarkup);
	std::string Renderer::getPixelColor(int x, int y);
	void inputClick(int x, int y);
	void inputMove(int x, int y);
	void inputSetLock(bool isLocked);

	std::atomic<int> _width;
	std::atomic<int> _height;
};

