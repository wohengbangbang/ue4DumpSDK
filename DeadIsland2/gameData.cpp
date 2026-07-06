#include "gameData.h"




// 游戏窗口句柄
HWND gameWindowHwnd = 0;
// 游戏句柄
HANDLE gameHwnd = 0;
// 游戏进程id
DWORD gamePid =0;

// 游戏窗口高
int height = 0;
// 游戏窗口宽
int width = 0;
// 视图矩阵
float viewMatrix[16] = { 0 };
// 游戏窗口位置信息
RECT windowPosInfo{};


