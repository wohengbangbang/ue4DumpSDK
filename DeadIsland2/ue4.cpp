#include "ue4.h"
#include "tools.hpp"
#include "gameData.h"

uintptr_t matrixAddr = 0;
uintptr_t gName = 0;
uintptr_t gWorld = 0;
uintptr_t gObject = 0;
uint64_t uWorld = 0;
uint64_t uLevel = 0;
uint64_t uActor = 0;
uintptr_t baseModelAddr = 0;

// 获取游戏模型名称
std::string GetName(int NameId) {
    //return *reinterpret_cast<FNameEntry*> (gName[NameId >>16] + 2 * NameId&&65535); // ue引擎GName算法（原版）

    if (NameId == 0) return "";
    uintptr_t BlockPtrAddr = gName + (NameId >> 16) * 8;
    uint64_t BlockPtr = ReadMemory<uint64_t>(BlockPtrAddr,gameHwnd);

    if (!BlockPtr) return "";

    uintptr_t NameEntryAddr = BlockPtr + 2 * (NameId & 0xFFFF); // 65535

    FNameEntry info = ReadMemory<FNameEntry>(NameEntryAddr,gameHwnd);

    std::string name;
    // 判断是否等于1
    if (info.Header.bIsWide == 1) {

        std::wstring wstr(info.WideName, info.Header.Len);
        name = std::string(wstr.begin(), wstr.end());
    }
    else {
        name = std::string(info.AnsiName, info.Header.Len);
    }

    auto pos = name.find('/');

    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }

    return name;
}