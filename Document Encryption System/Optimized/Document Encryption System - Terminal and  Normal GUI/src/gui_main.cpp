#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x600
#endif
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include "encryptor.hpp"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

HWND hInputFile, hOutputFolder, hPassword, hStatus;
HINSTANCE hInst;
bool inputError = false;
bool outputError = false;
int statusColor = 0; // 0: Default, 1: Red, 2: Green, 3: Yellow
bool isProgrammaticChange = false;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void BrowseFile(HWND hwnd, HWND target);
void BrowseFolder(HWND hwnd, HWND target);
void ExecuteAction(HWND hwnd, bool encrypt);
void ValidatePath(HWND hwnd, HWND hEdit, bool isInput, bool clearOnFailure, bool isBackground);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    const char CLASS_NAME[] = "CryptonWndClass";
    WNDCLASS wc = {}; wc.lpfnWndProc = WindowProc; wc.hInstance = hInstance; wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "CRYPTON - Secure Document Encryption", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 500, 420, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;
    ShowWindow(hwnd, nCmdShow);
    SetTimer(hwnd, 1, 500, NULL);
    MSG msg = {}; while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        HFONT hFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        int y = 20;
        HWND hTitle = CreateWindow("STATIC", "CRYPTON SECURITY ENGINE", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, y, 440, 20, hwnd, NULL, hInst, NULL);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)hFont, TRUE); y += 40;
        CreateWindow("STATIC", "Source File:", WS_VISIBLE | WS_CHILD, 20, y, 440, 20, hwnd, NULL, hInst, NULL); y += 20;
        hInputFile = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 20, y, 360, 25, hwnd, (HMENU)501, hInst, NULL);
        SendMessage(hInputFile, WM_SETFONT, (WPARAM)hFont, TRUE);
        HWND btnInput = CreateWindow("BUTTON", "Browse", WS_VISIBLE | WS_CHILD, 390, y, 70, 25, hwnd, (HMENU)101, hInst, NULL);
        SendMessage(btnInput, WM_SETFONT, (WPARAM)hFont, TRUE); y += 40;
        CreateWindow("STATIC", "Destination Folder:", WS_VISIBLE | WS_CHILD, 20, y, 440, 20, hwnd, NULL, hInst, NULL); y += 20;
        hOutputFolder = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 20, y, 360, 25, hwnd, (HMENU)502, hInst, NULL);
        SendMessage(hOutputFolder, WM_SETFONT, (WPARAM)hFont, TRUE);
        HWND btnOutput = CreateWindow("BUTTON", "Browse", WS_VISIBLE | WS_CHILD, 390, y, 70, 25, hwnd, (HMENU)102, hInst, NULL);
        SendMessage(btnOutput, WM_SETFONT, (WPARAM)hFont, TRUE); y += 40;
        CreateWindow("STATIC", "Security Password:", WS_VISIBLE | WS_CHILD, 20, y, 440, 20, hwnd, NULL, hInst, NULL); y += 20;
        hPassword = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_PASSWORD | ES_AUTOHSCROLL, 20, y, 440, 25, hwnd, (HMENU)503, hInst, NULL);
        SendMessage(hPassword, WM_SETFONT, (WPARAM)hFont, TRUE); y += 50;
        HWND btnEncrypt = CreateWindow("BUTTON", "ENCRYPT", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 20, y, 220, 40, hwnd, (HMENU)201, hInst, NULL);
        SendMessage(btnEncrypt, WM_SETFONT, (WPARAM)hFont, TRUE);
        HWND btnDecrypt = CreateWindow("BUTTON", "DECRYPT", WS_VISIBLE | WS_CHILD, 250, y, 210, 40, hwnd, (HMENU)202, hInst, NULL);
        SendMessage(btnDecrypt, WM_SETFONT, (WPARAM)hFont, TRUE); y += 60;
        hStatus = CreateWindow("STATIC", "Ready for operation.", WS_VISIBLE | WS_CHILD | SS_CENTER, 20, y, 440, 20, hwnd, NULL, hInst, NULL);
        SendMessage(hStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam), wmCode = HIWORD(wParam);
        if (wmCode == EN_CHANGE) {
            if (wmId == 501 && inputError) { inputError = false; if (!outputError) { statusColor = 0; SetWindowText(hStatus, "Ready for operation."); } }
            if (wmId == 502 && outputError) { outputError = false; if (!inputError) { statusColor = 0; SetWindowText(hStatus, "Ready for operation."); } }
            if (wmId == 503 && statusColor == 1 && !isProgrammaticChange) { if (!inputError && !outputError) { statusColor = 0; SetWindowText(hStatus, "Ready for operation."); } }
            InvalidateRect(hStatus, NULL, TRUE);
        }
        if (wmCode == EN_KILLFOCUS) {
            if (wmId == 501) ValidatePath(hwnd, hInputFile, true, true, false);
            else if (wmId == 502) ValidatePath(hwnd, hOutputFolder, false, true, false);
        }
        if (wmId == 101) BrowseFile(hwnd, hInputFile);
        else if (wmId == 102) BrowseFolder(hwnd, hOutputFolder);
        else if (wmId == 201) ExecuteAction(hwnd, true);
        else if (wmId == 202) ExecuteAction(hwnd, false);
        break;
    }
    case WM_TIMER: {
        if (wParam == 1) { 
            ValidatePath(hwnd, hInputFile, true, false, true); 
            ValidatePath(hwnd, hOutputFolder, false, false, true); 
        } else if (wParam == 2) {
            KillTimer(hwnd, 2); inputError = false; outputError = false; statusColor = 0; 
            SetWindowText(hStatus, "Ready for operation."); InvalidateRect(hStatus, NULL, TRUE);
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam; SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
        if ((HWND)lParam == hStatus) {
            if (inputError || outputError || statusColor == 1) SetTextColor(hdc, RGB(220, 20, 60)); // RED
            else if (statusColor == 2) SetTextColor(hdc, RGB(34, 139, 34)); // GREEN
            else if (statusColor == 3) SetTextColor(hdc, RGB(218, 165, 32)); // YELLOW
            else SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        } else SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_DESTROY: KillTimer(hwnd, 1); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ValidatePath(HWND hwnd, HWND hEdit, bool isInput, bool clearOnFailure, bool isBackground) {
    char path[MAX_PATH]; GetWindowText(hEdit, path, MAX_PATH); std::string p = path; if (p.empty()) return;
    if (p.front() == '"' && p.back() == '"') { p = p.substr(1, p.length() - 2); SetWindowText(hEdit, p.c_str()); }
    DWORD attr = GetFileAttributesA(p.c_str()); bool exists = (attr != INVALID_FILE_ATTRIBUTES);
    if (isInput) {
        if (!exists || (attr & FILE_ATTRIBUTE_DIRECTORY)) { inputError = true; statusColor = 1; SetWindowText(hStatus, "Error: Source file missing!"); if (clearOnFailure) SetWindowText(hEdit, ""); }
        else if (!isBackground) { if (inputError) { inputError = false; if (!outputError) { statusColor = 0; SetWindowText(hStatus, "Ready for operation."); } } }
    } else {
        bool isDir = exists && (attr & FILE_ATTRIBUTE_DIRECTORY);
        if (!isDir) { outputError = true; statusColor = 1; SetWindowText(hStatus, "Error: Destination folder is missing!"); if (clearOnFailure) SetWindowText(hEdit, ""); }
        else if (!isBackground) { if (outputError) { outputError = false; if (!inputError) { statusColor = 0; SetWindowText(hStatus, "Ready for operation."); } } }
    }
    InvalidateRect(hStatus, NULL, TRUE);
}

void BrowseFile(HWND hwnd, HWND target) {
    char szFile[MAX_PATH] = { 0 }; OPENFILENAME ofn = { 0 }; ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFilter = "All Files\0*.*\0";
    if (GetOpenFileName(&ofn)) { SetWindowText(target, szFile); ValidatePath(hwnd, target, true, true, false); }
}

void BrowseFolder(HWND hwnd, HWND target) {
    BROWSEINFO bi = { 0 }; bi.hwndOwner = hwnd; bi.lpszTitle = "Select Folder"; bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl) { char path[MAX_PATH]; SHGetPathFromIDList(pidl, path); SetWindowText(target, path); ValidatePath(hwnd, target, false, true, false); CoTaskMemFree(pidl); }
}

void ExecuteAction(HWND hwnd, bool encrypt) {
    char input[MAX_PATH], output[MAX_PATH], pass[256];
    GetWindowText(hInputFile, input, MAX_PATH); GetWindowText(hOutputFolder, output, MAX_PATH); GetWindowText(hPassword, pass, 256);
    std::string iS = input, oS = output, pS = pass;
    if (iS.empty()) { inputError = true; statusColor = 1; SetWindowText(hStatus, "Error: Missing Source File!"); InvalidateRect(hStatus, NULL, TRUE); return; }
    if (oS.empty()) { outputError = true; statusColor = 1; SetWindowText(hStatus, "Error: Missing Destination Folder!"); InvalidateRect(hStatus, NULL, TRUE); return; }
    if (pS.empty()) { statusColor = 1; SetWindowText(hStatus, "Error: Missing Security Password!"); InvalidateRect(hStatus, NULL, TRUE); return; }
    DWORD iA = GetFileAttributesA(iS.c_str());
    if (iA == INVALID_FILE_ATTRIBUTES || (iA & FILE_ATTRIBUTE_DIRECTORY)) { inputError = true; statusColor = 1; SetWindowText(hStatus, "Error: Source missing!"); InvalidateRect(hStatus, NULL, TRUE); return; }
    DWORD oA = GetFileAttributesA(oS.c_str());
    if (oA == INVALID_FILE_ATTRIBUTES || !(oA & FILE_ATTRIBUTE_DIRECTORY)) { outputError = true; statusColor = 1; SetWindowText(hStatus, "Error: Dest missing!"); InvalidateRect(hStatus, NULL, TRUE); return; }
    if (!encrypt && (iS.length() < 4 || iS.substr(iS.length() - 4) != ".enc")) {
        inputError = true; statusColor = 1; SetWindowText(hStatus, "Error: Selection must be an .enc file!");
        SetTimer(hwnd, 2, 2500, NULL); InvalidateRect(hStatus, NULL, TRUE); return;
    }
    inputError = false; outputError = false; statusColor = 3; // Yellow
    SetWindowText(hStatus, encrypt ? "Encrypting documents..." : "Decrypting documents..."); InvalidateRect(hStatus, NULL, TRUE); UpdateWindow(hStatus);
    if (encrypt) {
        size_t pos = iS.find_last_of("\\/"); std::string fn = (pos == std::string::npos) ? iS : iS.substr(pos + 1);
        size_t dot = fn.find_last_of("."); std::string bn = (dot == std::string::npos) ? fn : fn.substr(0, dot);
        std::string ext = (dot == std::string::npos) ? "" : fn.substr(dot);
        std::string o = oS; if (o.back() != '\\' && o.back() != '/') o += "\\"; o += bn + ".enc";
        if (Encryptor::encryptFile(iS, o, pS, ext)) { 
            statusColor = 2; SetWindowText(hStatus, "Encryption Successful"); 
            SetTimer(hwnd, 2, 2500, NULL); MessageBox(hwnd, "Encrypted Successfully", "Done", MB_OK | MB_ICONINFORMATION);
        } else { statusColor = 1; SetWindowText(hStatus, "Error: Failed!"); }
    } else {
        std::string o = oS; if (o.back() != '\\' && o.back() != '/') o += "\\";
        if (Encryptor::decryptFile(iS, o, pS)) { 
            statusColor = 2; SetWindowText(hStatus, "Decryption Successful"); 
            SetTimer(hwnd, 2, 2500, NULL); MessageBox(hwnd, "Decrypted Successfully", "Done", MB_OK | MB_ICONINFORMATION);
        } else { statusColor = 1; SetWindowText(hStatus, "Error : Wrong Password !"); }
    }
    isProgrammaticChange = true;
    SetWindowText(hPassword, "");
    isProgrammaticChange = false;
    InvalidateRect(hStatus, NULL, TRUE);
}
