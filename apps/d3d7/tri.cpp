/**************************************************************************
 *
 * Copyright 2012 Jose Fonseca
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


#include <initguid.h>

#include <windows.h>

#include <d3d.h>


static LPDIRECTDRAW7 g_pDD = NULL;
static LPDIRECT3D7 g_pD3D = NULL;
static LPDIRECT3DDEVICE7 g_pDevice = NULL;
static LPDIRECTDRAWSURFACE7 g_pddsPrimary;
static LPDIRECTDRAWSURFACE7 g_pddsBackBuffer;
static RECT g_rcScreenRect;
static RECT g_rcViewportRect;


int main(int argc, char *argv[])
{
    HRESULT hr;

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        DefWindowProc,
        0,
        0,
        hInstance,
        NULL,
        NULL,
        NULL,
        NULL,
        "SimpleDX7",
        NULL
    };
    RegisterClassEx(&wc);

    const int WindowWidth = 250;
    const int WindowHeight = 250;
    const int WindowDepth = 4;
    BOOL Windowed = TRUE;

    DWORD dwStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW;

    RECT rect = {0, 0, WindowWidth, WindowHeight};
    AdjustWindowRect(&rect, dwStyle, FALSE);

    HWND hWnd = CreateWindow(wc.lpszClassName,
                             "Simple example using DirectX7",
                             dwStyle,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             rect.right - rect.left,
                             rect.bottom - rect.top,
                             NULL,
                             NULL,
                             hInstance,
                             NULL);
    if (!hWnd) {
        return 1;
    }

    ShowWindow(hWnd, SW_SHOW);

    hr = DirectDrawCreateEx(NULL, (void **)&g_pDD, IID_IDirectDraw7, NULL);
    if (FAILED(hr)) {
        return 1;
    }

    hr = g_pDD->SetCooperativeLevel(hWnd, DDSCL_NORMAL);
    if (FAILED(hr)) {
        return 1;
    }

    /*
     * Create primary
     */

    DDSURFACEDESC2 ddsd;
    ZeroMemory(&ddsd, sizeof ddsd);
    ddsd.dwSize         = sizeof ddsd;
    ddsd.dwFlags        = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    hr = g_pDD->CreateSurface(&ddsd, &g_pddsPrimary, NULL);
    if (FAILED(hr)) {
        return 1;
    }

    /*
     * Create backbuffer
     */

    ddsd.dwFlags        = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;

    GetClientRect(hWnd, &g_rcScreenRect);
    GetClientRect(hWnd, &g_rcViewportRect);
    ClientToScreen(hWnd, (POINT*)&g_rcScreenRect.left);
    ClientToScreen(hWnd, (POINT*)&g_rcScreenRect.right);
    ddsd.dwWidth  = g_rcScreenRect.right - g_rcScreenRect.left;
    ddsd.dwHeight = g_rcScreenRect.bottom - g_rcScreenRect.top;

    hr = g_pDD->CreateSurface(&ddsd, &g_pddsBackBuffer, NULL);
    if (FAILED(hr)) {
        return 1;
    }

    LPDIRECTDRAWCLIPPER pcClipper;
    hr = g_pDD->CreateClipper(0, &pcClipper, NULL);
    if (FAILED(hr)) {
        return 1;
    }

    pcClipper->SetHWnd(0, hWnd);
    g_pddsPrimary->SetClipper(pcClipper);
    pcClipper->Release();

    /*
     * Initialize D3D
     */

    hr = g_pDD->QueryInterface(IID_IDirect3D7, (void **)&g_pD3D);
    if (FAILED(hr)) {
        return 1;
    }

    hr = g_pD3D->CreateDevice(IID_IDirect3DHALDevice, g_pddsBackBuffer, &g_pDevice);
    if (FAILED(hr)) {
        g_pD3D->Release();
        g_pD3D = NULL;
        return 1;
    }

    struct Vertex {
        float x, y, z;
        DWORD color;
    };


    D3DCOLOR clearColor = D3DRGBA(0.3f, 0.1f, 0.3f, 1.0f);
    g_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, clearColor, 1.0f, 0);
    g_pDevice->BeginScene();

    g_pDevice->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
    g_pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);

    static Vertex vertices[] = {
        { -0.9f, -0.9f, 0.5f, D3DRGBA(0.8f, 0.0f, 0.0f, 0.1f) },
        {  0.9f, -0.9f, 0.5f, D3DRGBA(0.0f, 0.9f, 0.0f, 0.1f) },
        {  0.0f,  0.9f, 0.5f, D3DRGBA(0.0f, 0.0f, 0.7f, 0.1f) },
    };

    g_pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, D3DFVF_XYZ | D3DFVF_DIFFUSE, vertices, 3, 0);

    g_pDevice->EndScene();

    /*
     * Present
     */
    hr = g_pddsPrimary->Blt(&g_rcScreenRect, g_pddsBackBuffer, &g_rcViewportRect, DDBLT_WAIT, NULL);
    if (FAILED(hr)) {
        return 1;
    }

    g_pDevice->Release();
    g_pDevice = NULL;
    g_pD3D->Release();
    g_pD3D = NULL;

    DestroyWindow(hWnd);

    return 0;
}
