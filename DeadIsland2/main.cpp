#include <iostream>
#include <Windows.h>
#include <format>
#include "tools.hpp"
#include "ue4.h"
#include "gameData.h"
#include "OSImGui/OS-ImGui.h"
#include "OSImGui/imgui/imgui.h"
#include "OSImGui/imgui/imgui_internal.h"


// 初始化
bool init() {

    // 设置控制台输出编码为UTF-8
    SetConsoleOutputCP(CP_UTF8);
    // 设置控制台输入编码为UTF-8  
    SetConsoleCP(CP_UTF8);

    // 模块名
    const wchar_t* exeName = L"DeadIsland-Win64-Shipping.exe";
    // 获取游戏窗口句柄
    gameWindowHwnd = ::FindWindow(L"UnrealWindow", NULL);

    if (!gameWindowHwnd) {
        std::cout << "窗口未找到\n";
        return false;
    }

    // 通过窗口句柄获取进程id
    ::GetWindowThreadProcessId(gameWindowHwnd, &gamePid);
    // 通过pid打开进程获取进程句柄
    gameHwnd = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, gamePid);

    if (!gameHwnd) {
        std::cout << "游戏句柄获取失败\n";
        return false;
    }

    baseModelAddr = GetModuleBase(gamePid, exeName);

    if (!baseModelAddr) {
        std::cout << "游戏模块基地址获取失败\n";
        return false;
    }

    // 获取GName地址
    gName = baseModelAddr + 0x6CF4C90;
    if (!gName) {
        std::cout << "GName获取失败\n";
        return false;
    }

    // 获取gObject
    gObject = baseModelAddr + 0x6D28680;
    if (!gObject) {
        std::cout << "GObject获取失败\n";
        return false;
    }

    // 获取gWorld
    gWorld = baseModelAddr + 0x6EAD848;
    if (!gWorld) {
        std::cout << "GWorld获取失败\n";
        return false;
    }

    // 获取矩阵
    matrixAddr = baseModelAddr + 0x697D490;
    if (!matrixAddr) {
        std::cout << "matrixAddr获取失败\n";
        return false;
    }

    printf("[+] 成功附加到进程Pid = %d\n", gamePid);
    printf("[+] 模块基址 = 0x%llX\n", baseModelAddr);
    printf("[+] GObject = 0x%llX\n", gObject);
    printf("[+] GWorld = 0x%llX\n\n", gWorld);

    return true;
}

bool worldToScreen(Vector worldCoor, ClipCoor* screenCoor) {
    // 计算齐次坐标的w分量
    float w = worldCoor.x * viewMatrix[3] +
        worldCoor.y * viewMatrix[7] +
        worldCoor.z * viewMatrix[11] +
        viewMatrix[15];

    // 如果w值太小，表示点在摄像机后面，不可见
    if (w < 0.001f) {
        return false;
    }

    // 计算投影后的x和y分量
    float x = worldCoor.x * viewMatrix[0] +
        worldCoor.y * viewMatrix[4] +
        worldCoor.z * viewMatrix[8] +
        viewMatrix[12];

    float y = worldCoor.x * viewMatrix[1] +
        worldCoor.y * viewMatrix[5] +
        worldCoor.z * viewMatrix[9] +
        viewMatrix[13];

    // 归一化到NDC空间[-1, 1]
    float nx = x / w;
    float ny = y / w;


    if (GetWindowRect(gameWindowHwnd, &windowPosInfo)) {
        width = windowPosInfo.right - windowPosInfo.left;
        height = windowPosInfo.bottom - windowPosInfo.top;
    }


    // 转换到屏幕空间
    float windowCenterX = width / 2.0f;
    float windowCenterY = height / 2.0f;

    // 设置屏幕坐标，z分量设为0（2D屏幕坐标）
    screenCoor->x = windowCenterX + (windowCenterX * nx);
    screenCoor->y = windowCenterY - (windowCenterY * ny);  // Y轴翻转
    screenCoor->z = 0.0f;

    return true;
}



