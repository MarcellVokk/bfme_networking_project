#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#pragma comment(lib, "d3d9.lib")

#include "Game.h"
#include "Renderer.h"
#include "DxUtils.h"

#include "Detours.h"
#include <d3dx9.h>
#include <d3d9.h>

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>

typedef HRESULT (WINAPI* PRESENT)(LPDIRECT3DDEVICE9, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);
PRESENT origPresent = NULL;

typedef HRESULT (WINAPI* RESET)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
RESET origReset = NULL;

HRESULT WINAPI detrPresent(LPDIRECT3DDEVICE9 dev, CONST RECT* a1, CONST RECT* a2, HWND a3, CONST RGNDATA* a4)
{
	__asm pushad
	g_pRenderer.draw(dev);
	__asm popad

	return origPresent(dev, a1, a2, a3, a4);
}

HRESULT WINAPI detrReset(LPDIRECT3DDEVICE9 dev, D3DPRESENT_PARAMETERS* pp)
{
	__asm pushad
	g_pRenderer.reset(dev);
	__asm popad

	return origReset(dev, pp);
}

Renderer g_pRenderer;

void SetGameHooks()
{
	HMODULE hMod = NULL;

	do
	{
		hMod = GetModuleHandleA("d3d9.dll");
		Sleep(10);
	}
	while (!hMod);

	DWORD_PTR* pVtbl = DxUtils::FindDevice((DWORD)hMod, 0x128000);
	DWORD_PTR* vtbl = 0;
	*(DWORD_PTR*)&vtbl = *(DWORD_PTR*)pVtbl;

	origPresent = (PRESENT)vtbl[17];
	origReset = (RESET)vtbl[16];

	DetourRestoreAfterWith();
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	DetourAttach(&(PVOID&)origPresent, detrPresent);
	DetourAttach(&(PVOID&)origReset, detrReset);

	DetourTransactionCommit();

	std::string pipeName = "\\\\.\\pipe\\bfme_api_interface_" + std::to_string(GetCurrentProcessId());
    HANDLE hPipe;
	DWORD bytesRead;
	DWORD bytesWritten;

	try
	{
		hPipe = CreateNamedPipeA(pipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 10000, 10000, 3000, NULL);
		if (hPipe != INVALID_HANDLE_VALUE)
		{
			while (true)
			{
				BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
				if (connected)
				{
					std::vector<char> buffer(1024);
					std::string message;
					bool messageComplete = false;

					do
					{
						if (!ReadFile(hPipe, buffer.data(), 1024, &bytesRead, NULL))
						{
							break;
						}

						for (DWORD i = 0; i < 1024; ++i)
						{
							if (buffer[i] == '\0')
							{
								message.append(buffer.data(), i);
								messageComplete = true;
								break;
							}
						}

						if (!messageComplete)
						{
							message.append(buffer.data(), 1024);
						}

					} while (!messageComplete && bytesRead == 1024);

					std::string response = HandleMessage(message) + "\n";
					WriteFile(hPipe, response.c_str(), response.length(), &bytesWritten, NULL);
					FlushFileBuffers(hPipe);
				}

				DisconnectNamedPipe(hPipe);
			}
		}
	}
	catch (...)
	{
	}

	g_pRenderer.updateOverlayMarkup("text 0 0 0 0 Segoe%20Ui%0 0 255 255 0 0 Bad%20things%0");

	while (true)
	{
		Sleep(100);
	}
}

std::string HandleMessage(std::string message)
{
	size_t pos = message.find('\n');
	std::string command;
	if (pos != std::string::npos)
	{
		command = message.substr(0, pos);
		message.erase(0, pos + 1);
	}

	if (command == "updateOverlayMarkup")
	{
		g_pRenderer.updateOverlayMarkup(message);
		return "OK";
	}
	else
	{
		std::vector<std::string> tokens;
		std::stringstream messageLineStream(message);
		std::string token;
		while (std::getline(messageLineStream, token, ' '))
		{
			tokens.push_back(token);
		}

		if (command == "getPixelColor" && tokens.size() >= 2)
		{
			int posX = std::stoi(tokens[0]);
			int posY = std::stoi(tokens[1]);

			return g_pRenderer.getPixelColor(posX, posY);
		}
		else if (command == "getViewportSize")
		{
			return std::to_string(g_pRenderer._width.load()) + " " + std::to_string(g_pRenderer._height.load());
		}
		else if (command == "inputClick" && tokens.size() >= 2)
		{
			int posX = std::stoi(tokens[0]);
			int posY = std::stoi(tokens[1]);

			g_pRenderer.inputClick(posX, posY);

			return "OK";
		}
		else if (command == "inputMove" && tokens.size() >= 2)
		{
			int posX = std::stoi(tokens[0]);
			int posY = std::stoi(tokens[1]);

			g_pRenderer.inputMove(posX, posY);

			return "OK";
		}
		else if (command == "inputSetLock" && tokens.size() >= 1)
		{
			bool lock = std::stoi(tokens[0]) == 1;

			g_pRenderer.inputSetLock(lock);

			return "OK";
		}
	}

	return "ERR_COMMAND_UNKNOWN";
}
