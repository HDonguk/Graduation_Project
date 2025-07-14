#pragma once
#include "stdafx.h"

class Framework;

class Win32Application
{
public:
    Win32Application() = default;
    Win32Application(UINT width, UINT height, std::wstring name);

    void CreateWnd(Framework* framework, HINSTANCE hInstance);

    void SetCustomWindowText(LPCWSTR text);
    HWND GetHwnd() { return m_hwnd; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    bool GetWindowVisible() { return m_windowVisible; }
    void SetWindowVisible(bool visible) { m_windowVisible = visible; }
    void OnResize(UINT width, UINT height);
protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd;

    // Window title.
    std::wstring m_title;

    // Window dimensions.
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    bool m_windowVisible;
};
