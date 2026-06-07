#include <windows.h>
#include <tlhelp32.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>



#define dwLocalPlayer      0x01C6A2A8
#define dwEntityList       0x01C6A2AC
#define dwClientState      0x01C5B8A0  
#define dwViewAngles       0x4D10      
#define m_iTeamNum         0xF4
#define m_vecOrigin        0x38
#define m_iHealth          0xF8
#define m_bDormant         0xE9
#define m_vecViewOffset    0x104
#define m_bSpotted         0x935
#define m_iCrosshairId     0x0B2E


struct Vector3 {
    float x, y, z;

    Vector3 operator-(const Vector3& o) const {
        return { x - o.x, y - o.y, z - o.z };
    }

    float Length() const {
        return sqrtf(x*x + y*y + z*z);
    }

    void Normalize() {
        while (x > 180) x -= 360;
        while (x < -180) x += 360;
        while (y > 89) y -= 180;
        while (y < -89) y += 180;
        if (z != 0) z = 0;
    }
};

class Aimbot {
private:
    HANDLE hProcess;
    DWORD pid;
    DWORD clientState;
    DWORD engineModule;
    Vector3 viewAngles;

    
    float aimFOV = 10.0f;       
    float smoothFactor = 5.0f;  
    int boneTarget = 6;         
    bool aimEnabled = true;
    bool aimOnKey = false;      

public:
    Aimbot() : hProcess(NULL), pid(0), clientState(0), engineModule(0) {
        pid = GetProcessId("hl.exe");
        if (!pid) {
            std::cerr << "[-] hl.exe no encontrado" << std::endl;
            exit(1);
        }

        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess) {
            std::cerr << "[-] No se pudo abrir hl.exe (Admin?)" << std::endl;
            exit(1);
        }

        
        engineModule = GetModuleAddress("engine.dll");
        if (!engineModule) {
            
            clientState = ReadOffset(DWORD) + dwClientState;
        } else {
            clientState = ReadOffset(DWORD, engineModule + dwClientState);
        }

