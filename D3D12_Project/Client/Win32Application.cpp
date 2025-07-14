//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "stdafx.h"
#include "Win32Application.h"
#include "Framework.h"
#include <WindowsX.h>

Win32Application::Win32Application(UINT width, UINT height, std::wstring name) : 
    m_hwnd{ nullptr },
    m_width{ width },
    m_height{ height },
    m_title{ name },
    m_windowVisible{ true }
{
    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

void Win32Application::CreateWnd(Framework* framework, HINSTANCE hInstance)
{
    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"MyGameClass";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left, // AdjustWindowRect() �Լ��� ���ؼ� ũ�Ⱑ �����Ʊ� ����
        windowRect.bottom - windowRect.top, // AdjustWindowRect() �Լ��� ���ؼ� ũ�Ⱑ �����Ʊ� ����
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        framework);
}

// Helper function for setting the window's title text.
void Win32Application::SetCustomWindowText(LPCWSTR text)
{
    std::wstring windowText = m_title + L" : " + text;
    SetWindowText(m_hwnd, windowText.c_str());
}

void Win32Application::OnResize(UINT width, UINT height)
{
    m_width = width;
    m_height = height;
    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

// Main message handler for the sample.
LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Framework* pSample = reinterpret_cast<Framework*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
        {
            // Save the DXSample* passed in to CreateWindow.
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;

    case WM_KEYDOWN:
        if (pSample)
        {
            pSample->OnKeyDown(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pSample)
        {
            pSample->OnKeyUp(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_COMMAND:
        if (pSample)
        {
            // LoginUI로 메시지 전달
            if (pSample->IsInLoginScreen()) {
                if (pSample->GetLoginUI().HandleMessage(message, wParam, lParam)) {
                    return 0;
                }
            }
        }
        return 0;

    case WM_SIZE:
        if (pSample)
        {
            // LoginUI로 메시지 전달
            if (pSample->IsInLoginScreen()) {
                if (pSample->GetLoginUI().HandleMessage(message, wParam, lParam)) {
                    return 0;
                }
            }
            
            RECT clientRect{};
            GetClientRect(hWnd, &clientRect);
            pSample->OnResize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (pSample)
        {
            pSample->GetScene(pSample->GetCurrentSceneName()).GetObj<CameraObject>(L"CameraObject").OnMouseInput(
                wParam, pSample->GetWin32App().GetHwnd());

        }
        return 0;

    case WM_MOVE:
        if (pSample) 
        {
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
