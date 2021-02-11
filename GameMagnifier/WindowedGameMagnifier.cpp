/*************************************************************************************************
*
* File: WindowedGameMagnifier.cpp
*
* Description: Implements a magnifier that is controlled by XInput
* This program uses Microsoft's Magnification and XInput API.
* 
* Basic commands: 
* -Pressing both analogs at the same time toggles the magnifier.
* -Use the right analog to move the magnifier.
* -Holding both analogs up for a few seconds closes the program.
* -Holding LT will hold the magnifier in place.
* 
* If you want to use the Guide Button to toggle the magnifier on and off, 
* follow the steps below:
* 1. Put xinput1_3.dll in this apps folder.
* 
* You are done if your version of Windows is below Windows 10.
* For Windows 10, it seems that there is an update that disables the Guide Button, 
* and in order to get it working again you will have to follow a few more steps 
* (Credits to nullskillz from Steam forums):
* 
* 2. Go to Device Manager > XBOX 360 Peripherals > Right click your XBOX Controller > 
* Update Driver > Search in computer > Pick from a list > 
* Now choose a different driver from the list and reboot your PC. 
* For me there were only 2 drivers in the list and they had exactly the same name,  
* I tried the first one and it worked. 
* 
* The program will work on Windowed and Windowed Borderless modes. Fullscreen is not supported.
* 
*************************************************************************************************/

// Ensure that the following definition is in effect before winuser.h is included.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501    
#endif

#include <windows.h>
#include <wincodec.h>
#include "resource.h"
#include <WinUser.h>
#include <strsafe.h>
#include <magnification.h>
#include <Xinput.h>
#include <math.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>

#pragma comment(lib,"XInput.lib")
#pragma comment(lib,"Xinput9_1_0.lib")

// For simplicity, the sample uses a constant magnification factor.
#define MAGFACTOR 2.0f
#define RESTOREDWINDOWSTYLES WS_SIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN | WS_CAPTION | WS_MAXIMIZEBOX

// Global variables and strings.
HINSTANCE           hInst;
HWND                hwndMag;
HWND                hwndHost;
RECT                magWindowRect;
RECT                hostWindowRect;
const TCHAR         WindowClassName[] = TEXT("WindowedGameMagnifier");
const TCHAR         WindowTitle[] = TEXT("Windowed Game Magnifier");
const int           FPS = 60;
const int           timerInterval = 1000 / FPS; // close to the refresh rate @60hz
const int           LENS_WIDTH = 800;
const int           LENS_HEIGHT = 600;
const INT           HKQUIT = 101;
const INT           HKTOGGLE = 100;
const INT           HKLEFT = 200;
const INT           HKRIGHT = 300;
const INT           HKUP = 400;
const INT           HKDOWN = 500;
const FLOAT         INPUT_DEADZONE = 0.2f;
const FLOAT         INPUT_MAX_ANALOG_VALUE = 32767.0f;
const INT           GUIDE_BUTTON_VALUE = 1024; // 0x0400

// Forward declarations.
ATOM                RegisterHostWindowClass(HINSTANCE hInstance);
BOOL                SetupMagnifier(HINSTANCE hinst);
//LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void UpdateMagWindow(_In_ int x, _In_ int y);
BOOL                isFullScreen = FALSE;
float sign(_In_ float n);


// Guide Button Setups
typedef struct
{
    unsigned long eventCount;
    WORD                                wButtons;
    BYTE                                bLeftTrigger;
    BYTE                                bRightTrigger;
    SHORT                               sThumbLX;
    SHORT                               sThumbLY;
    SHORT                               sThumbRX;
    SHORT                               sThumbRY;
} XINPUT_GAMEPAD_SECRET;

typedef int(__stdcall* secret_get_gamepad) (int, XINPUT_GAMEPAD_SECRET*); // function pointer to secret (guide button compatible) getstate 
HINSTANCE xinput_dll = LoadLibrary(L"xinput1_3.dll");
secret_get_gamepad  gpad = (secret_get_gamepad) GetProcAddress(xinput_dll, (LPCSTR)100); // load ordinal 100
// End Guide Button Setups