        std::cout << "[+] Aimbot cargado. F1 = toggle, F2 = toggle key-only" << std::endl;
        std::cout << "[+] Mouse4 (XButton1) para aim mientras se mantiene presionado" << std::endl;
    }

    ~Aimbot() {
        if (hProcess) CloseHandle(hProcess);
    }

    DWORD GetProcessId(const char* name) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return 0;
        PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };
        DWORD ret = 0;
        if (Process32First(hSnap, &pe)) {
            do {
                if (!_stricmp(pe.szExeFile, name)) {
                    ret = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
        return ret;
    }

    DWORD GetModuleAddress(const char* moduleName) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (hSnap == INVALID_HANDLE_VALUE) return 0;
        MODULEENTRY32 me = { sizeof(MODULEENTRY32) };
        DWORD ret = 0;
        if (Module32First(hSnap, &me)) {
            do {
                if (!_stricmp(me.szModule, moduleName)) {
                    ret = (DWORD)me.modBaseAddr;
                    break;
                }
            } while (Module32Next(hSnap, &me));
        }
        CloseHandle(hSnap);
        return ret;
    }

    template<typename T>
    T ReadOffset(DWORD address) {
        T val;
        ReadProcessMemory(hProcess, (LPCVOID)address, &val, sizeof(T), NULL);
        return val;
    }

    bool ReadMemory(DWORD address, void* buffer, SIZE_T size) {
        return ReadProcessMemory(hProcess, (LPCVOID)address, buffer, size, NULL) != 0;
    }

    bool WriteMemory(DWORD address, void* buffer, SIZE_T size) {
        return WriteProcessMemory(hProcess, (LPVOID)address, buffer, size, NULL) != 0;
    }

    Vector3 GetEntityPosition(DWORD entityPtr) {
        return ReadOffset<Vector3>(entityPtr + m_vecOrigin);
    }

    Vector3 GetEntityBone(DWORD entityPtr, int bone) {
        
        
        DWORD boneMatrix = ReadOffset<DWORD>(entityPtr + 0x2B8);
        if (!boneMatrix) return {0, 0, 0};

        
        
        float boneData[12];
        ReadMemory(boneMatrix + (bone * 0x30), boneData, sizeof(boneData));

        return { boneData[0], boneData[1], boneData[2] };
    }

    Vector3 CalculateAngle(const Vector3& src, const Vector3& dst) {
        Vector3 delta = dst - src;
        float hyp = sqrtf(delta.x*delta.x + delta.y*delta.y);

        Vector3 angle;
        angle.x = atan2f(-delta.z, hyp) * (180.0f / 3.14159265f);
        angle.y = atan2f(delta.y, delta.x) * (180.0f / 3.14159265f);
        angle.z = 0;

        angle.Normalize();
        return angle;
    }

    float GetFOV(const Vector3& viewAngle, const Vector3& targetAngle) {
        Vector3 delta = viewAngle - targetAngle;
        delta.Normalize();
        return sqrtf(delta.x*delta.x + delta.y*delta.y);
    }

    Vector3 SmoothAngle(const Vector3& current, const Vector3& target, float smooth) {
        Vector3 delta = target - current;
        delta.Normalize();
        return {
            current.x + delta.x / smooth,
            current.y + delta.y / smooth,
            current.z
        };
    }

    void Run() {
        std::cout << "[+] Aimbot funcionando..." << std::endl;

        while (true) {
            
            if (GetAsyncKeyState(VK_F1) & 1) aimEnabled = !aimEnabled;
            if (GetAsyncKeyState(VK_F2) & 1) aimOnKey = !aimOnKey;
            if (GetAsyncKeyState(VK_END) & 1) break;

            Sleep(1);

            if (!aimEnabled) continue;

            
            if (aimOnKey && !(GetAsyncKeyState(VK_XBUTTON1) & 0x8000)) continue;

            
            DWORD localPlayer = ReadOffset<DWORD>(dwLocalPlayer);
            if (!localPlayer) continue;

            int localTeam = ReadOffset<int>(localPlayer + m_iTeamNum);
            int localHealth = ReadOffset<int>(localPlayer + m_iHealth);

            
            Vector3 localPos = GetEntityPosition(localPlayer);
            Vector3 localViewOffset = ReadOffset<Vector3>(localPlayer + m_vecViewOffset);
            Vector3 eyePos = { localPos.x + localViewOffset.x,
                               localPos.y + localViewOffset.y,
                               localPos.z + localViewOffset.z };

            
            DWORD clientStatePtr = clientState;
            Vector3 currentAngles = ReadOffset<Vector3>(clientStatePtr + dwViewAngles);

            
            float bestFOV = aimFOV;
            DWORD bestTarget = 0;
            Vector3 bestBonePos = {0, 0, 0};

            for (int i = 1; i <= 32; i++) {
                DWORD entityPtr = ReadOffset<DWORD>(dwEntityList + i * 0x10);
                if (!entityPtr || entityPtr == localPlayer) continue;

                int health = ReadOffset<int>(entityPtr + m_iHealth);
                int team = ReadOffset<int>(entityPtr + m_iTeamNum);
                bool dormant = ReadOffset<bool>(entityPtr + m_bDormant);

                if (health <= 0 || health > 100) continue;
                if (team == localTeam) continue;  
                if (dormant) continue;

                
                Vector3 bonePos = GetEntityBone(entityPtr, boneTarget);
                if (bonePos.x == 0 && bonePos.y == 0 && bonePos.z == 0) {
                    r
                    bonePos = GetEntityPosition(entityPtr);
                }

                
                Vector3 targetAngle = CalculateAngle(eyePos, bonePos);
                float fov = GetFOV(currentAngles, targetAngle);

                if (fov < bestFOV) {
                    bestFOV = fov;
                    bestTarget = entityPtr;
                    bestBonePos = bonePos;
                }
            }

            
            if (bestTarget) {
                Vector3 targetAngle = CalculateAngle(eyePos, bestBonePos);
                Vector3 smoothed = SmoothAngle(currentAngles, targetAngle, smoothFactor);

                
                WriteMemory(clientStatePtr + dwViewAngles, &smoothed, sizeof(Vector3));
            }
        }

        std::cout << "[+] Aimbot finalizado." << std::endl;
    }
};

int main() {
    SetConsoleTitle("CS 1.6 Aimbot (Externo)");

    Aimbot aimbot;
    aimbot.Run();

    return 0;
}
