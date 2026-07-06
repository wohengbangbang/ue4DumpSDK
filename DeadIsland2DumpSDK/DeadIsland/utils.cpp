#include "utils.h"


std::string ExplainMemoryProtect(DWORD protect) {
    switch (protect & 0xFF) {  // 只取保护属性位
    case PAGE_NOACCESS:          return "不可访问";
    case PAGE_READONLY:           return "只读";
    case PAGE_READWRITE:          return "读写";
    case PAGE_WRITECOPY:          return "写入时复制";
    case PAGE_EXECUTE:             return "执行";
    case PAGE_EXECUTE_READ:        return "执行/读";
    case PAGE_EXECUTE_READWRITE:   return "执行/读写";
    case PAGE_EXECUTE_WRITECOPY:   return "执行/写入时复制";
    case PAGE_GUARD:                return "保护页";
    case PAGE_NOCACHE:              return "无缓存";
    default:                        return "未知";
    }
}

std::string ExplainMemoryState(DWORD state) {
    switch (state) {
    case MEM_COMMIT:    return "已提交";
    case MEM_RESERVE:   return "已保留";
    case MEM_FREE:      return "空闲";
    case MEM_RELEASE:   return "已释放";
    default:            return "未知";
    }
}

std::string ExplainMemoryType(DWORD type) {
    switch (type) {
    case MEM_IMAGE:     return "映像文件";
    case MEM_MAPPED:    return "映射文件";
    case MEM_PRIVATE:   return "私有内存";
    default:            return "未知";
    }
}



// 综合解释函数
void ExplainMemoryPage(const MEMORY_BASIC_INFORMATION& mbi) {

    printf("=== 内存页属性解释 ===\n");
    printf("基址: %p\n", mbi.BaseAddress);
    printf("大小: %zu 字节 (%.2f KB)\n", mbi.RegionSize, mbi.RegionSize / 1024.0);

    printf("保护属性: 0x%lX - %s\n", mbi.Protect, ExplainMemoryProtect(mbi.Protect).c_str());
    printf("状态: 0x%lX - %s\n", mbi.State, ExplainMemoryState(mbi.State).c_str());
    printf("类型: 0x%lX - %s\n", mbi.Type, ExplainMemoryType(mbi.Type).c_str());
}