// CRYPTON - Advanced Document Encryption System
// Dear ImGui + Win32 + OpenGL2
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commctrl.h>
#include <GL/gl.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl2.h"
#include "encryptor.hpp"

#include <string>
#include <cstring>
#include <cmath>
#include <cstdio>

// ---- Globals ----
static HWND g_hwnd = NULL;
static HDC g_hDC = NULL;
static HGLRC g_hRC = NULL;
static ImFont* g_fontBold = NULL;
static ImFont* g_fontLarge = NULL;
static ImFont* g_fontTitle = NULL;

static char g_inputFile[MAX_PATH] = "";
static char g_outputFolder[MAX_PATH] = "";
static char g_password[256] = "";
static bool g_showPassword = false;
static int g_statusType = 0; // 0=ready,1=error,2=success,3=processing
static char g_statusMsg[512] = "READY FOR SECURE OPERATIONS.";
static float g_statusTimer = 0.0f;
static float g_validateTimer = 0.0f;
static bool g_inputError = false;
static bool g_outputError = false;
static bool g_inputWasValid = false;
static bool g_outputWasValid = false;
static bool g_isProcessing = false;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ---- Theme ----
void SetupTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0; s.FrameRounding = 6; s.GrabRounding = 4;
    s.ScrollbarRounding = 6; s.FramePadding = ImVec2(12,8);
    s.ItemSpacing = ImVec2(10,10); s.WindowPadding = ImVec2(24,24);
    s.FrameBorderSize = 1; s.WindowBorderSize = 0;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.05f,0.05f,0.08f,0.85f); // More translucent for glass effect
    c[ImGuiCol_ChildBg]         = ImVec4(0.09f,0.09f,0.14f,1);
    c[ImGuiCol_Text]            = ImVec4(0.90f,0.92f,0.95f,1);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.45f,0.47f,0.53f,1);
    c[ImGuiCol_Border]          = ImVec4(0.18f,0.20f,0.28f,1);
    c[ImGuiCol_FrameBg]         = ImVec4(0.11f,0.12f,0.17f,1);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.14f,0.16f,0.22f,1);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.16f,0.18f,0.26f,1);
    c[ImGuiCol_Button]          = ImVec4(0.11f,0.13f,0.21f,1);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.16f,0.19f,0.32f,1);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.00f,0.55f,0.82f,1);
    c[ImGuiCol_Header]          = ImVec4(0.14f,0.16f,0.24f,1);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.18f,0.20f,0.30f,1);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.20f,0.24f,0.36f,1);
    c[ImGuiCol_Separator]       = ImVec4(0.18f,0.20f,0.28f,1);
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0.07f,0.07f,0.11f,1);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.20f,0.22f,0.30f,1);
    c[ImGuiCol_CheckMark]       = ImVec4(0.00f,0.82f,1.00f,1);
    c[ImGuiCol_TitleBg]         = ImVec4(0.05f,0.05f,0.08f,1);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.07f,0.07f,0.11f,1);
}

// ---- Helpers ----
bool BrowseFile(char* out) {
    char buf[MAX_PATH]={0}; OPENFILENAMEA ofn={0};
    ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=g_hwnd;
    ofn.lpstrFile=buf; ofn.nMaxFile=MAX_PATH;
    ofn.lpstrFilter="All Files\0*.*\0";
    if(GetOpenFileNameA(&ofn)){strcpy(out,buf);return true;}
    return false;
}

bool BrowseFolder(char* out) {
    BROWSEINFOA bi={0}; bi.hwndOwner=g_hwnd;
    bi.lpszTitle="Select Destination Folder";
    bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl=SHBrowseForFolderA(&bi);
    if(pidl){char p[MAX_PATH]; SHGetPathFromIDListA(pidl,p);
    strcpy(out,p); CoTaskMemFree(pidl); return true;}
    return false;
}

int PasswordStrength(const char* p) {
    int len=(int)strlen(p); if(len==0) return 0;
    int score=0; bool upper=false,lower=false,digit=false,special=false;
    for(int i=0;i<len;i++){
        if(p[i]>='A'&&p[i]<='Z') upper=true;
        else if(p[i]>='a'&&p[i]<='z') lower=true;
        else if(p[i]>='0'&&p[i]<='9') digit=true;
        else special=true;
    }
    if(len>=6) score++; if(len>=10) score++;
    if(upper) score++; if(lower) score++;
    if(digit) score++; if(special) score++;
    return score; // 0-6
}

void SetStatus(int type, const char* msg, float timer = -1.0f) {
    g_statusType=type; strcpy(g_statusMsg,msg);
    if(timer >= 0.0f) g_statusTimer = timer;
    else if(type == 1) g_statusTimer = 0; // Permanent errors by default
    else g_statusTimer = 3.5f;
}

// Strip surrounding quotes from pasted paths
void StripQuotes(char* path) {
    size_t len = strlen(path);
    if(len >= 2 && path[0] == '"' && path[len-1] == '"') {
        memmove(path, path+1, len-2);
        path[len-2] = '\0';
    }
}

