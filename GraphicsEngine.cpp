#include "GraphicsEngine.h"

#include <iostream>
#include <ostream>
#include <thread>
#include <vector>

int SCREEN_WIDTH = 1500;
int SCREEN_HEIGHT = 1000;

HWND GraphicsEngine::hwnd;
HDC GraphicsEngine::hdcMem = nullptr;        // Persistent memory DC
HGDIOBJ GraphicsEngine::oldBitmap = nullptr; // Old bitmap associated with hdcMem
HBITMAP GraphicsEngine::offscreenBitmap;
uint32_t* GraphicsEngine::bitmapMemory;

FastNoiseLite GraphicsEngine::noise1;
FastNoiseLite GraphicsEngine::noise2;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            EndPaint(hwnd, &ps);
            return 0;
        }
    case WM_KEYDOWN:
        {
            if (wParam == VK_ESCAPE)
            {
                PostQuitMessage(0);
                return 0;
            }
            else if (wParam == VK_UP)
            {
                float frequency = GraphicsEngine::noise1.GetFrequency() * 1.2f;
                GraphicsEngine::noise1.SetFrequency(frequency);
            }
            else if (wParam == VK_DOWN)
            {
                float frequency = GraphicsEngine::noise1.GetFrequency() * 0.8f;
                GraphicsEngine::noise1.SetFrequency(frequency);
            }
            else if (wParam == 'R')
            {
                GraphicsEngine::noise1.SetSeed(rand());
            }
            else if (wParam == 'F')
            {
                FastNoiseLite::NoiseType noiseType = GraphicsEngine::noise1.GetNoiseType();
                GraphicsEngine::noise1.SetNoiseType(static_cast<FastNoiseLite::NoiseType>((noiseType + 1) % 6));
            }
            else if (wParam == 'W')
            {
                float frequency = GraphicsEngine::noise2.GetFrequency() * 1.2f;
                GraphicsEngine::noise2.SetFrequency(frequency);
            }
            else if (wParam == 'S')
            {
                float frequency = GraphicsEngine::noise2.GetFrequency() * 0.8f;
                GraphicsEngine::noise2.SetFrequency(frequency);
            }
            else if (wParam == 'C')
            {
                FastNoiseLite::NoiseType noiseType = GraphicsEngine::noise2.GetNoiseType();
                GraphicsEngine::noise2.SetNoiseType(static_cast<FastNoiseLite::NoiseType>((noiseType + 1) % 6));
            }
            else
            {
                break;
            }

            GraphicsEngine::UpdateScreen();
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void GraphicsEngine::Init()
{
    // Bigger noise
    noise1 = FastNoiseLite();
    noise1.SetSeed(rand());
    noise1.SetFrequency(0.002f);
    noise1.SetNoiseType(FastNoiseLite::NoiseType_Perlin);

    // Smaller noise
    noise2 = FastNoiseLite();
    noise2.SetSeed(rand());
    noise2.SetFrequency(0.015f);
    noise2.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
}

void GraphicsEngine::CreateScreen()
{
    const wchar_t* className = L"GraphicsEngine";
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = className;

    RegisterClass(&wc);

    DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX;


    GraphicsEngine::hwnd = CreateWindowEx(0, className, L"Perlin Noise", windowStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, SCREEN_WIDTH, SCREEN_HEIGHT, nullptr, nullptr,
        wc.hInstance, nullptr);

    if (hwnd == nullptr)
        std::cout << "Error creating window" << std::endl;

    ShowWindow(hwnd, SW_SHOW);

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
}

bool GraphicsEngine::DrawScreen()
{
    // Handle Windows messages
    MSG msg = {};
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            return false;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    HDC hdc = GetDC(hwnd);

    

    // Copy the offscreen bitmap to the window
    if (hdcMem == nullptr)
    {
        hdcMem = CreateCompatibleDC(hdc);
        oldBitmap = SelectObject(hdcMem, offscreenBitmap); // Associate offscreen bitmap with hdcMem
    }

    BitBlt(hdc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hdcMem, 0, 0, SRCCOPY);

    ReleaseDC(hwnd, hdc); // Release the window's HDC

    return true;
}

void GraphicsEngine::UpdateScreen()
{
    // Create or update the offscreen bitmap
    if (offscreenBitmap == nullptr)
    {
        HDC hdc = GetDC(hwnd);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = SCREEN_WIDTH;
        bmi.bmiHeader.biHeight = -SCREEN_HEIGHT; // Negative for top-down bitmap
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32; // 32 bits for ARGB
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        offscreenBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bitmapMemory), nullptr, 0);
        ReleaseDC(hwnd, hdc);
    }

    // Define the number of threads
    const int numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    // Divide the work among threads (rows of pixels)
    int rowsPerThread = SCREEN_HEIGHT / numThreads;
    for (int t = 0; t < numThreads; ++t)
    {
        int startRow = t * rowsPerThread;
        int endRow = (t == numThreads - 1) ? SCREEN_HEIGHT : startRow + rowsPerThread;

        // Launch a thread to process a section of rows
        threads.emplace_back([startRow, endRow]() {
            UpdateSection(startRow, endRow);
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads)
    {
        thread.join();
    }
}

void GraphicsEngine::UpdateSection(int startRow, int endRow)
{
    for (int i = startRow; i < endRow; ++i)
    {
        for (int j = 0; j < SCREEN_WIDTH; ++j)
        {
            uint32_t color = NoiseToColor(GetNoise(i, j)); // Convert noise to ARGB
            bitmapMemory[i * SCREEN_WIDTH + j] = color;   // Write directly to bitmap memory
        }
    }
}

COLORREF GraphicsEngine::NoiseToColor(float noise)
{
    uint8_t red, green, blue;

    // Water
    if (noise < 0.3f)
    {
        float darkener = noise + 0.7f;
        red = static_cast<uint8_t>(29 * darkener);
        green = static_cast<uint8_t>(162 * darkener);
        blue = static_cast<uint8_t>(261 * darkener);
    }
    // Grass
    else if (noise < 0.7f)
    {
        float darkener = noise + 0.3f;
        red = static_cast<uint8_t>(124 * darkener);
        green = static_cast<uint8_t>(252 * darkener);
        blue = 0;
    }
    // Mountains
    else
    {
        uint8_t value = static_cast<uint8_t>(noise * 255);
        red = value;
        green = value;
        blue = value;
    }

    // Return the color in BGRA format
    return (blue) | (green << 8) | (red << 16) | (0xFF << 24); // 0xFF for fully opaque alpha
}

float GraphicsEngine::GetNoise(int i, int j)
{
    float iFloat = static_cast<float>(i);
    float jFloat = static_cast<float>(j);

    float noiseValue1 = (noise1.GetNoise(iFloat, jFloat) + 1) * 0.5f * 0.75f;
    float noiseValue2 = (noise2.GetNoise(iFloat, jFloat) + 1) * 0.5f * 0.25f;
    
    return (noiseValue1 + noiseValue2);
}
