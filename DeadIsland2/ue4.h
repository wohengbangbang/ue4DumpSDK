#pragma once
#include <iostream>

// 游戏结构 ue4/5
/*
*	GWorld -> UWorld -> ULevel -> UActor -> RootComponent - > 对象世界坐标
*   RootComponent -> Pawn(class Actor) -> PlayerState -> info(class PlayerState) 
*   UWorld -> OwningGameInstance -> LocalPlayers -> Player(class LocalPlayer) -> PlayerController -> 
*/


extern uintptr_t matrixAddr;
extern uintptr_t gName;
extern uintptr_t gWorld;
extern uintptr_t gObject;

extern uint64_t uWorld;
extern uint64_t uLevel;
extern uint64_t uActor;

// 模块基地址 DeadIsland-Win64-Shipping.exe
extern uintptr_t baseModelAddr;

struct FNameEntryHeader {
    uint16_t bIsWide : 1;
    static constexpr uint32_t ProbeHashBits = 5;
    uint16_t LowercaseProbHash : ProbeHashBits;
    uint16_t Len : 10;
};


struct FNameEntry {
    FNameEntryHeader Header;
    union {
        char AnsiName[1024];
        wchar_t WideName[1024];
    };
};


// 获取游戏模型名称
std::string GetName(int NameId);