// Background path validation - distinguishes between INVALID (typing) and MISSING (deleted)
void ValidatePathsBackground() {
    if(g_isProcessing) return;
    // Validate source file
    if(strlen(g_inputFile) > 0) {
        DWORD a = GetFileAttributesA(g_inputFile);
        bool bad = (a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_DIRECTORY));
        if(bad) {
            // Allow override if no error yet, OR if we are showing a temporary "ENTER" error from an action
            bool isEntering = (g_statusTimer > 0.0f && strstr(g_statusMsg, "ENTER SOURCE FILE"));
            if(!g_inputError || isEntering) {
                g_inputError = true;
                g_statusType = 1;
                if(g_inputWasValid) {
                    strcpy(g_statusMsg, "ERROR : SOURCE FILE IS MISSING");
                } else {
                    strcpy(g_statusMsg, "ERROR : INVALID SOURCE FILE !");
                    g_statusTimer = 0; // Make it permanent until corrected
                }
            }
        } else {
            g_inputWasValid = true; // Mark as having been valid
            // Only clear the error if no temporary timer is active
            if(g_inputError && g_statusTimer <= 0.0f) {
                g_inputError = false;
                if(!g_outputError) { g_statusType = 0; strcpy(g_statusMsg, "READY FOR SECURE OPERATIONS."); }
                else { 
                    g_statusType = 1;
                    if(g_outputWasValid) strcpy(g_statusMsg, "ERROR : DESTINATION FOLDER IS MISSING");
                    else strcpy(g_statusMsg, "ERROR : INVALID DESTINATION FOLDER !");
                }
            }
        }
    } else {
        g_inputWasValid = false; // Reset if cleared
        // Only clear if no action timer is running (keep "ENTER" message)
        if(g_inputError && g_statusTimer <= 0.0f) {
            g_inputError = false;
            if(!g_outputError) { g_statusType = 0; strcpy(g_statusMsg, "READY FOR SECURE OPERATIONS."); }
        }
    }

    // Validate destination folder
    if(strlen(g_outputFolder) > 0) {
        DWORD a = GetFileAttributesA(g_outputFolder);
        bool isDir = (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
        if(!isDir) {
            bool isEntering = (g_statusTimer > 0.0f && strstr(g_statusMsg, "ENTER DESTINATION FOLDER"));
            if(!g_outputError || isEntering) {
                g_outputError = true;
                if(!g_inputError) {
                    g_statusType = 1;
                    if(g_outputWasValid) {
                        strcpy(g_statusMsg, "ERROR : DESTINATION FOLDER IS MISSING");
                    } else {
                        strcpy(g_statusMsg, "ERROR : INVALID DESTINATION FOLDER !");
                        g_statusTimer = 0;
                    }
                }
            }
        } else {
            g_outputWasValid = true;
            // Only clear the error if no temporary timer is active
            if(g_outputError && g_statusTimer <= 0.0f) {
                g_outputError = false;
                if(!g_inputError) { g_statusType = 0; strcpy(g_statusMsg, "READY FOR SECURE OPERATIONS."); }
            }
        }
    } else {
        g_outputWasValid = false;
        if(g_outputError && g_statusTimer <= 0.0f) {
            g_outputError = false;
            if(!g_inputError) { g_statusType = 0; strcpy(g_statusMsg, "READY FOR SECURE OPERATIONS."); }
        }
    }
}

struct OperationData {
    std::string inputPath;
    std::string outputPath;
    std::string password;
    std::string extension;
    bool isEncrypt;
};

DWORD WINAPI OperationThread(LPVOID lpParam) {
    OperationData* data = (OperationData*)lpParam;
    
    if (data->isEncrypt) {
        if (Encryptor::encryptFile(data->inputPath, data->outputPath, data->password, data->extension)) {
            Sleep(1000); // Ensure processing message was seen
            SetStatus(2, "ENCRYPTION SUCCESSFUL");
            MessageBoxA(g_hwnd, "Encrypted Successfully", "Done", MB_OK | MB_ICONINFORMATION);
        } else {
            SetStatus(1, "ERROR : ENCRYPTION FAILED !", 1.5f);
        }
    } else {
        std::string out = data->outputPath;
        int result = Encryptor::decryptFile(data->inputPath, out, data->password);
        if (result == Encryptor::kDecryptOk) {
            Sleep(1000); // Ensure processing message was seen
            SetStatus(2, "DECRYPTION SUCCESSFUL");
            MessageBoxA(g_hwnd, "Decrypted Successfully", "Done", MB_OK | MB_ICONINFORMATION);
        } else if (result == Encryptor::kDecryptNotEncrypted) {
            SetStatus(1, "ERROR : FILE IS NOT A VALID ENCRYPTED FILE !", 2.5f);
        } else if (result == Encryptor::kDecryptWrongPassword) {
            SetStatus(1, "ERROR : WRONG PASSWORD !", 1.5f);
        } else if (result == Encryptor::kDecryptDataError) {
            SetStatus(1, "ERROR : FILE DATA IS CORRUPTED !", 2.0f);
        } else {
            SetStatus(1, "ERROR : DECRYPTION FAILED !", 1.5f);
        }
    }
    g_isProcessing = false;
    delete data;
    return 0;
}