//
// FUNCTION: WinMain()
//
// PURPOSE: Entry point for the application.
//
int APIENTRY WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPSTR     /*lpCmdLine*/,
    _In_ int       nCmdShow)
{

    bool quit = false;
    bool isMag = true;
    int toggleTimer = 0;
    int quitTimer = 0;

    float xSpd = 0;
    float ySpd = 0;
    float maxSpd = 20;
    float brakeSpd = 3.0f;
    float accelMult = 5;

    float x = (float)GetSystemMetrics(SM_CXSCREEN) / 2.0f;// + ((float)LENS_WIDTH / 2.0f);
    float y = (float)GetSystemMetrics(SM_CYSCREEN) / 2.0f;// + ((float)LENS_HEIGHT / 2.0f);


    //UpdateWindow(hwndHost);

    // Start with magnifier already open
    if (isMag) {
        isMag = true;
        toggleTimer = 99999; // Can toggle magnifier instantly
        // Create Window
        MagInitialize();
        SetupMagnifier(hInstance);
        ShowWindow(hwndHost, nCmdShow);
    }


    // Main loop.
    BOOL bRet; // Window messages (maximize, minimize, etc)
    MSG msg;
    DWORD dwResult; // Controller
    while (!quit)
        //while ((bRet = PeekMessage(&msg, hwndHost, 0, 0, PM_REMOVE)) != 0)
        //while (GetMessage(&msg, hwndHost, 0, 0))
    {
        if (quit) {
            if (isMag) {
                MagUninitialize();
                DestroyWindow(hwndHost);
            }
            return 0;
        }


        // Controller stuff
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
        {
            XINPUT_STATE state;
            ZeroMemory(&state, sizeof(XINPUT_STATE));

            // Simply get the state of the controller from XInput.
            dwResult = XInputGetState(i, &state);

            if (dwResult == ERROR_SUCCESS)
            {

                bool GB = 0; // Guide Button (a.k.a Home)

                if (xinput_dll) {
                    XINPUT_GAMEPAD_SECRET dllGamepad;
                    gpad(i, &dllGamepad);
                    //LPCWSTR guidePressed = L"false";
                    //if (state.Gamepad.wButtons & GUIDE_BUTTON_VALUE)
                    if (dllGamepad.wButtons & GUIDE_BUTTON_VALUE)
                    {
                        //guidePressed = L"true ";
                        //OutputDebugString(L"Guide button is down.\n");
                        GB = 1;
                    }
                }

                bool leftAnalogPressed = state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
                bool rightAnalogPressed = state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;

                bool LT = state.Gamepad.bLeftTrigger;
                

                // Left analog
                float LX = state.Gamepad.sThumbLX;
                float LY = state.Gamepad.sThumbLY*-1;
                float lxAbs = abs(LX);
                float lyAbs = abs(LY);
                float lxSign = sign(LX);
                float lySign = sign(LY);

                if (lxAbs < INPUT_MAX_ANALOG_VALUE * INPUT_DEADZONE) { LX = 0; }
                if (lxAbs > INPUT_MAX_ANALOG_VALUE) { LX = INPUT_MAX_ANALOG_VALUE * lxSign; }

                if (lyAbs < INPUT_MAX_ANALOG_VALUE * INPUT_DEADZONE) { LY = 0; }
                if (lyAbs > INPUT_MAX_ANALOG_VALUE) { LY = INPUT_MAX_ANALOG_VALUE * lySign; }

                lxSign = sign(LX);
                lySign = sign(LY);

                // Right analog
                float RX = state.Gamepad.sThumbRX;
                float RY = state.Gamepad.sThumbRY * -1.0f;
                float rxAbs = abs(RX);
                float ryAbs = abs(RY);
                float rxSign = sign(RX);
                float rySign = sign(RY);


                if (rxAbs < INPUT_MAX_ANALOG_VALUE * INPUT_DEADZONE) { RX = 0; }
                if (rxAbs > INPUT_MAX_ANALOG_VALUE) { RX = INPUT_MAX_ANALOG_VALUE * rxSign; }

                if (ryAbs < INPUT_MAX_ANALOG_VALUE * INPUT_DEADZONE) { RY = 0; }
                if (ryAbs > INPUT_MAX_ANALOG_VALUE) { RY = INPUT_MAX_ANALOG_VALUE * rySign; }

                float rxRatio = RX / INPUT_MAX_ANALOG_VALUE;
                float ryRatio = RY / INPUT_MAX_ANALOG_VALUE;

                rxSign = sign(RX);
                rySign = sign(RY);

                std::wstringstream s;
                s << "RX: " << RX << ", RY: " << RY << ", LT: " << LT << "\n";
                OutputDebugString(s.str().c_str());

                if (RX > 0) {
                    xSpd += abs(rxRatio) * accelMult;
                }
                else if (RX < 0) {
                    xSpd -= abs(rxRatio) * accelMult;
                }
                else {
                    if (xSpd > 0) {
                        xSpd -= brakeSpd;
                        if (xSpd < 0) { xSpd = 0; }
                    }
                    if (xSpd < 0) {
                        xSpd += brakeSpd;
                        if (xSpd > 0) { xSpd = 0; }
                    }
                }

                if (RY > 0) {
                    ySpd += abs(ryRatio) * accelMult;
                }
                else if (RY < 0) {
                    ySpd -= abs(ryRatio) * accelMult;
                }
                else {
                    if (ySpd > 0) {
                        ySpd -= brakeSpd;
                        if (ySpd < 0) { ySpd = 0; }
                    }
                    if (ySpd < 0) {
                        ySpd += brakeSpd;
                        if (ySpd > 0) { ySpd = 0; }
                    }
                }

                if (abs(xSpd) > maxSpd) { xSpd = maxSpd * rxSign; }
                if (abs(ySpd) > maxSpd) { ySpd = maxSpd * rySign; }


                if (isMag) {
                    // Process window messages
                    bRet = PeekMessage(&msg, hwndHost, 0, 0, PM_REMOVE);
                    if (bRet == -1)
                    {
                        // handle the error and possibly exit
                    }
                    else
                    {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }


                    // Update positions
                    if (!LT) {
                        if (x + LENS_WIDTH / 2 > (float)GetSystemMetrics(SM_CXSCREEN) + LENS_WIDTH / 2) { x = (float)GetSystemMetrics(SM_CXSCREEN) + LENS_WIDTH / 2 - LENS_WIDTH / 2 - 1; }
                        if (x < 0) { x = 0; }
                        if (y + LENS_HEIGHT / 2 > (float)GetSystemMetrics(SM_CYSCREEN) + LENS_HEIGHT / 2) { y = (float)GetSystemMetrics(SM_CYSCREEN) + LENS_HEIGHT / 2 - LENS_HEIGHT / 2 - 1; }
                        if (y < 0) { y = 0; }

                        x += xSpd;
                        y += ySpd;
                    }
                }

                // Toggle magnifier
                if ((leftAnalogPressed && rightAnalogPressed && toggleTimer > 400)
                || (GB && toggleTimer > 400)) {
                    if (!isMag) {
                        isMag = true;
                        toggleTimer = 0;
                        // Create Window
                        MagInitialize();
                        SetupMagnifier(hInstance);
                        ShowWindow(hwndHost, nCmdShow);
                    }
                    else {
                        MagUninitialize();
                        DestroyWindow(hwndHost);
                        isMag = false;
                        toggleTimer = 0;
                    }
                }

                // Holding up on both sticks for 5 secs, quits
                if (rySign < 0 && lySign < 0) {
                    quitTimer += timerInterval;
                }
                else {
                    quitTimer = 0;
                }

                if (quitTimer > 2000) {
                    quit = true;
                } 

            }
            else
            {
                // Controller is not connected
            }
        }


        UpdateMagWindow((int)x, (int)y);
        Sleep(timerInterval);
        toggleTimer += timerInterval;
    }

    //KillTimer(NULL, timerId);
    MagUninitialize();
    DestroyWindow(hwndHost);
    //return (int) msg.wParam;
    return 0;
}

