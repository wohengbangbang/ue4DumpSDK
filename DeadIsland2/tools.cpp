#include "tools.hpp"

// 获取进程ID
DWORD GetProcessId(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };

    if (::Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (::Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

// 获取模块基地址
uintptr_t GetModuleBase(DWORD pid, const wchar_t* moduleName) {
    uintptr_t base = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    MODULEENTRY32W entry = { sizeof(MODULEENTRY32W) };

    if (Module32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, moduleName) == 0) {
                base = (uintptr_t)entry.modBaseAddr;
                break;
            }
        } while (Module32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return base;
}

// 安全内存读取
bool ReadMemory(uintptr_t addr, void* buff, size_t size, HANDLE gameHwnd) {
    SIZE_T bytesRead;
    return ::ReadProcessMemory(gameHwnd, (LPCVOID)addr, buff, size, &bytesRead) && bytesRead == size;
}