void DoEncrypt() {
    if(g_isProcessing) return;
    StripQuotes(g_inputFile); StripQuotes(g_outputFolder);
    std::string iS = g_inputFile, oS = g_outputFolder, pS = g_password;
    if (iS.empty()) { g_inputError = true; SetStatus(1, "ERROR : ENTER SOURCE FILE", 1.5f); return; }
    if (oS.empty()) { g_outputError = true; SetStatus(1, "ERROR : ENTER DESTINATION FOLDER", 1.5f); return; }
    if (pS.empty()) { SetStatus(1, "ERROR : ENTER PASSWORD", 1.5f); return; }
    DWORD iA = GetFileAttributesA(iS.c_str());
    if (iA == INVALID_FILE_ATTRIBUTES || (iA & FILE_ATTRIBUTE_DIRECTORY)) { g_inputError = true; SetStatus(1, "ERROR : INVALID SOURCE FILE !"); return; }
    DWORD oA = GetFileAttributesA(oS.c_str());
    if (oA == INVALID_FILE_ATTRIBUTES || !(oA & FILE_ATTRIBUTE_DIRECTORY)) { g_outputError = true; SetStatus(1, "ERROR : INVALID OUTPUT FOLDER !"); return; }

    g_inputError = false; g_outputError = false;
    size_t pos = iS.find_last_of("\\/"); std::string fn = (pos == std::string::npos) ? iS : iS.substr(pos + 1);
    size_t dot = fn.find_last_of("."); std::string bn = (dot == std::string::npos) ? fn : fn.substr(0, dot);
    std::string ext = (dot == std::string::npos) ? "" : fn.substr(dot);
    std::string o = oS; if (o.back() != '\\' && o.back() != '/') o += "\\"; o += bn + ".enc";

    SetStatus(3, "ENCRYPTING FILES....", 999.0f);
    g_isProcessing = true;
    
    OperationData* data = new OperationData{iS, o, pS, ext, true};
    memset(g_password, 0, sizeof(g_password));
    CreateThread(NULL, 0, OperationThread, data, 0, NULL);
}

void DoDecrypt() {
    if(g_isProcessing) return;
    StripQuotes(g_inputFile); StripQuotes(g_outputFolder);
    std::string iS = g_inputFile, oS = g_outputFolder, pS = g_password;
    if (iS.empty()) { g_inputError = true; SetStatus(1, "ERROR : ENTER SOURCE FILE", 1.5f); return; }
    if (oS.empty()) { g_outputError = true; SetStatus(1, "ERROR : ENTER DESTINATION FOLDER", 1.5f); return; }
    if (pS.empty()) { SetStatus(1, "ERROR : ENTER PASSWORD", 1.5f); return; }
    DWORD iA = GetFileAttributesA(iS.c_str());
    if (iA == INVALID_FILE_ATTRIBUTES || (iA & FILE_ATTRIBUTE_DIRECTORY)) { g_inputError = true; SetStatus(1, "ERROR : INVALID SOURCE FILE !"); return; }
    DWORD oA = GetFileAttributesA(oS.c_str());
    if (oA == INVALID_FILE_ATTRIBUTES || !(oA & FILE_ATTRIBUTE_DIRECTORY)) { g_outputError = true; SetStatus(1, "ERROR : INVALID OUTPUT FOLDER !"); return; }
    if (iS.length() < 4 || iS.substr(iS.length() - 4) != ".enc") {
        g_inputError = true; SetStatus(1, "ERROR : STANDARD DECRYPTION REQUIRES SOURCE FILE WITH .enc EXTENSION", 2.5f); return;
    }

    g_inputError = false; g_outputError = false;
    std::string o = oS; if (o.back() != '\\' && o.back() != '/') o += "\\";

    SetStatus(3, "DECRYPTING FILES....", 999.0f);
    g_isProcessing = true;

    OperationData* data = new OperationData{iS, o, pS, "", false};
    memset(g_password, 0, sizeof(g_password));
    CreateThread(NULL, 0, OperationThread, data, 0, NULL);
}

// ---- Draw gradient rect helper ----
void DrawGradientRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c1, ImU32 c2) {
    dl->AddRectFilledMultiColor(a,b,c1,c2,c2,c1);
}