//
// FUNCTION: HostWndProc()
//
// PURPOSE: Window procedure for the window that hosts the magnifier control.
//
LRESULT CALLBACK HostWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            if (isFullScreen)
            {
                //GoPartialScreen();
            }
        }
        break;

    case WM_SYSCOMMAND:
        if (GET_SC_WPARAM(wParam) == SC_MAXIMIZE)
        {
            //GoFullScreen();
        }
        else
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        if (hwndMag != NULL)
        {
            GetClientRect(hWnd, &magWindowRect);
            // Resize the control to fill the window.
            SetWindowPos(hwndMag, NULL,
                magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom, 0);
        }
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

//
//  FUNCTION: RegisterHostWindowClass()
//
//  PURPOSE: Registers the window class for the window that contains the magnification control.
//
ATOM RegisterHostWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = {};

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = HostWndProc;
    wcex.hInstance = hInstance;
    wcex.hbrBackground = (HBRUSH)(1 + COLOR_BTNFACE);
    wcex.lpszClassName = WindowClassName;

    return RegisterClassEx(&wcex);
}

//
// FUNCTION: SetupMagnifier
//
// PURPOSE: Creates the windows and initializes magnification.
//
BOOL SetupMagnifier(HINSTANCE hinst)
{
    // Set bounds of host window according to screen size.
    hostWindowRect.top = 0;
    hostWindowRect.bottom = GetSystemMetrics(SM_CYSCREEN) / 4;  // top quarter of screen
    hostWindowRect.left = 0;
    hostWindowRect.right = GetSystemMetrics(SM_CXSCREEN);

    // Create the host window.
    RegisterHostWindowClass(hinst);
    hwndHost = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        WindowClassName, WindowTitle,
        RESTOREDWINDOWSTYLES,
        0, 0, hostWindowRect.right, hostWindowRect.bottom, NULL, NULL, hInst, NULL);
    if (!hwndHost)
    {
        return FALSE; //
    }

    // Make the window opaque.
    SetLayeredWindowAttributes(hwndHost, 0, 255, LWA_ALPHA);

    // Create a magnifier control that fills the client area.
    GetClientRect(hwndHost, &magWindowRect);
    hwndMag = CreateWindow(WC_MAGNIFIER, TEXT("MagnifierWindow"),
        WS_CHILD | WS_VISIBLE,
        magWindowRect.left, magWindowRect.top, magWindowRect.right, magWindowRect.bottom, hwndHost, NULL, hInst, NULL);
    if (!hwndMag)
    {
        return FALSE;
    }

    SetWindowLong(hwndHost, GWL_STYLE, 0);

    // Set the magnification factor.
    MAGTRANSFORM matrix;
    memset(&matrix, 0, sizeof(matrix));
    matrix.v[0][0] = MAGFACTOR;
    matrix.v[1][1] = MAGFACTOR;
    matrix.v[2][2] = 1.0f;

    BOOL ret = MagSetWindowTransform(hwndMag, &matrix);

    return ret;
}



