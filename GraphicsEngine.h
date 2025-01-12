#pragma once
#include <cstdint>
#include <windows.h>
#include "FastNoiseLite.h"

class GraphicsEngine
{
public:
    static HWND hwnd;
    static HDC hdcMem;
    static HGDIOBJ oldBitmap;
    static HBITMAP offscreenBitmap;
    static uint32_t* bitmapMemory;

    static FastNoiseLite noise1;
    static FastNoiseLite noise2;
public:
    static void Init();
    static void CreateScreen();
    static bool DrawScreen();
    static void UpdateScreen();
    static void UpdateSection(int startRow, int endRow);

    static COLORREF NoiseToColor(float noise);
    static float GetNoise(int i, int j);
};
