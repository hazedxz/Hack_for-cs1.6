import ctypes
import ctypes.wintypes
import struct
import time
import threading
from ctypes import c_ulong, c_void_p, c_size_t, c_uint32


PROCESS_ALL_ACCESS = 0x1F0FFF
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400



dwEntityList = 0x01C6A2AC
dwViewMatrix = 0x01C5B880
dwLocalPlayer = 0x01C6A2A8
m_iTeamNum = 0xF4
m_vecOrigin = 0x38
m_iHealth = 0xF8
m_bDormant = 0xE9

class Vector3(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float),
                ("y", ctypes.c_float),
                ("z", ctypes.c_float)]


import pygame
import sys

class CSWallhack:
    def __init__(self):
        self.pid = self.get_process_id("hl.exe")
        if not self.pid:
            print("[-] CS 1.6 no está ejecutándose")
            sys.exit(1)

        self.hProcess = ctypes.windll.kernel32.OpenProcess(
            PROCESS_VM_READ, False, self.pid
        )
        if not self.hProcess:
            print("[-] No se pudo abrir el proceso. Ejecuta como Admin.")
            sys.exit(1)

        
        pygame.init()
        self.info = pygame.display.Info()
        self.screen = pygame.display.set_mode(
            (self.info.current_w, self.info.current_h),
            pygame.NOFRAME | pygame.SRCALPHA
        )
        pygame.display.set_caption("CSWH Overlay")

        
        hwnd = pygame.display.get_wm_info()["window"]
        GWL_EXSTYLE = -20
        WS_EX_LAYERED = 0x80000
        WS_EX_TRANSPARENT = 0x20
        WS_EX_TOOLWINDOW = 0x80

        ctypes.windll.user32.SetWindowLongW(hwnd, GWL_EXSTYLE,
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW)
        ctypes.windll.user32.SetLayeredWindowAttributes(hwnd, 0, 200, 0x2)

        self.clock = pygame.time.Clock()
        self.running = True

    def get_process_id(self, name):
        
        kernel32 = ctypes.windll.kernel32
        CreateToolhelp32Snapshot = kernel32.CreateToolhelp32Snapshot
        Process32First = kernel32.Process32First
        Process32Next = kernel32.Process32Next
        CloseHandle = kernel32.CloseHandle

        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", c_uint32),
                ("cntUsage", c_uint32),
                ("th32ProcessID", c_uint32),
                ("th32DefaultHeapID", c_void_p),
                ("th32ModuleID", c_uint32),
                ("cntThreads", c_uint32),
                ("th32ParentProcessID", c_uint32),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", c_uint32),
                ("szExeFile", ctypes.c_char * 260)
            ]

        hSnap = CreateToolhelp32Snapshot(0x00000002, 0)
        if hSnap == -1:
            return 0

        pe = PROCESSENTRY32()
        pe.dwSize = ctypes.sizeof(PROCESSENTRY32)

        if Process32First(hSnap, ctypes.byref(pe)):
            while True:
                if pe.szExeFile.decode('utf-8', errors='ignore').lower() == name.lower():
                    CloseHandle(hSnap)
                    return pe.th32ProcessID
                if not Process32Next(hSnap, ctypes.byref(pe)):
                    break

        CloseHandle(hSnap)
        return 0

    def read_memory(self, address, size):
        buffer = ctypes.create_string_buffer(size)
        bytes_read = c_size_t(0)
        if ctypes.windll.kernel32.ReadProcessMemory(
            self.hProcess, c_void_p(address), buffer, size, ctypes.byref(bytes_read)
        ):
            return buffer.raw
        return None

    def read_float(self, address):
        data = self.read_memory(address, 4)
        if data:
            return struct.unpack('f', data)[0]
        return 0.0

    def read_int(self, address):
        data = self.read_memory(address, 4)
        if data:
            return struct.unpack('i', data)[0]
        return 0

    def get_view_matrix(self):
        buffer = self.read_memory(self.dwViewMatrix, 16 * 4)
        if buffer:
            return struct.unpack('16f', buffer)
        return None

    def world_to_screen(self, matrix, pos):
        if not matrix:
            return None

        w = matrix[12] * pos[0] + matrix[13] * pos[1] + matrix[14] * pos[2] + matrix[15]
        if w < 0.01:
            return None

        inv_w = 1.0 / w
        x = (matrix[0] * pos[0] + matrix[1] * pos[1] + matrix[2] * pos[2] + matrix[3]) * inv_w
        y = (matrix[4] * pos[0] + matrix[5] * pos[1] + matrix[6] * pos[2] + matrix[7]) * inv_w

        
        screen_x = (self.info.current_w / 2) * (1.0 + x)
        screen_y = (self.info.current_h / 2) * (1.0 - y)

        return (int(screen_x), int(screen_y))

    def get_player_info(self, index):
        entity_ptr_addr = self.dwEntityList + (index * 0x10)
        entity_ptr = self.read_int(entity_ptr_addr)
        if not entity_ptr or entity_ptr < 0x10000:
            return None

        health = self.read_int(entity_ptr + self.m_iHealth)
        if health <= 0 or health > 100:
            return None

        team = self.read_int(entity_ptr + self.m_iTeamNum)
        x = self.read_float(entity_ptr + self.m_vecOrigin)
        y = self.read_float(entity_ptr + self.m_vecOrigin + 4)
        z = self.read_float(entity_ptr + self.m_vecOrigin + 8)

        return {
            "health": health,
            "team": team,
            "pos": (x, y, z),
            "ent_ptr": entity_ptr
        }

    def run(self):
        print("[+] Wallhack ESP ejecutándose...")
        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                if event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_END:
                        self.running = False

            self.screen.fill((0, 0, 0, 0))

            
            view_matrix = self.get_view_matrix()

            
            local_player = self.read_int(self.dwLocalPlayer)
            local_team = 0
            if local_player:
                local_team = self.read_int(local_player + self.m_iTeamNum)

            
            for i in range(1, 32):  
                player = self.get_player_info(i)
                if not player:
                    continue

                screen_pos = self.world_to_screen(view_matrix, player["pos"])
                if not screen_pos:
                    continue

                
                color = (255, 0, 0, 200) if player["team"] != local_team else (0, 255, 0, 200)

                
                box_size = 40
                pygame.draw.rect(
                    self.screen, color[:3],
                    (screen_pos[0]-box_size//2, screen_pos[1]-box_size,
                     box_size, box_size*2), 2
                )

                
                font = pygame.font.Font(None, 20)
                text = font.render(f"P{i}", True, color[:3])
                self.screen.blit(text, (screen_pos[0]-15, screen_pos[1]-box_size-20))

                
                health_color = (0, 255, 0) if player["health"] > 50 else (255, 255, 0) if player["health"] > 20 else (255, 0, 0)
                bar_width = 4
                bar_height = 30
                hp_height = int(bar_height * player["health"] / 100)
                pygame.draw.rect(self.screen, health_color,
                               (screen_pos[0] + box_size//2 + 5, screen_pos[1] - bar_height + (bar_height - hp_height),
                                bar_width, hp_height))

            pygame.display.flip()
            self.clock.tick(60)

        ctypes.windll.kernel32.CloseHandle(self.hProcess)
        pygame.quit()

if __name__ == "__main__":
    wh = CSWallhack()
    wh.run()