/*bool WordToScrenn(Vector worldCoor, Vec2* screenCoor)
{
    //GetWindowsInfo();
    

    // 世界坐标转屏幕坐标(行主序)
    ClipCoor clipCoor;
    clipCoor.x = viewMatrix[0][0] * worldCoor.x + viewMatrix[0][1] * worldCoor.y + viewMatrix[0][2] * worldCoor.z + viewMatrix[0][3];
    clipCoor.y = viewMatrix[1][0] * worldCoor.x + viewMatrix[1][1] * worldCoor.y + viewMatrix[1][2] * worldCoor.z + viewMatrix[1][3];
    clipCoor.z = viewMatrix[2][0] * worldCoor.x + viewMatrix[2][1] * worldCoor.y + viewMatrix[2][2] * worldCoor.z + viewMatrix[2][3];
    clipCoor.w = viewMatrix[3][0] * worldCoor.x + viewMatrix[3][1] * worldCoor.y + viewMatrix[3][2] * worldCoor.z + viewMatrix[3][3];

    // 判断是否在屏幕内
    if (clipCoor.w < 0.001) {
        return  false;
    }

    // 剪辑坐标转NDC坐标
    Vec3 NDC;
    NDC.x = clipCoor.x / clipCoor.w;
    NDC.y = clipCoor.y / clipCoor.w;
    NDC.z = clipCoor.z / clipCoor.w;

    // NDC坐标转屏幕坐标
    RECT rect;
    if (GetWindowRect(gameWindowHwnd, &rect)) {
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }

    screenCoor->x = width / 2 + (width / 2) * NDC.x;
    screenCoor->y = height / 2 - (height / 2) * NDC.y;

    return true;
}*/


// 遍历游戏数据
void traverseData(){

    // 获取UWorld 世界
    uWorld = ReadMemory<uint64_t>(gWorld, gameHwnd);
    // 获取ULevel 关卡
    uLevel = ReadMemory<uint64_t>(uWorld + 0x30, gameHwnd);
    uint64_t p_worldArray = ReadMemory<uint64_t>(uLevel + 0xA8, gameHwnd); // 世界对象数组地址
    
    // 获取矩阵数组
    uintptr_t p_jzBaseAddr = ReadMemory<uintptr_t>(matrixAddr, gameHwnd);
    uintptr_t p_py1 = ReadMemory<uintptr_t>(p_jzBaseAddr + 0x20, gameHwnd);
    //uint64_t p_jzAddr = ReadMemory<uint64_t>(ReadMemory<uint64_t>(matrixAddr + 0x20, gameHwnd) + 0x270, gameHwnd); // 矩阵地址

    // 获取本地玩家地址（自身）
    uintptr_t gameInstance = ReadMemory<uintptr_t>(uWorld + 0x198, gameHwnd);
    uintptr_t localPlayers = ReadMemory<uintptr_t>(gameInstance + 0x38, gameHwnd);
    uintptr_t player = ReadMemory<uintptr_t>(localPlayers, gameHwnd);
    uintptr_t playerController = ReadMemory<uintptr_t>(player + 0x30, gameHwnd);
    uintptr_t AcknowledgedPawn = ReadMemory<uintptr_t>(playerController + 0x2d8, gameHwnd); // 获取自身人物地址 -> 获取人物信息，角色状态等
    // 获取世界对象数量 
    int worldObjNum = ReadMemory<int>(uLevel + 0xA0, gameHwnd);
    
    for (int i = 0; i < worldObjNum; i++) {

        // 获取矩阵数组
        ReadArray(p_py1 + 0x270, viewMatrix, gameHwnd);  // 自动推断大小

        // 获取对象世界坐标
        uint64_t objAddr = ReadMemory<uint64_t>(p_worldArray + i * 8, gameHwnd);
        uint64_t rootComponent = ReadMemory<uint64_t>(objAddr + 0x160, gameHwnd); // 获取根组件
        if (rootComponent) {
            Vector objVec = ReadMemory<Vector>(rootComponent + 0x184, gameHwnd);
            if (objVec.x == 0 || objVec.y == 0 || objVec.z == 0) continue;
            

            ClipCoor coor;
            if (worldToScreen(objVec, &coor)) {
               std::string objTypeName = GetName(ReadMemory<int>(objAddr + 0x18, gameHwnd));
               // 绘制对象名称
               OSImGui::OSImGui::get().StrokeText(objTypeName, Vec2(coor.x,coor.y), ImColor(255,255,0,255));
               // 输出对象地址
               std::string hex_str = std::format("0x{:x}", objAddr);
               OSImGui::OSImGui::get().StrokeText(hex_str, Vec2(coor.x, coor.y - 20), ImColor(255, 255, 0, 255));
            }

        }

    }

}