// ---- Get perimeter pos helper ----
ImVec2 GetPerimeterPos(ImVec2 wp, ImVec2 ws, float p, float inset_x, float inset_y) {
    float w = ws.x - 2.0f * inset_x;
    float h = ws.y - 2.0f * inset_y;
    float perimeter = 2.0f * (w + h);
    p = fmodf(p, perimeter);
    if (p < 0) p += perimeter;
    
    if (p < w) return ImVec2(wp.x + inset_x + p, wp.y + inset_y);
    p -= w;
    if (p < h) return ImVec2(wp.x + ws.x - inset_x, wp.y + inset_y + p);
    p -= h;
    if (p < w) return ImVec2(wp.x + ws.x - inset_x - p, wp.y + ws.y - inset_y);
    p -= w;
    return ImVec2(wp.x + inset_x, wp.y + ws.y - inset_y - p);
}

// ---- Draw Tron Effects helper ----
void DrawTronEffects(ImDrawList* dl, ImVec2 wp, ImVec2 ws, float t) {
    // 1. Wobbling circular gradient orbs in the background
    float cx = wp.x + ws.x * 0.5f;
    float cy = wp.y + ws.y * 0.5f;

    // --- Original 4 orbs ---
    // Orb 1: Cyan
    float orb1_x = cx + sinf(t * 2.5f) * (ws.x * 0.4f);
    float orb1_y = cy + cosf(t * 2.0f) * (ws.y * 0.35f);
    for (int i = 0; i < 40; i++) {
        float r = 1800.0f - i * 42.0f;
        float alpha = 0.018f * ((float)i / 40.0f);
        dl->AddCircleFilled(ImVec2(orb1_x, orb1_y), r, IM_COL32(0, 180, 255, (int)(255 * alpha)));
    }
    
    // Orb 2: Purple
    float orb2_x = cx + cosf(t * 2.2f) * (ws.x * 0.4f);
    float orb2_y = cy + sinf(t * 2.8f) * (ws.y * 0.35f);
    for (int i = 0; i < 40; i++) {
        float r = 1800.0f - i * 42.0f;
        float alpha = 0.018f * ((float)i / 40.0f);
        dl->AddCircleFilled(ImVec2(orb2_x, orb2_y), r, IM_COL32(138, 87, 237, (int)(255 * alpha)));
    }

    // Orb 3: Deep Blue
    float orb3_x = cx + sinf(t * 1.8f) * (ws.x * 0.45f);
    float orb3_y = cy + cosf(t * 1.6f) * (ws.y * 0.4f);
    for (int i = 0; i < 40; i++) {
        float r = 1400.0f - i * 32.0f;
        float alpha = 0.018f * ((float)i / 40.0f);
        dl->AddCircleFilled(ImVec2(orb3_x, orb3_y), r, IM_COL32(0, 100, 255, (int)(255 * alpha)));
    }

    // Orb 4: Magenta
    float orb4_x = cx + cosf(t * 1.4f) * (ws.x * 0.45f);
    float orb4_y = cy + sinf(t * 2.4f) * (ws.y * 0.4f);
    for (int i = 0; i < 40; i++) {
        float r = 1400.0f - i * 32.0f;
        float alpha = 0.018f * ((float)i / 40.0f);
        dl->AddCircleFilled(ImVec2(orb4_x, orb4_y), r, IM_COL32(200, 50, 200, (int)(255 * alpha)));
    }

    // --- 4 Counter-moving orbs (opposite direction) ---
    // Counter-Orb 1: Cyan (moves opposite to Orb 1)
    float corb1_x = cx - sinf(t * 2.5f) * (ws.x * 0.4f);
    float corb1_y = cy - cosf(t * 2.0f) * (ws.y * 0.35f);
    for (int i = 0; i < 40; i++) {
        float r = 1800.0f - i * 42.0f;
        float alpha = 0.018f * ((float)i / 40.0f);
        dl->AddCircleFilled(ImVec2(corb1_x, corb1_y), r, IM_COL32(0, 200, 255, (int)(255 * alpha)));
    }

    // Counter-Orb 2: Purple (moves opposite to Orb 2)
    float corb2_x = cx - cosf(t * 2.2f) * (ws.x * 0.4f);
    float corb2_y = cy - sinf(t * 2.8f) * (ws.y * 0.35f);
    for (int i = 0; i < 40; i++) {
        float r = 1800.0f - i * 42.0f;
        float alpha = 0.018f * ((float)i / 40.0f);
        dl->AddCircleFilled(ImVec2(corb2_x, corb2_y), r, IM_COL32(160, 100, 255, (int)(255 * alpha)));
    }

    // Counter-Orb 3: Teal (moves opposite to Orb 3)
    float corb3_x = cx - sinf(t * 1.8f) * (ws.x * 0.45f);
    float corb3_y = cy - cosf(t * 1.6f) * (ws.y * 0.4f);
    for (int i = 0; i < 40; i++) {
        float r = 1400.0f - i * 32.0f;
        float alpha = 0.018f * ((float)i / 40.0f);
        dl->AddCircleFilled(ImVec2(corb3_x, corb3_y), r, IM_COL32(0, 140, 220, (int)(255 * alpha)));
    }

    // Counter-Orb 4: Violet (moves opposite to Orb 4)
    float corb4_x = cx - cosf(t * 1.4f) * (ws.x * 0.45f);
    float corb4_y = cy - sinf(t * 2.4f) * (ws.y * 0.4f);
    for (int i = 0; i < 40; i++) {
        float r = 1400.0f - i * 32.0f;
        float alpha = 0.018f * ((float)i / 40.0f);
        dl->AddCircleFilled(ImVec2(corb4_x, corb4_y), r, IM_COL32(180, 60, 220, (int)(255 * alpha)));
    }

    // 1.5. Very Light Frosted Glass Overlay
    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), IM_COL32(12, 12, 20, 45)); // Very light frost
    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), IM_COL32(255, 255, 255, 2)); // Barely-there sheen
    


    // 2. Two continuous chasing snakes (Cyan and Purple)
    float inset_x = 14.0f; // Push left/right lines outwards so they sit perfectly on the edges of the window
    float inset_y = 2.0f;  // Keep top/bottom near the edge as requested
    float w = ws.x - 2.0f * inset_x;
    float h = ws.y - 2.0f * inset_y;
    float perimeter = 2.0f * (w + h);
    float speed = 2000.0f; // pixels per second - faster
    
    // Draw a faint continuous base border track so the edges are always defined
    dl->AddRect(ImVec2(wp.x+inset_x, wp.y+inset_y), ImVec2(wp.x+ws.x-inset_x, wp.y+ws.y-inset_y), IM_COL32(40,20,80,120), 0, 0, 2.0f);
    
    // The comet head position
    float head_p = fmodf(t * speed, perimeter);
    float tail_len = perimeter * 0.5f; // Each snake is exactly half the perimeter
    
    int segments_per_snake = 150; // Smooth continuous segments
    float seg_len = tail_len / segments_per_snake;
    
    for (int snake = 0; snake < 2; snake++) {
        // Snake 0 head is at head_p. Snake 1 head is at head_p - tail_len
        float current_head = head_p - snake * tail_len;
        
        // Colors for each snake
        int r_head = (snake == 0) ? 0 : 180;
        int g_head = (snake == 0) ? 255 : 50;
        int b_head = 255;
        
        for (int i = 0; i < segments_per_snake; i++) {
            float p1 = current_head - (i * seg_len);
            float p2 = current_head - ((i + 1) * seg_len);
            
            ImVec2 pt1 = GetPerimeterPos(wp, ws, p1, inset_x, inset_y);
            ImVec2 pt2 = GetPerimeterPos(wp, ws, p2, inset_x, inset_y);
            
            // fraction goes from 1.0 (head) to 0.0 (tail)
            float fraction = 1.0f - ((float)i / segments_per_snake);
            
            // Base alpha 0.1 so the ring never vanishes completely
            // Square root falloff keeps the snake bright for a much longer distance
            float alpha = 0.1f + 0.9f * sqrtf(fraction); 

            ImU32 col = IM_COL32(r_head, g_head, b_head, (int)(255 * alpha));
            
            // Dynamic thickness: thicker at the head, thinner at the tail
            float thickness = 0.5f + 0.5f * fraction; 
            
            // Draw continuous line segments
            dl->AddLine(pt1, pt2, col, thickness);
            
            // Add a tiny circle at the joint to round off corners and prevent any gaps
            dl->AddCircleFilled(pt1, thickness * 0.5f, col);
        }
    }
}

