#include "DxUtils.h"

#include <d3dx9.h>
#include <d3d9.h>

struct Vertex
{
    D3DXVECTOR3 position;
    D3DXVECTOR2 texcoord;
};

// Custom FVF (Flexible Vertex Format)
#define D3DFVF_CUSTOM (D3DFVF_XYZ | D3DFVF_TEX1)

DWORD_PTR* DxUtils::FindDevice(DWORD Base, DWORD Len)
{
	unsigned long i = 0, n = 0;

	for (i = 0; i < Len; i++)
	{
		if (*(BYTE*)(Base + i + 0x00) == 0xC7)n++;
		if (*(BYTE*)(Base + i + 0x01) == 0x06)n++;
		if (*(BYTE*)(Base + i + 0x06) == 0x89)n++;
		if (*(BYTE*)(Base + i + 0x07) == 0x86)n++;
		if (*(BYTE*)(Base + i + 0x0C) == 0x89)n++;
		if (*(BYTE*)(Base + i + 0x0D) == 0x86)n++;

		if (n == 6)
			return (DWORD_PTR*)(Base + i + 2); n = 0;
	}
	return(0);
}

void DxUtils::RenderText(LPDIRECT3DDEVICE9 device, const std::wstring& text, int x, int y, const std::string& fontFamily, int fontSize, DWORD dwColor)
{
    // Don't bother drawing if the text is empty
    if (text.size() <= 1)
    {
        return;
    }

    // Initialize render state blocks
    LPDIRECT3DSTATEBLOCK9 pStateBlockSaved = NULL;
    LPDIRECT3DSTATEBLOCK9 pStateBlockDrawText = NULL;

    // Create the state blocks for rendering text
    for (UINT which = 0; which < 2; which++)
    {
        device->BeginStateBlock();

        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHAREF, 0x08);
        device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_CLIPPING, TRUE);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, FALSE);
        device->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
        device->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

        if (which == 0)
            device->EndStateBlock(&pStateBlockSaved);
        else
            device->EndStateBlock(&pStateBlockDrawText);
    }

    pStateBlockSaved->Capture();
    pStateBlockDrawText->Apply();

    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    //device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 16);

    // Prepare to create a bitmap
    DWORD* pBitmapBits;
    BITMAPINFO bmi;
    ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 256;
    bmi.bmiHeader.biHeight = -256;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biBitCount = 32;

    HDC hDC = CreateCompatibleDC(NULL);
    SetMapMode(hDC, MM_TEXT);

    // Create the font
    HFONT hFont = CreateFont(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE | DEFAULT_PITCH, fontFamily.c_str());
    SelectObject(hDC, hFont);

    // Set text properties
    SetTextColor(hDC, RGB(255, 255, 255));
    SetBkColor(hDC, 0xffffff00); // Transparent background
    SetTextAlign(hDC, TA_TOP);

    // Measure text
    SIZE size;
    GetTextExtentPoint32W(hDC, text.c_str(), text.length() - 1, &size);

    // Update bitmap size based on text size
    bmi.bmiHeader.biWidth = size.cx;
    bmi.bmiHeader.biHeight = -size.cy;

    // Create a bitmap for the text
    HBITMAP hbmBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (void**)&pBitmapBits, NULL, 0);
    SelectObject(hDC, hbmBitmap);

    // Output text to bitmap
    ExtTextOutW(hDC, 0, 0, ETO_OPAQUE, NULL, text.c_str(), text.length(), NULL);

    // Create texture from the bitmap
    LPDIRECT3DTEXTURE9 texture;
    HRESULT hr = device->CreateTexture(size.cx, size.cy, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL);

    if (FAILED(hr))
    {
        DeleteObject(hbmBitmap);
        DeleteObject(hFont);
        DeleteDC(hDC);
        return;
    }

    // Lock the surface and write the alpha values for the set pixels
    D3DLOCKED_RECT d3dlr;
    texture->LockRect(0, &d3dlr, 0, 0);
    BYTE* pDstRow = (BYTE*)d3dlr.pBits;
    DWORD* pDst32;
    BYTE bAlpha;

    for (LONG y = 0; y < size.cy; y++)
    {
        pDst32 = (DWORD*)pDstRow;
        for (LONG x = 0; x < size.cx; x++)
        {
            bAlpha = (BYTE)(pBitmapBits[size.cx * y + x] & 0xff);  // Extract alpha (intensity)

            // Apply the original text color (dwColor) and use the extracted alpha channel
            if (bAlpha > 0)
            {
                DWORD r = (dwColor & 0x00FF0000) >> 16;
                DWORD g = (dwColor & 0x0000FF00) >> 8;
                DWORD b = (dwColor & 0x000000FF);
                DWORD finalColor = (bAlpha << 24) | (r << 16) | (g << 8) | b;

                *pDst32++ = finalColor;
            }
            else
            {
                *pDst32++ = 0x00ffffff;  // Transparent pixel
            }
        }
        pDstRow += d3dlr.Pitch;
    }

    texture->UnlockRect(0);

    struct Vertex
    {
        float x, y, z, rhw;
        float u, v;
    };

    Vertex quad[] =
    {
        { (float)x, (float)y, 0.0f, 1.0f, 0.0f, 0.0f },
        { (float)x + size.cx, (float)y, 0.0f, 1.0f, 1.0f, 0.0f },
        { (float)x, (float)y + size.cy, 0.0f, 1.0f, 0.0f, 1.0f },
        { (float)x + size.cx, (float)y + size.cy, 0.0f, 1.0f, 1.0f, 1.0f }
    };

    device->SetTexture(0, texture);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));

    // Cleanup
    DeleteObject(hbmBitmap);
    DeleteObject(hFont);
    DeleteDC(hDC);
    texture->Release();

    // Restore the saved render states
    pStateBlockSaved->Apply();
    pStateBlockSaved->Release();
    pStateBlockDrawText->Release();
}

void DxUtils::MeasureText(const std::wstring& text, const std::string& fontFamily, int fontSize, LPSIZE size)
{
    HDC hDC = CreateCompatibleDC(NULL);
    SetMapMode(hDC, MM_TEXT);

    // Create the font
    HFONT hFont = CreateFont(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE | DEFAULT_PITCH, fontFamily.c_str());
    SelectObject(hDC, hFont);

    // Measure text
    GetTextExtentPoint32W(hDC, text.c_str(), text.length() - 1, size);

    // Cleanup
    DeleteObject(hFont);
    DeleteDC(hDC);
}