// 外部绘制测试
void DrawCallBack()
{

    ImGui::Begin("Menu");
    {
        ImGui::Text("This is a text.");


        if (ImGui::Button("Quit"))
        {
            // 使用Gui.Quit()退出程序或者卸载DLL。
            Gui.Quit();
            //...
        }
    }ImGui::End();

    
    traverseData();

}

// 遍历背包
void getTraverseBackpack() {
    uint64_t world = ReadMemory<uint64_t>(gWorld, gameHwnd);
    uint64_t py1 =  ReadMemory<uint64_t>(world + 0x198,gameHwnd);
    uint64_t py2 =  ReadMemory<uint64_t>(py1 + 0x38,gameHwnd);
    uint64_t py3 =  ReadMemory<uint64_t>(py2 + 0x0,gameHwnd);
    uint64_t py4 =  ReadMemory<uint64_t>(py3 + 0x30,gameHwnd);
    uint64_t py5 =  ReadMemory<uint64_t>(py4 + 0x2d8,gameHwnd);
    uint64_t py6 =  ReadMemory<uint64_t>(py5 + 0xa08,gameHwnd);
    uint64_t inventoryComponent =  ReadMemory<uint64_t>(py6 + 0x260,gameHwnd); 
    uint64_t categories =  ReadMemory<uint64_t>(inventoryComponent + 0x120,gameHwnd); //TArray
    int count =  ReadMemory<int>(inventoryComponent + 0x128,gameHwnd); //TArray 当前
    int max =  ReadMemory<int>(inventoryComponent + 0x12c,gameHwnd); //TArray   最大

    // 遍历背包物品种类 InventoryCategoryContainer
    // bool bInfiniteSize 0x30 背包无限大小
    // struct InventoryCategorySlots 0x98
    // -- > 
    for (int i = 0; i < count; i++) {
        uint64_t item = ReadMemory<uint64_t>(categories + (i * 0x8), gameHwnd);
        printf("0x%p  %s\n", item,GetName(ReadMemory<uint64_t>(item + 0x18, gameHwnd)).c_str());
        // struct InventoryCategorySlots - > TArray InventoryCategorySlot
        uint64_t InventoryCategorySlot = ReadMemory<uint64_t>(item + 0x98 + 0x108, gameHwnd); // TArray
        uint32_t count = ReadMemory<uint32_t>(item + 0x1a8, gameHwnd); // TArray
        uint32_t Stride = 0x28; // 结构体大小
        for (int i = 0; i < count; i++) {
            // 遍历背包类别物品
            // ItemActor* item; // +0x10 [0x8]
            // ItemActor* oldItem; // +0x18 [0x8]
            // int32_t count; // +0x20 [0x4]
            // 2. 根据偏移 0x10 读取 ItemActor 指针
            // 1. 计算当前结构体的起始位置
            uint64_t CurrentSlotAddr = InventoryCategorySlot + (i * Stride);
            uint64_t ItemActorPtr = ReadMemory<uint64_t>(CurrentSlotAddr + 0x10, gameHwnd);
            uint32_t ItemActorCount = ReadMemory<uint64_t>(CurrentSlotAddr + 0x20, gameHwnd);
            printf("    0x%p  %s // %d\n", ItemActorPtr,GetName(ReadMemory<uint64_t>(ItemActorPtr + 0x18, gameHwnd)).c_str(), ItemActorCount);
        }
        
        
        

    }


    printf("categories:%llp\n",py6);
     
}


int main() {
	
    if (init())
        std::cout << "初始化完成！\n";
    else
        return 0;

    getTraverseBackpack();


    
    // 遍历World
    //traverseData();
    // 外部模式
    try {

        Gui.AttachAnotherWindow("Dead Island 2  ", "UnrealWindow", DrawCallBack);
        
    }
    catch (OSImGui::OSException& e)
    {
        std::cout << e.what() << std::endl;
    }
    
    return 0;


    
}