// ---- Main UI ----
void RenderUI() {
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime;

    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##main", NULL,
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();

    // Apply Tron Effects
    DrawTronEffects(dl, wp, ws, (float)ImGui::GetTime());

    // ---- Professional Centered Layout ----
    float cardMaxW = 720.0f;
    float cardW = (ws.x - 80.0f < cardMaxW) ? ws.x - 80.0f : cardMaxW;
    float cardH = 540.0f;
    float cardX = wp.x + (ws.x - cardW) * 0.5f;
    float cardY = wp.y + (ws.y - cardH) * 0.5f;
    float t = (float)ImGui::GetTime();

    // Auto-reset status after timer expires
    if(g_statusTimer > 0.0f) {
        g_statusTimer -= dt;
        if(g_statusTimer <= 0.0f) {
            // Reset to Ready, clear temporary error flags
            g_statusType = 0; strcpy(g_statusMsg, "READY FOR SECURE OPERATIONS.");
            g_inputError = false; g_outputError = false;
        }
    }

    // Real-time path validation every frame
    ValidatePathsBackground();
    // Transparent glassmorphic card
    dl->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
        IM_COL32(8, 10, 18, 100), 16.0f);
    dl->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
        IM_COL32(255, 255, 255, 6), 16.0f);
    dl->AddRectFilled(ImVec2(cardX+8, cardY+1), ImVec2(cardX+cardW-8, cardY+2),
        IM_COL32(0, 200, 255, 30), 8.0f);

    // Card border snakes
    {
        float cw = cardW - 4.0f, ch = cardH - 4.0f;
        float cPerim = 2.0f*(cw+ch);
        float cHead = cPerim - fmodf(t*1400.0f, cPerim); // Reverse direction of outer, same speed
        float cTail = cPerim*0.5f; // Each snake covers half
        int cSegs = 100;
        float cSegLen = cTail/cSegs;
        for(int s=0;s<2;s++){
            float curH = cHead - s*cTail;
            int r=(s==0)?0:160, g=(s==0)?220:80, b=255;
            for(int i=0;i<cSegs;i++){
                float p1=curH+i*cSegLen, p2=curH+(i+1)*cSegLen;
                auto cPos=[&](float p)->ImVec2{
                    p=fmodf(p,cPerim);if(p<0)p+=cPerim;
                    if(p<cw)return ImVec2(cardX+2+p,cardY+2);p-=cw;
                    if(p<ch)return ImVec2(cardX+cardW-2,cardY+2+p);p-=ch;
                    if(p<cw)return ImVec2(cardX+cardW-2-p,cardY+cardH-2);p-=cw;
                    return ImVec2(cardX+2,cardY+cardH-2-p);
                };
                ImVec2 a=cPos(p1),bb=cPos(p2);
                float fr=1.0f-(float)i/cSegs;
                float al=0.05f+0.95f*sqrtf(fr);
                dl->AddLine(a,bb,IM_COL32(r,g,b,(int)(255*al)),0.5f+1.0f*fr);
                dl->AddCircleFilled(a,(0.5f+1.0f*fr)*0.4f,IM_COL32(r,g,b,(int)(255*al)));
            }
        }
    }

    float padX=32.0f, innerW=cardW-padX*2, fieldW=innerW-100.0f;
    ImGui::SetCursorScreenPos(ImVec2(cardX+padX, cardY+24));

    // Glowing title box
    float titleBoxY = cardY + 16.0f;
    float titleBoxH = 110.0f;
    float titleBoxX = cardX + padX;
    float titleBoxW = innerW;
    {
        float glowA = 120.0f + 135.0f * (0.5f + 0.5f * sinf(t * 6.0f)); // Fast bright pulse
        int ga = (int)glowA;
        // Subtle fill
        dl->AddRectFilled(ImVec2(titleBoxX, titleBoxY), ImVec2(titleBoxX+titleBoxW, titleBoxY+titleBoxH),
            IM_COL32(5, 8, 18, 140), 8.0f);
        // Glowing border - full brightness
        dl->AddRect(ImVec2(titleBoxX, titleBoxY), ImVec2(titleBoxX+titleBoxW, titleBoxY+titleBoxH),
            IM_COL32(0, 217, 255, ga), 8.0f, 0, 2.0f);
        // Outer bloom - brighter
        dl->AddRect(ImVec2(titleBoxX-1, titleBoxY-1), ImVec2(titleBoxX+titleBoxW+1, titleBoxY+titleBoxH+1),
            IM_COL32(0, 217, 255, ga/3), 9.0f, 0, 3.0f);
    }

    // Title text inside box
    ImGui::SetCursorScreenPos(ImVec2(cardX+padX, titleBoxY + 4));
    if(g_fontTitle) ImGui::PushFont(g_fontTitle);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f,0.85f,1.0f,1.0f));
    {float tw=ImGui::CalcTextSize("CRYPTON").x;ImGui::SetCursorPosX((ws.x-tw)*0.5f);ImGui::Text("CRYPTON");}
    ImGui::PopStyleColor();
    if(g_fontTitle) ImGui::PopFont();
    if(g_fontLarge) ImGui::PushFont(g_fontLarge);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.58f,0.68f,1.0f));
    {float sw=ImGui::CalcTextSize("Advanced Document Security Engine").x;ImGui::SetCursorPosX((ws.x-sw)*0.5f);ImGui::Text("Advanced Document Security Engine");}
    ImGui::PopStyleColor();
    if(g_fontLarge) ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(cardX+padX, titleBoxY + titleBoxH + 12));

    // Source File
    ImGui::SetCursorScreenPos(ImVec2(cardX+padX, ImGui::GetCursorScreenPos().y));
    if(g_fontBold) ImGui::PushFont(g_fontBold);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f,0.85f,1.00f,1));ImGui::Text("SOURCE FILE");ImGui::PopStyleColor();
    if(g_fontBold) ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(cardX+padX, ImGui::GetCursorScreenPos().y));
    ImGui::PushItemWidth(fieldW);if(ImGui::InputText("##input",g_inputFile,sizeof(g_inputFile))) g_inputWasValid = false;ImGui::PopItemWidth();
    // Clear invalid path when user leaves field
    if(ImGui::IsItemDeactivated() && strlen(g_inputFile) > 0) {
        DWORD a = GetFileAttributesA(g_inputFile);
        if(a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_DIRECTORY)) { 
            g_inputFile[0] = '\0'; g_inputError = false; 
            if(!g_outputError && g_statusTimer <= 0.0f) { g_statusType = 0; strcpy(g_statusMsg, "READY FOR SECURE OPERATIONS."); } 
        }
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0.50f,0.75f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0,0.65f,0.95f,1));ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0,0.80f,1,1));
    if(ImGui::Button("Browse##1",ImVec2(80,0))) BrowseFile(g_inputFile);ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0,4));ImGui::SetCursorScreenPos(ImVec2(cardX+padX, ImGui::GetCursorScreenPos().y));

    // Destination Folder
    if(g_fontBold) ImGui::PushFont(g_fontBold);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f,0.85f,1.00f,1));ImGui::Text("DESTINATION FOLDER");ImGui::PopStyleColor();
    if(g_fontBold) ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(cardX+padX, ImGui::GetCursorScreenPos().y));
    ImGui::PushItemWidth(fieldW);if(ImGui::InputText("##output",g_outputFolder,sizeof(g_outputFolder))) g_outputWasValid = false;ImGui::PopItemWidth();
    // Clear invalid path when user leaves field
    if(ImGui::IsItemDeactivated() && strlen(g_outputFolder) > 0) {
        DWORD a = GetFileAttributesA(g_outputFolder);
        bool isDir = (a != INVALID_FILE_ATTRIBUTES) && (a & FILE_ATTRIBUTE_DIRECTORY);
        if(!isDir) { 
            g_outputFolder[0] = '\0'; g_outputError = false; 
            if(!g_inputError && g_statusTimer <= 0.0f) { g_statusType = 0; strcpy(g_statusMsg, "READY FOR SECURE OPERATIONS."); } 
        }
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0.50f,0.75f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0,0.65f,0.95f,1));ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0,0.80f,1,1));
    if(ImGui::Button("Browse##2",ImVec2(80,0))) BrowseFolder(g_outputFolder);ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0,4));ImGui::SetCursorScreenPos(ImVec2(cardX+padX, ImGui::GetCursorScreenPos().y));

    // Security Password
    if(g_fontBold) ImGui::PushFont(g_fontBold);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f,0.85f,1.00f,1));ImGui::Text("SECURITY PASSWORD");ImGui::PopStyleColor();
    if(g_fontBold) ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(cardX+padX, ImGui::GetCursorScreenPos().y));
    ImGuiInputTextFlags pflags=g_showPassword?0:ImGuiInputTextFlags_Password;
    ImGui::PushItemWidth(fieldW);ImGui::InputText("##pass",g_password,sizeof(g_password),pflags);ImGui::PopItemWidth();ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.15f,0.16f,0.22f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.20f,0.22f,0.30f,1));ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0.25f,0.28f,0.38f,1));
    if(ImGui::Button(g_showPassword?"Hide":"Show",ImVec2(80,0))) g_showPassword=!g_showPassword;ImGui::PopStyleColor(3);

    // Password strength
    int strength=PasswordStrength(g_password);
    if(strlen(g_password)>0){
        ImGui::SetCursorScreenPos(ImVec2(cardX+padX, ImGui::GetCursorScreenPos().y));
        float frac=strength/6.0f;ImU32 barCol;const char* label;
        if(strength<=2){barCol=IM_COL32(220,50,50,255);label="Weak";}
        else if(strength<=4){barCol=IM_COL32(240,180,40,255);label="Moderate";}
        else{barCol=IM_COL32(0,200,120,255);label="Strong";}
        ImVec2 bp=ImGui::GetCursorScreenPos();float barW=fieldW*frac;
        dl->AddRectFilled(bp,ImVec2(bp.x+fieldW,bp.y+6),IM_COL32(30,32,44,255),3);
        dl->AddRectFilled(bp,ImVec2(bp.x+barW,bp.y+6),barCol,3);
        ImGui::Dummy(ImVec2(0,10));
        float ccr=((barCol>>0)&0xFF)/255.0f,ccg=((barCol>>8)&0xFF)/255.0f,ccb=((barCol>>16)&0xFF)/255.0f;
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(ccr,ccg,ccb,1));ImGui::SameLine(0,8);ImGui::Text("%s",label);ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0,12));

    // Buttons
    float btnW=(innerW-16)/2,btnH=48;
    ImGui::SetCursorScreenPos(ImVec2(cardX+padX, ImGui::GetCursorScreenPos().y));
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0.50f,0.75f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0,0.65f,0.95f,1));ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0,0.80f,1,1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,8);
    if(g_fontBold) ImGui::PushFont(g_fontBold);
    if(ImGui::Button("ENCRYPT",ImVec2(btnW,btnH))) DoEncrypt();
    if(g_fontBold) ImGui::PopFont();
    ImGui::PopStyleVar();ImGui::PopStyleColor(3);

    ImGui::SameLine(0,16);
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.45f,0.30f,0.75f,1));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(0.55f,0.40f,0.85f,1));ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(0.65f,0.50f,1,1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,8);
    if(g_fontBold) ImGui::PushFont(g_fontBold);
    if(ImGui::Button("DECRYPT",ImVec2(btnW,btnH))) DoDecrypt();
    if(g_fontBold) ImGui::PopFont();
    ImGui::PopStyleVar();ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0,8));

    // Status bar inside card (with spacing from buttons)
    float stBarY=cardY+cardH-50.0f;
    float stBarW=cardW-padX*2;
    dl->AddRect(ImVec2(cardX+padX,stBarY),ImVec2(cardX+cardW-padX,stBarY+36),IM_COL32(0,180,255,10),6.0f,0,1.0f); // Just a faint border, no fill
    ImVec4 stCol;
    switch(g_statusType){
        case 1:stCol=ImVec4(0.90f,0.20f,0.25f,1);break;case 2:stCol=ImVec4(0,0.80f,0.50f,1);break;
        case 3:stCol=ImVec4(0.90f,0.75f,0.20f,1);break;default:stCol=ImVec4(0.45f,0.55f,0.65f,1);break;
    }
    ImGui::PushStyleColor(ImGuiCol_Text,stCol);
    float msgW=ImGui::CalcTextSize(g_statusMsg).x;
    float stCenterX=cardX+padX+(stBarW-msgW)*0.5f;
    ImGui::SetCursorScreenPos(ImVec2(stCenterX,stBarY+9));
    ImGui::Text("%s",g_statusMsg);
    ImGui::PopStyleColor();

    ImGui::End();
}

