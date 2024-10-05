#include <algorithm>
#include <chrono>
#include <filesystem>
#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <locale>
#include <codecvt>
#include <Windows.h>

#include "Version.h"
#include "Renderer.h"
#include "StringUtils.h"
#include "DxUtils.h"
#include <fstream>

std::mutex requestMutex;
std::condition_variable requestWorker;
std::atomic<bool> requestAvailable(false);
std::atomic<bool> requestReady(false);

std::mutex overlayMutex;
std::string overlayMarkup = "";

std::atomic<int> _width(0);
std::atomic<int> _height(0);

RECT cursorClip = { sizeof(RECT) };
IDirect3DSurface9* pDestSurface = nullptr;
ID3DXLine* pLine = nullptr;

bool _captureRequested = false;
int _captureRequestedX = 0;
int _captureRequestedY = 0;
int _capturedR = 0;
int _capturedG = 0;
int _capturedB = 0;

bool _cursorClickRequested = false;
bool _cursorMoveRequested = false;
float _cursorRequestedX = 0.0f;
float _cursorRequestedY = 0.0f;

bool _inputBlocked = false;

bool _updateInputLock = false;
bool _updateCursorClip = false;

void Renderer::draw(IDirect3DDevice9 *pDevice)
{
	if (pDevice == nullptr)
	{
		return;
	}

	int _vWidth = 0;
	int _vHeight = 0;

	// Get frame's screen bounds
	{
		D3DVIEWPORT9 viewPort;
		if (SUCCEEDED(pDevice->GetViewport(&viewPort)))
		{
			_vWidth = viewPort.Width;
			_vHeight = viewPort.Height;

			_width.store(_vWidth);
			_height.store(_vHeight);

			if (_vWidth == 0 || _vHeight == 0)
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	if (requestAvailable.load())
	{
		std::unique_lock<std::mutex> lock(requestMutex);

		// Process input requests
		{
			if (_vWidth != 0 && _vHeight != 0)
			{
				try
				{
					if (_updateInputLock)
					{
						_updateInputLock = false;

						BlockInput(_inputBlocked);

						_updateCursorClip = true;
					}

					if (_updateCursorClip || _cursorClickRequested || _cursorMoveRequested)
					{
						_updateCursorClip = false;

						if (_inputBlocked)
						{
							cursorClip.left = int(_cursorRequestedX * _vWidth);
							cursorClip.top = int(_cursorRequestedY * _vHeight);
							cursorClip.right = int(_cursorRequestedX * _vWidth) + 1;
							cursorClip.bottom = int(_cursorRequestedY * _vHeight) + 1;
						}
						else
						{
							cursorClip.left = 0;
							cursorClip.top = 0;
							cursorClip.right = _vWidth;
							cursorClip.bottom = _vHeight;
						}

						ClipCursor(&cursorClip);
					}

					if (_cursorClickRequested)
					{
						_cursorClickRequested = false;

						INPUT inputMouseDown = { sizeof(INPUT) };
						inputMouseDown.type = 0; /// input type mouse
						inputMouseDown.mi.dx = (int)(_cursorRequestedX * 65535.0f);
						inputMouseDown.mi.dy = (int)(_cursorRequestedY * 65535.0f);
						inputMouseDown.mi.dwFlags = 0x0002 | 0x8000 | 0x0001 | 0x2000; /// left down | absolute | move | nocoalesce

						INPUT inputMouseUp = { sizeof(INPUT) };
						inputMouseUp.type = 0; /// input type mouse
						inputMouseUp.mi.dx = (int)(_cursorRequestedX * 65535.0f);
						inputMouseUp.mi.dy = (int)(_cursorRequestedY * 65535.0f);
						inputMouseUp.mi.dwFlags = 0x0004 | 0x8000 | 0x0001 | 0x2000; /// left up | absolute | move | nocoalesce

						INPUT input[] = { inputMouseDown, inputMouseUp };
						SendInput(2, input, sizeof(INPUT));
					}

					if (_cursorMoveRequested)
					{
						_cursorMoveRequested = false;

						INPUT inputMouseMove = { sizeof(INPUT) };
						inputMouseMove.type = 0; /// input type mouse
						inputMouseMove.mi.dx = (int)(_cursorRequestedX * 65535.0f);
						inputMouseMove.mi.dy = (int)(_cursorRequestedY * 65535.0f);
						inputMouseMove.mi.dwFlags = 0x8000 | 0x0001 | 0x2000; /// absolute | move | nocoalesce

						INPUT input[] = { inputMouseMove };
						SendInput(1, input, sizeof(INPUT));
					}
				}
				catch (...)
				{
				}
			}
		}

		// Process capture requests
		{
			if (_vWidth != 0 && _vHeight != 0)
			{
				if (_captureRequested)
				{
					_captureRequested = false;

					IDirect3DSurface9* pBackBuffer = nullptr;

					try
					{
						// Get backbuffer
						if (FAILED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
							throw;

						// Get backbuffer format
						D3DSURFACE_DESC desc;
						if (FAILED(pBackBuffer->GetDesc(&desc)))
							throw;

						// Invalidate destSurface if the format or viewport size changed
						if (pDestSurface)
						{
							// Get destSurface format
							D3DSURFACE_DESC destSurfaceDesc;
							if (FAILED(pDestSurface->GetDesc(&destSurfaceDesc)))
								throw;

							if (destSurfaceDesc.Width != _vWidth || destSurfaceDesc.Height != _vHeight || destSurfaceDesc.Format != desc.Format)
							{
								pDestSurface->Release();
								pDestSurface = nullptr;
							}
						}

						// Create destSurface if needed
						if (!pDestSurface)
						{
							if (FAILED(pDevice->CreateOffscreenPlainSurface(_vWidth, _vHeight, desc.Format, D3DPOOL_SYSTEMMEM, &pDestSurface, nullptr)))
								throw;
						}

						// Copy backbuffer to destSurface (system memory)
						if (FAILED(pDevice->GetRenderTargetData(pBackBuffer, pDestSurface)))
							throw;

						// Lock destSurface so we can access the pixel data
						D3DLOCKED_RECT lockedRect;
						if (FAILED(pDestSurface->LockRect(&lockedRect, nullptr, 0)))
							throw;

						try
						{
							DWORD* pBuffer = static_cast<DWORD*>(lockedRect.pBits);
							DWORD p = pBuffer[int(_captureRequestedY / 1440.0f * _vHeight) * (lockedRect.Pitch / 4) + int(_captureRequestedX / 2560.0f * _vWidth)];
							BYTE r = static_cast<BYTE>((p >> 16) & 0xFF);
							BYTE g = static_cast<BYTE>((p >> 8) & 0xFF);
							BYTE b = static_cast<BYTE>(p & 0xFF);

							_capturedR = r;
							_capturedG = g;
							_capturedB = b;
						}
						catch (...)
						{
						}

						// Unlock destSurface
						pDestSurface->UnlockRect();
					}
					catch (...)
					{
					}

					// Release the backbuffer if needed
					if (pBackBuffer)
						pBackBuffer->Release();
				}
			}
		}

		requestAvailable.store(false);
		requestReady.store(true);
		lock.unlock();
		requestWorker.notify_one();
	}

	// Draw overlay
	{
		std::string overlayMarkupToRender;
		{
			std::unique_lock<std::mutex> lock(overlayMutex);
			overlayMarkupToRender = overlayMarkup;
			overlayMarkupToRender += "\ntext 0 " + std::to_string(_vHeight) + " 0 2 Segoe%20Ui 23 170 255 255 255 BFME%20INTERFACE%20" + BFME_INTERFACE_VERSION + "%0";
		}

		std::vector<int> svec;
		std::vector<int> dvec;

		int hstack = 0;
		int vstack = 0;
		int mxh = 0;
		int mxv = 0;
		int dir = -1;

		std::stringstream markupStream(overlayMarkupToRender);
		std::string markupLine;
		while (std::getline(markupStream, markupLine))
		{
			if (markupLine.size() == 0)
			{
				continue;
			}

			std::vector<std::string> tokens;
			std::stringstream markupLineStream(markupLine);
			std::string token;
			while (std::getline(markupLineStream, token, ' '))
			{
				tokens.push_back(StringUtils::ReplaceAll(token, "%0", ""));
			}

			if (tokens.size() <= 0)
			{
				continue;
			}

			int h = hstack;
			int v = vstack;

			if (tokens[0] == "text")
			{
				if (tokens.size() < 12)
				{
					continue;
				}

				int posX = std::stoi(tokens[1]) + hstack;
				int posY = std::stoi(tokens[2]) + vstack;
				int alignX = std::stoi(tokens[3]);
				int alignY = std::stoi(tokens[4]);
				std::string fontFamily = StringUtils::ReplaceAll(tokens[5], "%20", " ");
				int fontSize = std::stoi(tokens[6]);
				int colorA = std::stoi(tokens[7]);
				int colorR = std::stoi(tokens[8]);
				int colorG = std::stoi(tokens[9]);
				int colorB = std::stoi(tokens[10]);
				std::string content = StringUtils::ReplaceAll(tokens[11], "%20", " ");
				int wideLength = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, nullptr, 0);
				std::wstring contentW(wideLength, L'\0');
				MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, &contentW[0], wideLength);

				if (contentW == L"%1")
				{
					contentW = L"✓";
				}

				// Draw text
				{
					SIZE size;
					DxUtils::MeasureText(contentW, fontFamily, fontSize, &size);

					int finalX = 0, finalY = 0;
					if (alignX == 0)
					{
						finalX = posX + 5;
					}
					else if (alignX == 1)
					{
						finalX = posX - size.cx / 2;
					}
					else if (alignX == 2)
					{
						finalX = posX - size.cx - 5;
					}

					if (alignY == 0)
					{
						finalY = posY + 5;
					}
					else if (alignY == 1)
					{
						finalY = posY - size.cy / 2;
					}
					else if (alignY == 2)
					{
						finalY = posY - size.cy - 5;
					}

					DxUtils::RenderText(pDevice, contentW, finalX, finalY, fontFamily, fontSize, D3DCOLOR_ARGB(colorA, colorR, colorG, colorB));

					h = posX + size.cx;
					v = posY + size.cy;
				}
			}
			else if (tokens[0] == "rect")
			{
				if (tokens.size() < 9)
				{
					continue;
				}

				float posX = std::stof(tokens[1]) + hstack;
				float posY = std::stof(tokens[2]) + vstack;
				float width = std::stof(tokens[3]);
				float height = std::stof(tokens[4]);
				int colorA = std::stoi(tokens[5]);
				int colorR = std::stoi(tokens[6]);
				int colorG = std::stoi(tokens[7]);
				int colorB = std::stoi(tokens[8]);

				// Create line if needed
				if (!pLine)
				{
					D3DXCreateLine(pDevice, &pLine);
				}

				// Draw line
				if (pLine)
				{
					pLine->SetWidth(height);
					pLine->SetAntialias(TRUE);

					D3DXVECTOR2 line[] = { D3DXVECTOR2((posX + 5), (posY + 5) + height / 2), D3DXVECTOR2((posX + 5) + width, (posY + 5) + height / 2) };

					pLine->Begin();
					pLine->Draw(line, 2, D3DCOLOR_ARGB(colorA, colorR, colorG, colorB));
					pLine->End();

					h = int(posX) + int(width);
					v = int(posY) + int(height);
				}
			}
			else if (tokens[0] == "spacer")
			{
				if (tokens.size() < 2)
				{
					continue;
				}

				int size = std::stoi(tokens[1]);

				h += size;
				v += size;
			}

			if (dir == 0)
			{
				vstack = v;
			}
			else if (dir == 1)
			{
				hstack = h;
			}

			if (h > mxh)
			{
				mxh = h;
			}

			if (v > mxv)
			{
				mxv = v;
			}

			if (tokens[0] == "beginstack")
			{
				if (tokens.size() < 2)
				{
					continue;
				}

				svec.push_back(vstack);
				svec.push_back(hstack);
				dvec.push_back(dir);

				dir = std::stoi(tokens[1]);
				mxh = hstack;
				mxv = vstack;
			}
			else if (tokens[0] == "endstack")
			{
				if (svec.size() < 2 || dvec.size() < 1)
				{
					continue;
				}

				hstack = svec.back();
				svec.pop_back();
				vstack = svec.back();
				svec.pop_back();

				if (dvec.back() == 1 && dir == 0)
				{
					hstack = mxh;
				}
				else if (dvec.back() == 0 && dir == 1)
				{
					vstack = mxv;
				}

				dir = dvec.back();
				dvec.pop_back();
			}
		}
	}
}

void Renderer::reset(IDirect3DDevice9 *pDevice)
{
	std::unique_lock<std::mutex> lock(requestMutex);

	if (pLine)
	{
		pLine->Release();
		pLine = nullptr;
	}

	_updateInputLock = true;
	_updateCursorClip = true;
	requestAvailable.store(true);
}

void Renderer::updateOverlayMarkup(const std::string& newOverlayMarkup)
{
	std::unique_lock<std::mutex> lock(overlayMutex);

	overlayMarkup = newOverlayMarkup;
}

std::string Renderer::getPixelColor(int x, int y)
{
	{
		std::lock_guard<std::mutex> lock(requestMutex);
		_captureRequestedX = x;
		_captureRequestedY = y;
		_captureRequested = true;
		requestAvailable.store(true);
	}

	std::unique_lock<std::mutex> lock(requestMutex);
	requestWorker.wait(lock, [] { return requestReady.load(); });
	requestReady.store(false);

	return std::to_string(_capturedR) + " " + std::to_string(_capturedG) + " " + std::to_string(_capturedB);
}

void Renderer::inputClick(int x, int y)
{
	{
		std::lock_guard<std::mutex> lock(requestMutex);
		_cursorRequestedX = float(x) / 2560.0f;
		_cursorRequestedY = float(y) / 1440.0f;
		_cursorClickRequested = true;
		requestAvailable.store(true);
	}

	std::unique_lock<std::mutex> lock(requestMutex);
	requestWorker.wait(lock, [] { return requestReady.load(); });
	requestReady.store(false);
}

void Renderer::inputMove(int x, int y)
{
	{
		std::lock_guard<std::mutex> lock(requestMutex);
		_cursorRequestedX = float(x) / 2560.0f;
		_cursorRequestedY = float(y) / 1440.0f;
		_cursorMoveRequested = true;
		requestAvailable.store(true);
	}

	std::unique_lock<std::mutex> lock(requestMutex);
	requestWorker.wait(lock, [] { return requestReady.load(); });
	requestReady.store(false);
}

void Renderer::inputSetLock(bool isLocked)
{
	{
		std::lock_guard<std::mutex> lock(requestMutex);
		_inputBlocked = isLocked;
		_updateInputLock = true;
		requestAvailable.store(true);
	}

	std::unique_lock<std::mutex> lock(requestMutex);
	requestWorker.wait(lock, [] { return requestReady.load(); });
	requestReady.store(false);
}