// Description: 
//   Called by a timer to update the contents of the magnifier window and to set
//   the position of the host window. 
// Constants and global variables:
//   hwndHost - Handle of the host window.
//   hwndMag - Handle of the magnifier window.
//   LENS_WIDTH - Width of the magnifier window.
//   LENS_HEIGHT - Height of the magnifier window.
//   MAGFACTOR - The magnifica      tion factor.
//
void UpdateMagWindow(_In_ int x, _In_ int y)
{
    // Calculate a source rectangle that is centered at the mouse coordinates. 
    // Size the rectangle so that it fits into the magnifier window (the lens).
    RECT sourceRect;
    int borderWidth = GetSystemMetrics(SM_CXFIXEDFRAME);
    int captionHeight = GetSystemMetrics(SM_CYCAPTION);
    sourceRect.left = (x - (int)((LENS_WIDTH / 2) / MAGFACTOR)) + (int)(borderWidth / MAGFACTOR);
    sourceRect.top = (y - (int)((LENS_HEIGHT / 2) / MAGFACTOR)) + (int)(captionHeight / MAGFACTOR) + (int)(borderWidth / MAGFACTOR);
    sourceRect.right = LENS_WIDTH; //LENS_WIDTH;
    sourceRect.bottom = LENS_HEIGHT; //LENS_HEIGHT;

    // Pass the source rectangle to the magnifier control.
    MagSetWindowSource(hwndMag, sourceRect);

    // Move the host window so that the origin of the client area lines up
    // with the origin of the magnified source rectangle.
    MoveWindow(hwndHost,
        x - LENS_WIDTH / 2,
        y - LENS_HEIGHT / 2,
        LENS_WIDTH,
        LENS_HEIGHT,
        FALSE);

    // Force the magnifier control to redraw itself.
    InvalidateRect(hwndMag, NULL, TRUE);

    return;
}


float sign(_In_ float n) {
    if (n == 0) {
        return 0;
    }
    if (n > 0) {
        return 1;
    }
    else {
        return -1;
    }
}

BOOL FileExists(LPCTSTR szPath)
{
    DWORD dwAttrib = GetFileAttributes(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}