#include <windows.h>
#include <gl/GL.h>


typedef void (WINAPI* oglDepthRange)(GLclampd, GLclampd);
typedef void (WINAPI* oglDepthFunc)(GLenum);
typedef void (WINAPI* oglClearDepth)(GLclampd);
typedef BOOL (WINAPI* wglSwapBuffers_t)(HDC);


oglDepthFunc oglDepthFuncOrig = NULL;
oglDepthRange oglDepthRangeOrig = NULL;
oglClearDepth oglClearDepthOrig = NULL;
wglSwapBuffers_t wglSwapBuffersOrig = NULL;
bool wallhackEnabled = true;


BYTE depthFuncPatch[5];
BYTE depthFuncOriginal[5];


void WINAPI hk_glDepthFunc(GLenum func) {
    if (wallhackEnabled) {
        oglDepthFuncOrig(GL_ALWAYS);  
    } else {
        oglDepthFuncOrig(func);
    }
}


BOOL WINAPI hk_wglSwapBuffers(HDC hdc) {
    
    return wglSwapBuffersOrig(hdc);
}

void SetupHook() {
    HMODULE hOpenGL = GetModuleHandle("opengl32.dll");
    if (!hOpenGL) return;

    
    oglDepthFuncOrig = (oglDepthFunc)GetProcAddress(hOpenGL, "glDepthFunc");
    wglSwapBuffersOrig = (wglSwapBuffers_t)GetProcAddress(hOpenGL, "wglSwapBuffers");

    
    
    DWORD oldProtect;
    VirtualProtect(oglDepthFuncOrig, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    
    memcpy(depthFuncOriginal, oglDepthFuncOrig, 5);

    
    DWORD offset = ((DWORD)hk_glDepthFunc - (DWORD)oglDepthFuncOrig) - 5;
    depthFuncPatch[0] = 0xE9;  // jmp
    memcpy(&depthFuncPatch[1], &offset, 4);

    
    memcpy(oglDepthFuncOrig, depthFuncPatch, 5);

    VirtualProtect(oglDepthFuncOrig, 5, oldProtect, &oldProtect);
}

void RestoreHook() {
    DWORD oldProtect;
    VirtualProtect(oglDepthFuncOrig, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(oglDepthFuncOrig, depthFuncOriginal, 5);
    VirtualProtect(oglDepthFuncOrig, 5, oldProtect, &oldProtect);
}


DWORD WINAPI MainThread(LPVOID lpReserved) {
    
    while (!GetModuleHandle("opengl32.dll")) {
        Sleep(100);
    }

    SetupHook();

    
    while (true) {
        if (GetAsyncKeyState(VK_F1) & 1) {
            wallhackEnabled = !wallhackEnabled;
        }
        if (GetAsyncKeyState(VK_END) & 1) {
            break;
        }
        Sleep(10);
    }

    RestoreHook();
    FreeLibraryAndExitThread((HMODULE)lpReserved, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, hModule, 0, NULL);
    }
    return TRUE;
}
