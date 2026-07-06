#pragma once
#include <windows.h>
#include <iostream>
#include <string>
#include <tlhelp32.h>


void ExplainMemoryPage(const MEMORY_BASIC_INFORMATION& mbi);
