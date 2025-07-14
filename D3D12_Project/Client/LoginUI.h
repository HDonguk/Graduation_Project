#pragma once
#include "stdafx.h"
#include <string>
#include <functional>

// UIState enum을 클래스 외부로 이동
enum class UIState {
    LOGIN_SCREEN,
    CONNECTING,
    CONNECTED,
    ERROR_STATE
};

class LoginUI {
public:
    LoginUI();
    ~LoginUI();

    void Initialize(HWND hwnd, HINSTANCE hInstance);
    void Render();
    bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void SetState(UIState state);
    void SetErrorMessage(const std::wstring& message);
    void SetLoginCallback(std::function<void(const std::wstring&, const std::wstring&)> callback);
    void SetConnectCallback(std::function<void(const std::wstring&, int)> callback);
    HWND GetHwnd() const { return m_hwnd; }
    HWND GetEditUsernameHwnd() const { return m_editUsername; }

private:
    HWND m_hwnd = nullptr;
    HWND m_editUsername = nullptr;
    HWND m_editServerIP = nullptr;
    HWND m_editServerPort = nullptr;
    HWND m_buttonConnect = nullptr;
    HWND m_staticStatus = nullptr;

    UIState m_currentState = UIState::LOGIN_SCREEN;
    std::wstring m_errorMessage;
    std::function<void(const std::wstring&, const std::wstring&)> m_loginCallback;
    std::function<void(const std::wstring&, int)> m_connectCallback;

    void CreateControls(HINSTANCE hInstance);
    void UpdateLayout();
    void ShowControls(bool show);
    void OnConnectButton();
    void OnKeyDown(WPARAM wParam);
}; 