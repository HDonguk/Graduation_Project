#include "stdafx.h"
#include "LoginUI.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

LoginUI::LoginUI() {
    // Common Controls 초기화
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
}

LoginUI::~LoginUI() {
    // 컨트롤들은 부모 윈도우가 파괴될 때 자동으로 정리됨
}

void LoginUI::Initialize(HWND hwnd, HINSTANCE hInstance) {
    m_hwnd = hwnd;
    CreateControls(hInstance);
    UpdateLayout();
}

void LoginUI::CreateControls(HINSTANCE hInstance) {
    // 사용자명 입력
    m_editUsername = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"Player1",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0, m_hwnd, (HMENU)1001, hInstance, nullptr
    );

    // 서버 IP 입력
    m_editServerIP = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"127.0.0.1",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0, m_hwnd, (HMENU)1002, hInstance, nullptr
    );

    // 서버 포트 입력
    m_editServerPort = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"5000",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0, m_hwnd, (HMENU)1003, hInstance, nullptr
    );

    // 연결 버튼
    m_buttonConnect = CreateWindowEx(
        0, L"BUTTON", L"Connect to Server",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, m_hwnd, (HMENU)1004, hInstance, nullptr
    );

    // 상태 표시
    m_staticStatus = CreateWindowEx(
        0, L"STATIC", L"Enter your username and server information",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 0, 0, m_hwnd, (HMENU)1005, hInstance, nullptr
    );

    // 폰트 설정
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(m_editUsername, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editServerIP, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editServerPort, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_buttonConnect, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_staticStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void LoginUI::UpdateLayout() {
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    int centerX = width / 2;
    int startY = height / 3;
    int controlHeight = 25;
    int spacing = 10;

    // 사용자명 라벨
    SetWindowPos(CreateWindowEx(0, L"STATIC", L"Username:", WS_CHILD | WS_VISIBLE,
        centerX - 150, startY, 100, controlHeight, m_hwnd, nullptr, nullptr, nullptr),
        nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 사용자명 입력
    SetWindowPos(m_editUsername, nullptr, centerX - 50, startY, 200, controlHeight, SWP_NOZORDER);

    // 서버 IP 라벨
    SetWindowPos(CreateWindowEx(0, L"STATIC", L"Server IP:", WS_CHILD | WS_VISIBLE,
        centerX - 150, startY + controlHeight + spacing, 100, controlHeight, m_hwnd, nullptr, nullptr, nullptr),
        nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 서버 IP 입력
    SetWindowPos(m_editServerIP, nullptr, centerX - 50, startY + controlHeight + spacing, 200, controlHeight, SWP_NOZORDER);

    // 서버 포트 라벨
    SetWindowPos(CreateWindowEx(0, L"STATIC", L"Port:", WS_CHILD | WS_VISIBLE,
        centerX - 150, startY + (controlHeight + spacing) * 2, 100, controlHeight, m_hwnd, nullptr, nullptr, nullptr),
        nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 서버 포트 입력
    SetWindowPos(m_editServerPort, nullptr, centerX - 50, startY + (controlHeight + spacing) * 2, 200, controlHeight, SWP_NOZORDER);

    // 연결 버튼
    SetWindowPos(m_buttonConnect, nullptr, centerX - 50, startY + (controlHeight + spacing) * 3, 200, controlHeight, SWP_NOZORDER);

    // 상태 표시
    SetWindowPos(m_staticStatus, nullptr, centerX - 200, startY + (controlHeight + spacing) * 4, 400, controlHeight * 2, SWP_NOZORDER);
}

void LoginUI::ShowControls(bool show) {
    ShowWindow(m_editUsername, show ? SW_SHOW : SW_HIDE);
    ShowWindow(m_editServerIP, show ? SW_SHOW : SW_HIDE);
    ShowWindow(m_editServerPort, show ? SW_SHOW : SW_HIDE);
    ShowWindow(m_buttonConnect, show ? SW_SHOW : SW_HIDE);
    ShowWindow(m_staticStatus, SW_SHOW); // 상태는 항상 표시
}

void LoginUI::Render() {
    // Windows 컨트롤은 자동으로 렌더링되므로 추가 작업 불필요
}

bool LoginUI::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1004 && HIWORD(wParam) == BN_CLICKED) {
                OnConnectButton();
                return true;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                OnConnectButton();
                return true;
            }
            break;
        case WM_SIZE:
            UpdateLayout();
            return true;
    }
    return false;
}

void LoginUI::SetState(UIState state) {
    m_currentState = state;
    
    switch (state) {
        case UIState::LOGIN_SCREEN:
            ShowControls(true);
            SetWindowText(m_staticStatus, L"Enter your username and server information");
            SetWindowText(m_buttonConnect, L"Connect to Server");
            EnableWindow(m_buttonConnect, TRUE);
            break;
            
        case UIState::CONNECTING:
            ShowControls(false);
            SetWindowText(m_staticStatus, L"Connecting to server...");
            SetWindowText(m_buttonConnect, L"Connecting...");
            EnableWindow(m_buttonConnect, FALSE);
            break;
            
        case UIState::CONNECTED:
            ShowControls(false);
            SetWindowText(m_staticStatus, L"Connected successfully! Starting game...");
            break;
            
        case UIState::ERROR_STATE:
            ShowControls(true);
            SetWindowText(m_staticStatus, m_errorMessage.c_str());
            SetWindowText(m_buttonConnect, L"Retry Connection");
            EnableWindow(m_buttonConnect, TRUE);
            break;
    }
}

void LoginUI::SetErrorMessage(const std::wstring& message) {
    m_errorMessage = message;
    if (m_currentState == UIState::ERROR_STATE) {
        SetWindowText(m_staticStatus, m_errorMessage.c_str());
    }
}

void LoginUI::SetLoginCallback(std::function<void(const std::wstring&, const std::wstring&)> callback) {
    m_loginCallback = callback;
}

void LoginUI::SetConnectCallback(std::function<void(const std::wstring&, int)> callback) {
    m_connectCallback = callback;
}

void LoginUI::OnConnectButton() {
    if (m_currentState == UIState::CONNECTING) {
        return; // 연결 중에는 버튼 비활성화
    }

    // 입력값 가져오기
    wchar_t username[256] = {0};
    wchar_t serverIP[256] = {0};
    wchar_t serverPort[16] = {0};

    GetWindowText(m_editUsername, username, 256);
    GetWindowText(m_editServerIP, serverIP, 256);
    GetWindowText(m_editServerPort, serverPort, 16);

    // 입력 검증
    if (wcslen(username) == 0) {
        SetErrorMessage(L"Please enter a username");
        SetState(UIState::ERROR_STATE);
        return;
    }

    if (wcslen(serverIP) == 0) {
        SetErrorMessage(L"Please enter server IP");
        SetState(UIState::ERROR_STATE);
        return;
    }

    int port = _wtoi(serverPort);
    if (port <= 0 || port > 65535) {
        SetErrorMessage(L"Please enter a valid port number (1-65535)");
        SetState(UIState::ERROR_STATE);
        return;
    }

    // 로그인 콜백 호출
    if (m_loginCallback) {
        m_loginCallback(username, username); // 사용자명을 닉네임으로도 사용
    }

    // 연결 콜백 호출
    if (m_connectCallback) {
        m_connectCallback(serverIP, port);
    }

    SetState(UIState::CONNECTING);
}

void LoginUI::OnKeyDown(WPARAM wParam) {
    if (wParam == VK_RETURN) {
        OnConnectButton();
    }
} 