#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#include <d3dx9.h>
#include <d3d9.h>

class DxUtils
{
public:
    static DWORD_PTR* FindDevice(DWORD Base, DWORD Len);
    static void RenderText(LPDIRECT3DDEVICE9 device, const std::wstring& text, int x, int y, const std::string& fontFamily, int fontSize, DWORD dwColor);
    static void MeasureText(const std::wstring& text, const std::string& fontFamily, int fontSize, LPSIZE size);
};