// ---- WndProc ----
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if(ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;
    switch(msg){
    case WM_DROPFILES: {
        HDROP hd=(HDROP)wParam; char buf[MAX_PATH];
        DragQueryFileA(hd,0,buf,MAX_PATH);
        DWORD attr=GetFileAttributesA(buf);
        if(attr!=INVALID_FILE_ATTRIBUTES && !(attr&FILE_ATTRIBUTE_DIRECTORY))
            strcpy(g_inputFile, buf);
        DragFinish(hd); return 0;
    }
    case WM_SIZE: return 0;
    case WM_SYSCOMMAND:
        if((wParam&0xfff0)==SC_KEYMENU) return 0;
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ---- WinMain ----
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    // Enable High DPI awareness to prevent blurriness
    ImGui_ImplWin32_EnableDpiAwareness();

    // Init COM for file dialogs
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    WNDCLASSEXA wc={sizeof(wc), CS_OWNDC, WndProc, 0, 0, hInst,
        LoadIcon(NULL,IDI_APPLICATION), LoadCursor(NULL,IDC_ARROW),
        NULL, NULL, "CryptifyProClass", NULL};
    RegisterClassExA(&wc);

    // Get primary monitor DPI scale for initial window size
    float initial_dpi_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({0,0}, MONITOR_DEFAULTTOPRIMARY));
    if (initial_dpi_scale <= 0.0f) initial_dpi_scale = 1.0f;

    g_hwnd = CreateWindowExA(WS_EX_ACCEPTFILES, "CryptifyProClass",
        "CRYPTON - Advanced Security Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, (int)(620 * initial_dpi_scale), (int)(580 * initial_dpi_scale),
        NULL, NULL, hInst, NULL);

    g_hDC = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd={sizeof(pfd),1,
        PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,32, 0,0,0,0,0,0,0,0,0,0,0,0,0, 24,8,0,
        PFD_MAIN_PLANE,0,0,0,0};
    SetPixelFormat(g_hDC, ChoosePixelFormat(g_hDC,&pfd), &pfd);
    g_hRC = wglCreateContext(g_hDC);
    wglMakeCurrent(g_hDC, g_hRC);

    ShowWindow(g_hwnd, nShow);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;

    // Get DPI scale for the specific window
    float dpi_scale = ImGui_ImplWin32_GetDpiScaleForHwnd(g_hwnd);
    if(dpi_scale <= 0.0f) dpi_scale = 1.0f;

    // Load fonts scaled by DPI
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f * dpi_scale);
    g_fontBold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 16.0f * dpi_scale);
    g_fontLarge = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 22.0f * dpi_scale);
    g_fontTitle = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 52.0f * dpi_scale);

    // Scale ImGui styles
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpi_scale);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplOpenGL2_Init();
    SetupTheme();

    bool running = true;
    while(running){
        MSG msg;
        while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
            TranslateMessage(&msg); DispatchMessage(&msg);
            if(msg.message==WM_QUIT) running=false;
        }
        if(!running) break;

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        RenderUI();
        ImGui::Render();

        RECT r; GetClientRect(g_hwnd,&r);
        glViewport(0,0,r.right-r.left,r.bottom-r.top);
        glClearColor(0.07f,0.07f,0.11f,1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(g_hDC);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(NULL,NULL);
    wglDeleteContext(g_hRC);
    ReleaseDC(g_hwnd, g_hDC);
    DestroyWindow(g_hwnd);
    CoUninitialize();
    return 0;
}
