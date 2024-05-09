// BVT_GUI.cpp : Определяет точку входа для приложения.
//

#include "framework.h"
#include "BVT_GUI.h"

// 
UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;

//
// {B8407678-8EAB-4FF6-A637-9403FABDC3D0}
static const GUID iconGuid =
{ 0xb8407678, 0x8eab, 0x4ff6, { 0xa6, 0x37, 0x94, 0x3, 0xfa, 0xbd, 0xc3, 0xd0 } };


#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна


// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL AddNotificationIcon(HWND hwnd);
BOOL powerPipe(HWND hwnd);



bool Read(HANDLE handle, uint8_t* data, uint64_t length, DWORD& bytesRead)
{
    bytesRead = 0;
    BOOL fSuccess = ReadFile(
        handle,
        data,
        length,
        &bytesRead,
        NULL);
    if (!fSuccess || bytesRead == 0)
    {
        return false;
    }
    return true;
}

bool Write(HANDLE handle, uint8_t* data, uint64_t length)
{
    DWORD cbWritten = 0;
    BOOL fSuccess = WriteFile(
        handle,
        data,
        length,
        &cbWritten,
        NULL);
    if (!fSuccess || length != cbWritten)
    {
        return false;
    }
    return true;
}
std::vector<uint8_t*> LoadSignatureFromFile(const std::wstring& signatureFilePath) {
    std::ifstream file(signatureFilePath, std::ios::binary);

    std::vector<uint8_t*> data;

    const size_t block_size = 8;
    uint8_t buffer[block_size];

    while (file.read(reinterpret_cast<char*>(buffer), block_size)) {
        uint8_t* block_data = new uint8_t[block_size];
        std::copy(buffer, buffer + block_size, block_data);
        data.push_back(block_data);
    }

    return data;
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Разместите код здесь.

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BVTGUI, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Выполнить инициализацию приложения:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BVTGUI));

    MSG msg;

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}



//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BVTGUI));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_BVTGUI);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

HANDLE ConnectToServerPipe(const std::wstring& name, uint32_t timeout)
{
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    while (true)
    {
        hPipe = CreateFileW(
            reinterpret_cast<LPCWSTR>(name.c_str()),
            GENERIC_READ |
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (hPipe != INVALID_HANDLE_VALUE)
        {
            break;
        }
        DWORD error = GetLastError();
        if (error != ERROR_PIPE_BUSY)
        {
            return INVALID_HANDLE_VALUE;
        }
        if (!WaitNamedPipe(reinterpret_cast<LPCWSTR>(name.c_str()), timeout))
        {
            return INVALID_HANDLE_VALUE;
        }
    }
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    BOOL fSuccess = SetNamedPipeHandleState(
        hPipe,
        &dwMode,
        NULL,
        NULL);
    if (!fSuccess)
    {
        return INVALID_HANDLE_VALUE;
    }
    return hPipe;
}

HANDLE OpenFile(const std::wstring& name) {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    hFile = CreateFileW(
            reinterpret_cast<LPCWSTR>(name.c_str()),
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

    return hFile;
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
//   КОММЕНТАРИИ:
//
//        В этой функции маркер экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }
    // проверка task bar
    AddNotificationIcon(hWnd);

  

    return TRUE;
}



BOOL powerPipe(HWND hWnd, const std::wstring& filename)
{
    std::vector<uint8_t*> signature = LoadSignatureFromFile(filename);
    std::thread clientTread([hWnd, signature]() {
        
        DWORD sessionId;
        ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
        std::wstring path = std::format(L"\\\\.\\pipe\\SimpleService_{}", sessionId);
        HANDLE pipe = ConnectToServerPipe(path, 0);
        
        BOOL cleanFlag = true;
        uint8_t bufFlag[8] = "cleansg";
        for (uint8_t* buff : signature) {
            DWORD length = 0;
            Write(pipe, buff, 8);
            uint8_t buff2[8] = {};
            Read(pipe, buff2, 8, length);
            if (memcmp(buff2, bufFlag, 8) != 0) {
                cleanFlag = false;
                MessageBoxA(hWnd, (char*)buff2, "WARNING!!!", MB_OK | MB_ICONINFORMATION);
                break;
            }
        }

        if (cleanFlag) MessageBoxA(hWnd, "Clean", "Info", MB_OK | MB_ICONINFORMATION);
        });
    clientTread.detach();
    return true;
}

BOOL AddNotificationIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hwnd;
    // add the icon, setting the icon, tooltip, and callback message.
    // the icon will be identified with the GUID
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
    nid.guidItem = iconGuid;
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BVTGUI));
    LoadString(hInst, IDS_TOOLTIP, nid.szTip, ARRAYSIZE(nid.szTip));
    Shell_NotifyIcon(NIM_ADD, &nid);

    // NOTIFYICON_VERSION_4 is prefered
    nid.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

BOOL DeleteNotificationIcon()
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_GUID;
    nid.guidItem = iconGuid;
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowContextMenu(HWND hwnd, POINT pt)
{
    HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU1));
    if (hMenu)
    {
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        if (hSubMenu)
        {
            // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
            SetForegroundWindow(hwnd);

            // respect menu drop alignment
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
            {
                uFlags |= TPM_RIGHTALIGN;
            }
            else
            {
                uFlags |= TPM_LEFTALIGN;
            }

            TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
        }
        DestroyMenu(hMenu);
    }
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
//
//  WM_COMMAND  - обработать меню приложения
//  WM_PAINT    - Отрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND hwndEdit;
    static HWND hwndButton;

    static UINT s_uTaskbarRestart;
    switch (message)
    {
    case WM_CREATE:
        s_uTaskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));
        hwndEdit = CreateWindowEx(WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
            50, 50, 200, 20,
            hWnd,
            nullptr,
            nullptr,
            nullptr);

        hwndButton = CreateWindow(L"BUTTON",
            L"Проверить",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            260, 50, 100, 20,
            hWnd,
            (HMENU)button_id,
            nullptr,
            nullptr);
        break;
    case WMAPP_NOTIFYCALLBACK:
        switch (LOWORD(lParam))
        {
        case NIN_SELECT:
            ShowWindow(hWnd, SW_SHOW);
            UpdateWindow(hWnd);
            break;
        case WM_CONTEXTMENU:
        {
            POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
            ShowContextMenu(hWnd, pt);
        }
        break;
        }

        break;
    
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int textLength = GetWindowTextLength(hwndEdit);
        wchar_t* text = nullptr;
        delete[] text;
        text = new wchar_t[textLength + 1];
        GetWindowText(hwndEdit, text, textLength + 1);
        // Разобрать выбор в меню:
        switch (wmId)
        {
        case button_id:
            
            powerPipe(hWnd, text);
            break;
        case ID_CONTEXTMENU_SHOW_MAIN_WINDOW:
            ShowWindow(hWnd, SW_SHOW);
            UpdateWindow(hWnd);
            break;
        case ID_CONTEXTMENU_EXIT:
        case IDM_EXIT:
            if (IDYES == MessageBox(hWnd, L"Are you shure to close window?", L"WM_CONTEXTMENU", MB_ICONWARNING | MB_YESNO)) {
                DestroyWindow(hWnd);
            }
            break;
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Добавьте сюда любой код прорисовки, использующий HDC...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_CLOSE:
        //DestroyWindow(hWnd);
        ShowWindow(hWnd, SW_HIDE);
        UpdateWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        if (message == s_uTaskbarRestart)
            AddNotificationIcon(hWnd);
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
