#pragma once
#include <Windows.h>
#include <tlhelp32.h>


// 获取进程ID
DWORD GetProcessId(const wchar_t* processName);

// 获取模块基地址
uintptr_t GetModuleBase(DWORD pid, const wchar_t* moduleName);

// 安全内存读取
bool ReadMemory(uintptr_t addr, void* buff, size_t size, HANDLE gameHwnd);

template<typename T>
 T ReadMemory(uintptr_t addr, HANDLE gameHwnd) {
    T value;
    if (!ReadMemory(addr, &value, sizeof(T),gameHwnd)) {
        memset(&value, 0, sizeof(T));
        //std::cout << "Error:" << ::GetLastError() << std::endl;
    }
    return value;
}

 // 添加数组读取支持
 template<typename T, size_t N>
 bool ReadArray(uintptr_t addr, T(&arr)[N], HANDLE gameHwnd) {
     return ReadMemory(addr, arr, sizeof(T) * N, gameHwnd);
 }

