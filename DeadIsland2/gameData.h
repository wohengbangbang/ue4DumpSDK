#pragma once
#include <windows.h>

// 游戏窗口句柄
extern HWND gameWindowHwnd;
// 游戏句柄
extern HANDLE gameHwnd;
// 游戏进程id
extern DWORD gamePid;
// 游戏窗口高
extern int height;
// 游戏窗口宽
extern int width;
// 视图矩阵
extern float viewMatrix[16];  

// 窗口位置信息
extern RECT windowPosInfo;

// 世界对象坐标
struct Vector
{
	float x;
	float y;
	float z;
};

// 剪辑坐标
struct ClipCoor
{
	float x;
	float y;
	float z;
	float w;
};
