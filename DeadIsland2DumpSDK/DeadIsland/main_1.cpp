/*
    UE4/5 游戏逆向DumpSDK
*/

#include "utils.h"
#include <fstream>
#include <codecvt>  
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <algorithm>
#include <unordered_map>
#include <vector>

HWND gameWindowHwnd;
HANDLE gameHwnd;
DWORD gamePid;


uintptr_t baseAddr = 0;
uintptr_t gName = 0;
// gObject偏移
uintptr_t gObject = 0;

const char* OUTPUT_FILE_ADDR = "FadeoutSDK/Object.h";
const char* OUTPUT_CLASS_FILE = "FadeoutSDK/ObjectClass.h";
const char* OUTPUT_ENUM_FILE = "FadeoutSDK/EnumClass.h";
const char* OUTPUT_STRUCT_FILE = "FadeoutSDK/StructClass.h";

// 首字母小写
inline std::string ToLowerFirstLetter(std::string str) {
    if (!str.empty()) {
        // 使用 (unsigned char) 强制转换是处理非 ASCII 字符的安全做法
        str[0] = std::tolower(static_cast<unsigned char>(str[0]));
    }
    return str;
}


// 带返回值的版本（推荐）
BOOL GetMemoryPageInfoEx(HANDLE hProcess, LPCVOID lpAddress, MEMORY_BASIC_INFORMATION& mbi) {
    return ::VirtualQueryEx(hProcess, lpAddress, &mbi, sizeof(mbi)) == sizeof(mbi);
}

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


// 初始化
bool init() {
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

    baseAddr = GetModuleBase(gamePid, exeName);

    if (!baseAddr) {
        std::cout << "游戏模块基地址获取失败\n";
        return false;
    }

    // 获取GName地址
    gName = baseAddr + 0x6CF4C90;
    if (!gName) {
        std::cout << "GName获取失败\n";
        return false;
    }

    // 获取gObject
    gObject = baseAddr + 0x6D28680;
    if (!gObject) {
        std::cout << "GObject获取失败\n";
        return false;
    }

    printf("[+] 成功附加到进程Pid = %d\n", gamePid);
    printf("[+] 模块基址 = 0x%llX\n", baseAddr);
    printf("[+] GObject = 0x%llX\n\n", gObject);

    return true;
}

// 安全内存读取
bool ReadMemory(uintptr_t addr, void* buff, size_t size) {
    SIZE_T bytesRead;
    return ::ReadProcessMemory(gameHwnd, (LPCVOID)addr, buff, size, &bytesRead) && bytesRead == size;
}

template<typename T>
T ReadMemory(uintptr_t addr) {
    T value;
    if (!ReadMemory(addr, &value, sizeof(T))) {
        memset(&value, 0, sizeof(T));
        //std::cout << "Error:" << ::GetLastError() << std::endl;
    }
    return value;
}

struct FNameEntryHeader {
    uint16_t bIsWide : 1;
    //static constexpr uint32_t ProbeHashBits = 5;
    uint16_t LowercaseProbHash : 5;
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
std::string GetName(int NameId) {
    //return *reinterpret_cast<FNameEntry*> (gName[NameId >>16] + 2 * NameId&&65535); // ue引擎GName算法（原版）

    if (NameId == 0) return "";
    uintptr_t BlockPtrAddr = gName + (NameId >> 16) * 8;
    uint64_t BlockPtr = ReadMemory<uint64_t>(BlockPtrAddr);

    if (!BlockPtr) return "";

    uintptr_t NameEntryAddr = BlockPtr + 2 * (NameId & 0xFFFF); // 65535

    FNameEntry info = ReadMemory<FNameEntry>(NameEntryAddr);

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

// 弱引用对象查找
uint64_t GetWeakObject(uint32_t ObjectIndex, int32_t ExpectedSerialNumber) {
    // 1. 基本参数计算
    uint32_t ChunkIndex = ObjectIndex / 65536;
    uint32_t WithinChunkIndex = ObjectIndex % 65536;
    const uint32_t ItemSize = 0x18; // 绝大多数 UE4/UE5 版本的标准大小

    // 2. 定位 Chunk 数组地址 (Objects** ptr)
    // 假设 gObjectBase 是 FChunkedFixedUObjectArray 的首地址
    uint64_t ChunkArrayPtr = ReadMemory<uint64_t>(gObject);

    // 3. 获取特定 Chunk 的基址
    uint64_t SpecificChunkPtr = ReadMemory<uint64_t>(ChunkArrayPtr + ChunkIndex * 8);
    if (!SpecificChunkPtr) return 0;

    // 4. 定位到具体的 FUObjectItem 结构
    uint64_t ItemAddress = SpecificChunkPtr + (WithinChunkIndex * ItemSize);

    // 5. 验证 SerialNumber (偏移通常在 +0x10)
    int32_t ActualSerialNumber = ReadMemory<int32_t>(ItemAddress + 0x10);

    // 如果序列号对不上，说明对象已失效（被销毁或替换）
    if (ActualSerialNumber != ExpectedSerialNumber) {
        return 0;
    }

    // 6. 返回真实的 UObject 指针 (偏移在 +0x00)
    return ReadMemory<uint64_t>(ItemAddress);
}

// 获取所有GObject对象
uint64_t GetAllGObject(size_t id) {
    /*    // 当前元素数量
        //uint32_t nowItmeNum = ReadMemory<uint32_t>(gObject + 0x14);
        // 当前块数量
        //uint32_t nowItemBlock = ReadMemory<uint32_t>(gObject + 0x1C);

        // 读取指针

        //uint64_t Objects = ReadMemory<uint64_t>(ObjObjects);
        //uint64_t Object = ReadMemory<uint64_t>(Objects);

        //printf("ObjObjects = %p\n", ObjObjects);
        //printf("Objects = %p\n", Objects);
        //printf("Object = %p\n", Object);


        //printf("nowItmeNum = %d\n", nowItmeNum);
        //printf("nowItemBlock = %d\n", nowItemBlock);

        //std::cout << "\n\n======== 遍历对象 ========\n\n";*/

    size_t i = 0;
    for (i; id > 65536; i++) {
        // 这里还原i的值
        id -= 65536;
    }

    // 当第一个对象块中的所有元素遍历完后才会遍历下一个块
    uint64_t ObjObjects = ReadMemory<uint64_t>(gObject) + 8 * i;
    uint64_t Objects = ReadMemory<uint64_t>(ObjObjects) + id * 0x18;
    uint64_t Object = ReadMemory<uint64_t>(Objects);

    return Object;
    //printf("%llp\n", Object);

}
// 名称id
std::string GetNameForObject(uint64_t Object) {
    if (!Object) return "";
    uint32_t nameId = ReadMemory<uint32_t>(Object + 0x18);
    return GetName(nameId);
}
// 通过指定偏移获取名称
std::string GetNameForObject(uint64_t Object, DWORD offset) {
    if (!Object) return "";
    uint32_t nameId = ReadMemory<uint32_t>(Object + offset);
    return GetName(nameId);
}
// class指针判断当前对象是什么类型
uint64_t GetClass(uint64_t Object) {
    return Object ? ReadMemory<uint64_t>(Object + 0x10) : 0;
}
// 父类指针
uint64_t GetOuter(uint64_t Object) {
    return Object ? ReadMemory<uint64_t>(Object + 0x20) : 0;
}
// 父类指向的地址
uint64_t GetSuperClass(uint64_t Object) {
    return Object ? ReadMemory<uint64_t>(Object + 0x40) : 0;
}
// 获取成员
uint64_t GetProperties(uint64_t Object) {
    return Object ? ReadMemory<uint64_t>(Object + 0x50) : 0;

}
// 获取下一成员
uint64_t GetNextProperties(uint64_t Object) {
    return Object ? ReadMemory<uint64_t>(Object + 0x20) : 0;
}
// 获取字段类属性属于什么类型
uint64_t GetFieldClass(uint64_t Object)
{
    return ReadMemory<uint64_t>(Object + 0x8);
}

// 获取方法地址
uint64_t GetFunction(uint64_t Object) {
    return ReadMemory<uint64_t>(Object + 0xd8);
}

std::string GetType(uint64_t Object);
std::string dumpObject(uint64_t Object);

bool IsFunctionObject(uint64_t Object) {
    if (!Object) return false;

    for (uint64_t sObj = GetClass(Object); sObj; sObj = GetSuperClass(sObj)) {
        std::string className = GetNameForObject(sObj);
        if (className == "Function" || className == "DelegateFunction") {
            return true;
        }
    }

    return false;
}

std::string BuildFunctionSignature(uint64_t FunctionObject) {
    std::string functionName = GetNameForObject(FunctionObject);
    if (functionName.empty()) {
        functionName = "UnknownFunction";
    }

    std::string returnType = "void";
    std::vector<std::string> params;

    for (uint64_t prop = GetProperties(FunctionObject); prop; prop = GetNextProperties(prop)) {
        std::string propName = GetNameForObject(prop, 0x28);
        std::string propType = GetType(prop);

        if (propName.empty()) {
            continue;
        }

        if (propName == "ReturnValue") {
            returnType = propType;
            continue;
        }

        if (propName == "self" || propName == "ReturnValue") {
            continue;
        }

        params.emplace_back(propType + " " + ToLowerFirstLetter(propName));
    }

    std::ostringstream signature;
    signature << returnType << " " << functionName << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) signature << ", ";
        signature << params[i];
    }
    signature << ")";

    return signature.str();
}

std::string DumpClassFunction(uint64_t FunctionObject) {
    if (!FunctionObject) return "";

    std::ostringstream line;
    line << "    " << BuildFunctionSignature(FunctionObject) << ";";

    uint64_t nativeFunc = GetFunction(FunctionObject);
    if (nativeFunc) {
        line << " // Func: 0x" << std::hex << std::uppercase << nativeFunc;
    }

    line << " // " << dumpObject(FunctionObject);
    line << "\n";

    return line.str();
}


// 获取类型
std::string GetType(uint64_t Object) {
    // 0x8 FFidle
    if (!Object) return "Unknown";
    // 获取字段类型名称
    std::string typeName = GetName(ReadMemory<int>(GetFieldClass(Object)));


    if (typeName == "IntProperty") return "int32_t";
    if (typeName == "Int64Property") return "int64_t";
    if (typeName == "FloatProperty") return "float";
    if (typeName == "DoubleProperty") return "double";
    if (typeName == "BoolProperty") return "bool";
    if (typeName == "Int8Property") return "int8_t";


    if (typeName == "ObjectProperty") {
        // 0x78 在 UObjectProperty 中是指向 PropertyClass (UClass*) 的指针
        uint64_t PropertyClass = ReadMemory<uint64_t>(Object + 0x78);
        if (PropertyClass) {
            // 应该获取这个 Class 的名字，比如 "AActor"
            return GetNameForObject(PropertyClass) + "*";
        }
        return "UObject*";
    }

    if (typeName == "StructProperty")
        return "struct " + GetNameForObject(ReadMemory<uint64_t>(Object + 0x78));

    // 解析弱引用
    if (typeName == "WeakObjectProperty") {
        // 0x78 处存放的是该属性允许指向的类类型 (UClass*)
        uint64_t PropertyClass = ReadMemory<uint64_t>(Object + 0x78);
        if (PropertyClass) {
            return "TWeakObjectPtr<struct " + GetNameForObject(PropertyClass) + ">";
        }
        return "TWeakObjectPtr<struct UObject>";
    }

    // NameProperty
    if (typeName == "NameProperty") {
        return "FName";
    }

    // EnumProperty 
    if (typeName == "EnumProperty") {
        // 0x78 处存储的是 UEnum 指针
        uint64_t EnumPtr = ReadMemory<uint64_t>(Object + 0x80);
        return "enum " + GetNameForObject(EnumPtr);
    }

    // StrProperty
    if (typeName == "StrProperty") {
        return "FString";
    }

    // TextProperty
    if (typeName == "TextProperty") {
        return "FText";
    }

    // ByteProperty
    if (typeName == "ByteProperty") {
        // 可能是基础 byte，也可能是具体的 Enum
        uint64_t EnumPtr = ReadMemory<uint64_t>(Object + 0x70);
        if (EnumPtr) return "enum " + GetNameForObject(EnumPtr);
        return "uint8_t";
    }
    
    // ArrayProperty (TArray<T>)
    if (typeName == "ArrayProperty") {
        // 0x78 处存储的是 InnerProperty (数组成员的属性类型)
        uint64_t InnerProp = ReadMemory<uint64_t>(Object + 0x78);
        if (InnerProp) {
            return "TArray<" + GetType(InnerProp) + ">";
        }
        return "TArray<Unknown>";
    }

    // MapProperty (TMap<Key, Value>)
    if (typeName == "MapProperty") {
        // 0x78 是 KeyProp (键属性)
        uint64_t KeyProp = ReadMemory<uint64_t>(Object + 0x78);
        // 0x80 是 ValueProp (值属性)
        uint64_t ValueProp = ReadMemory<uint64_t>(Object + 0x80);

        std::string keyType = "UnknownKey";
        std::string valueType = "UnknownValue";

        if (KeyProp) keyType = GetType(KeyProp);
        if (ValueProp) valueType = GetType(ValueProp);

        return "TMap<" + keyType + ", " + valueType + ">";
    }

    // SetProperty
    if (typeName == "SetProperty") {
        uint64_t ElementProp = ReadMemory<uint64_t>(Object + 0x78);
        return "TSet<" + GetType(ElementProp) + ">";
    }

    //MulticastInlineDelegateProperty

    // ClassProperty
    if (typeName == "ClassProperty") {
        // 0x78 是该属性本身的类型（通常是 UClass）
        // uint64_t PropertyClass = ReadMemory<uint64_t>(Object + 0x78); 
        // 0x80 才是 TSubclassOf<T> 括号里的那个 T
        uint64_t MetaClassPtr = ReadMemory<uint64_t>(Object + 0x80);

        if (MetaClassPtr) {
            return "TSubclassOf<class " + GetNameForObject(MetaClassPtr) + ">";
        }
        return "class UClass*"; // 如果没有 MetaClass，默认就是类指针
    }


    return typeName;

}



std::string dumpObject(uint64_t Object) {

    if (!Object) return "";

    std::string name = GetNameForObject(Object);

    if (name.empty() || name == "None") return "void";

    int depth = 0;
    // 获取父目录 直到0x20偏移的地址为空
    uint64_t outer = GetOuter(Object);
    while (outer && depth < 10) {

        std::string outerName = GetNameForObject(outer);
        if (!outerName.empty() && outerName != "None")
            name = outerName + "." + name;
        outer = GetOuter(outer); depth++;
    };

    uint64_t CLASS = GetClass(Object);
    // 获取类型名称
    std::string className = GetNameForObject(CLASS);
    if (className.empty()) className = "UnknownClass";


    // 拼接class + 名称 + 父目录名称
    return className + " " + name;
}

// 辅助函数：将std::wstring转换为UTF-8编码的std::string
std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};

    int size_needed = WideCharToMultiByte(CP_UTF8, 0,
        wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);

    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0,
        wstr.c_str(), (int)wstr.size(), &result[0], size_needed, NULL, NULL);

    return result;
}

// dump class
void dumpToFile(const std::string str, std::string fileName) {


    if (str.empty()) return;

    // 创建目录
    ::CreateDirectory(L"FadeoutSDK", NULL);

    // 以追加模式打开文件
    std::ofstream file(fileName, std::ios::out | std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        printf("[-] 无法打开文件：%s\n", fileName);
        return;
    }

    // 检查是否需要写入BOM
    file.seekp(0, std::ios::end);
    if (file.tellp() == 0) {
        char bom[] = { (char)0xEF, (char)0xBB, (char)0xBF };
        file.write(bom, sizeof(bom));
    }

    // 写入内容
    file.write(str.c_str(), str.size());
}

// dump sdk info
void DumpSDKAddr() {
    uint32_t nowItmeNum = ReadMemory<uint32_t>(gObject + 0x14);  // NumElements

    if (nowItmeNum == 0 || nowItmeNum >= 1000000) return;

    // 在当前工作目录下创建FadeoutSDK文件夹
    ::CreateDirectory(L"FadeoutSDK", NULL);

    // 打开文件（二进制模式）
    std::ofstream file(OUTPUT_FILE_ADDR, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        printf("[-] 无法创建文件：%s\n", OUTPUT_FILE_ADDR);
        return;
    }

    // 写入UTF-8 BOM (可选，但推荐以便其他程序正确识别)
    char bom[] = { (char)0xEF, (char)0xBB, (char)0xBF };
    file.write(bom, sizeof(bom));

    printf("[+] 开始扫描 %d 个对象...\n", nowItmeNum);
    int validCount = 0;

    for (int i = 0; i < nowItmeNum; i++) {
        if (i % 10000 == 0) {
            printf("[*] 已扫描 %d / %d\n", i, nowItmeNum);
        }

        uint64_t Object = GetAllGObject(i);
        std::string desc = dumpObject(Object);

        if (!desc.empty()) {
            // 构建宽字符串行
            std::wstringstream str;

            str << L"[0x"
                << std::hex << std::uppercase << std::setfill(L'0') << std::setw(12)
                << Object
                << L"]"
                << " "
                << std::wstring(desc.begin(), desc.end()); // 假设desc是ASCII/UTF-8，转换为宽字符

            // 转换为UTF-8并写入文件
            std::string utf8_line = WideToUtf8(str.str());
            file.write(utf8_line.c_str(), utf8_line.size());
            file.write("\n", 1); // 添加换行符

            validCount++;
        }
    }

    file.close();
    printf("[+] 扫描完成！有效对象: %d / %d\n", validCount, nowItmeNum);
    printf("[+] 已保存到：%s\n", OUTPUT_FILE_ADDR);

}

// 拼接 DumpClass 并写入到文件
std::string DumpClass(uint64_t Object, const std::vector<uint64_t>& functions) {
    // 第一个类成员的位置 + 0x50
    // 第二个成员的位置   + 0x20 + 0x20 ...
    // 第二个成员的名称   + 0x28
    // 大小             + 0x3c
    // 偏移             + 0x4c
    // 类的大小 Object   + 0x58 

    int size = ReadMemory<int>(Object + 0x58);

    std::string className = "\n\n// " + dumpObject(Object) + "\n";
    className += "// classSize: " + std::to_string(size) + "\n";
    className += "class " + GetNameForObject(Object);

    // 查看当前类所继承的父类
    uint64_t superClass = GetSuperClass(Object);
    if (superClass) {
        className += " : public " + GetNameForObject(superClass);
    }

    className += "\n{\n";

    for (uint64_t obj = GetProperties(Object); obj; obj = GetNextProperties(obj)) {
        className += "    " + GetType(obj) + " " + ToLowerFirstLetter(GetNameForObject(obj, 0x28)) + ";";
        std::stringstream hexStr;
        std::stringstream hexStr2;

        int offset = ReadMemory<int>(obj + 0x4C);// 偏移
        int size = ReadMemory<int>(obj + 0x3C);// 大小

        hexStr << std::hex << offset;
        className += " // +0x" + hexStr.str();
        hexStr2 << std::hex << size;
        className += " [0x" + hexStr2.str() + "]" + "\n";
    }

    if (!functions.empty()) {
        std::vector<uint64_t> sortedFunctions = functions;
        std::sort(sortedFunctions.begin(), sortedFunctions.end(), [](uint64_t lhs, uint64_t rhs) {
            return GetNameForObject(lhs, 0x28) < GetNameForObject(rhs, 0x28);
            });

        className += "\n";
        className += "    // Functions\n";
        for (uint64_t func : sortedFunctions) {
            className += DumpClassFunction(func);
        }
    }

    className += "};\n";
    return className;
}

// dump Enum
// + 0x40 枚举偏移
// + 0x48 有多少个值 循环次数
// + 0xi * 0x10 枚举名称 + 0x8 枚举值
std::string DumpClassEnum(uint64_t Object) {
    std::string enumName = "\n\n// " + dumpObject(Object) + "\n";
    enumName += "enum class " + GetNameForObject(Object);
    enumName += "\n{\n";
    uint64_t enumPointer = ReadMemory<uint64_t>(Object + 0x40); // 枚举指针
    int enumCount = ReadMemory<int>(Object + 0x48);
    for (int i = 0; i < enumCount; i++) {
        uint32_t nameId = ReadMemory<uint32_t>(enumPointer + i * 0x10);
        uint32_t value = ReadMemory<uint32_t>(enumPointer + (i * 0x10) + 0x8);

        std::string enumNameStr = GetName(nameId);
        size_t pos = enumNameStr.rfind(":");
        if (pos != std::string::npos) enumNameStr = enumNameStr.substr(pos + 1);
        enumNameStr = "    " + enumNameStr;
        enumName += enumNameStr + "\t\t\t\t = " + std::to_string(value) + ",\n";
    }
    enumName += "};\n";
    return enumName;
    //printf("%s\n", enumName.c_str());
}

// dump Struct
std::string DumpClassStruct(uint64_t Object) {
    // 0x18 结构体名称
    // 0x40 父结构指针
    // 0x50 指向第一个结构第一个成员的指针
    // 0x58 大小
    std::string structName = "\n\n// " + dumpObject(Object) + "\n";
    structName += "struct " + GetNameForObject(Object);

    uint64_t superstrcut = GetSuperClass(Object);
    if (superstrcut) {
        structName += " : public " + GetNameForObject(superstrcut);
        structName += "\n{\n";
        for (uint64_t obj = GetProperties(Object); obj; obj = GetNextProperties(obj)) {
            structName += "    " + GetType(obj) + " " + ToLowerFirstLetter(GetNameForObject(obj, 0x28)) + ";";
            std::stringstream hexStr;
            std::stringstream hexStr2;

            int offset = ReadMemory<int>(obj + 0x4C);// 偏移
            int size = ReadMemory<int>(obj + 0x3C);// 大小

            hexStr << std::hex << offset;
            structName += " // +0x" + hexStr.str();
            hexStr2 << std::hex << size;
            structName += " [0x" + hexStr2.str() + "]" + "\n";
        }
        structName += "};\n";
        return structName;
    }
    else {
        structName += "\n{\n";
        for (uint64_t obj = GetProperties(Object); obj; obj = GetNextProperties(obj)) {
            structName += "    " + GetType(obj) + " " + ToLowerFirstLetter(GetNameForObject(obj, 0x28)) + ";";
            std::stringstream hexStr;
            std::stringstream hexStr2;

            int offset = ReadMemory<int>(obj + 0x4C);// 偏移
            int size = ReadMemory<int>(obj + 0x3C);// 大小

            hexStr << std::hex << offset;
            structName += " // +0x" + hexStr.str();
            hexStr2 << std::hex << size;
            structName += " [0x" + hexStr2.str() + "]" + "\n";
        }
        structName += "};\n";
        return structName;
    }

}

struct dumpTArray {
    uint16_t Data = 0x0;    // 指向存放实际数据的堆内存首地址
    uint16_t ArrayNum = 0x8;    // 	数组当前元素的实际个数
    uint16_t ArrayMax = 0xc;    // 	数组当前分配的最大容量 
};

struct dumpTUObjectArray {
    uint16_t Objects = 0x0;
    uint16_t NumChunks = 0x1c;
    uint16_t UnmElements = 0x14;
};

struct dumpFUObjectItem {
    uint16_t Class = 0x10;
    uint16_t Name = 0x18;
    uint16_t Outer = 0x20;
};

// 对象class
struct dumpUObject {
    uint16_t Class = 0x10;  // 当前属于什么类 class | struct | enmu ... 父类
    uint16_t Name = 0x18;   // 名称
    uint16_t Outer = 0x40; // 表示这个类继承于哪个类
    uint16_t ChildProperties = 0x50; // 第一个成员的指针 --> 0x20下一个成员 FField
    uint16_t PropertiesSize = 0x58; // 对象大小
    uint16_t Class2 = 0x78; // 引用对象的名称
};

// 结构体stcuct
struct dumpStruct {
    uint16_t SuperStruct = 0x40; // 在CoreUObject.Struct下，指向应为CoreUObject.Field的地址 父目录 它继承于谁
    uint16_t ChildProperties = 0x50; // 指向结构体第一个成员的指针
    uint16_t PropertiesSize = 0x58; // 结构体大小
    uint16_t Class2 = 0x78; // 引用对象的名称
};

// 枚举结构
struct dumpUEnum {
    uint16_t Name = 0x0;
    uint16_t Value = 0x8;
    uint16_t Size = 0x10;
    uint16_t Names = 0x40;
};

// 方法所在的偏移
struct dumpFunction {
    uint16_t Func = 0xd8;
};

// 保存指针下一个成员的指针
struct dumpFField {
    uint16_t Class = 0x8;
    uint16_t Next = 0x20;   // 指向下一个成员的指针
    uint16_t Name = 0x28; // FName
};

// 保存字段的属性，大小 偏移等
struct dumpFProperty {
    uint16_t ElementSize = 0x3c; // 当前元素的大小
    uint16_t Offset = 0x4c;      // 当前元素的偏移
    uint16_t size = 0x78c;     // 当前大小
};

struct dumpWeakObjectProperty
{
    int32_t ObjectIndex = 0x0; // 对象在全局对象池（GObjects）中的索引
    int32_t ObjectSerialNumber = 0x4; // 序列号，用于验证对象是否还是原来的那个（防止索引重用）
};

struct dumpClassFField {
    uint16_t name = 0x0; // 成员的类型
};


struct FByteProperty {
    uint16_t UEnum = 0x78;
};

struct FEnumProperty {
    uint16_t UEnum = 0x80;
};




// dumpmain
void DumpSDK() {
    uint32_t nowItmeNum = ReadMemory<uint32_t>(gObject + 0x14);  // NumElements
    int count = 0;

    std::unordered_map<uint64_t, std::vector<uint64_t>> functionsByOuter;
    std::vector<uint64_t> classObjects;
    std::vector<uint64_t> structObjects;
    std::vector<uint64_t> enumObjects;

    // 在最开始写入头文件
    ::CreateDirectory(L"FadeoutSDK", NULL);
    // 以覆盖模式写入
    std::ofstream file(OUTPUT_CLASS_FILE, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        printf("[-] 无法打开文件：%s\n", OUTPUT_CLASS_FILE);
        return;
    }

    // 检查是否需要写入BOM UTF-8
    file.seekp(0, std::ios::end);
    if (file.tellp() == 0) {
        char bom[] = { (char)0xEF, (char)0xBB, (char)0xBF };
        file.write(bom, sizeof(bom));
    }
    std::string strInclude = "#include\"EnumClass.h\"\n#include\"StructClass.h\"\n#include\"../ueStruct.h\"\n#include<iostream>\n\n";
    // 写入内容
    file.write(strInclude.c_str(), strInclude.size());
    file.close();


    for (size_t i = 0; i < nowItmeNum; i++) {
        uint64_t Object = GetAllGObject(i);

        if (!Object) continue;

        if (IsFunctionObject(Object)) {
            uint64_t outer = GetOuter(Object);
            if (outer) {
                functionsByOuter[outer].push_back(Object);
            }
        }

        uint64_t sObj = GetClass(Object);
        bool matchedClass = false;
        bool matchedStruct = false;
        bool matchedEnum = false;

        while (sObj) {
            if (GetNameForObject(sObj) == "Enum") {
                matchedEnum = true;
                break;
            }
            if (GetNameForObject(sObj) == "Class") {
                matchedClass = true;
                break;
            }
            if (GetNameForObject(sObj) == "ScriptStruct") {
                matchedStruct = true;
                break;
            }

            sObj = GetSuperClass(sObj);
        }

        if (matchedEnum) {
            enumObjects.push_back(Object);
        }
        else if (matchedClass) {
            classObjects.push_back(Object);
        }
        else if (matchedStruct) {
            structObjects.push_back(Object);
        }
    }

    static const std::vector<uint64_t> emptyFunctions;

    for (uint64_t Object : enumObjects) {
        dumpToFile(DumpClassEnum(Object), OUTPUT_ENUM_FILE);
    }

    for (uint64_t Object : classObjects) {
        auto it = functionsByOuter.find(Object);
        if (it != functionsByOuter.end()) {
            dumpToFile(DumpClass(Object, it->second), OUTPUT_CLASS_FILE);
        }
        else {
            dumpToFile(DumpClass(Object, emptyFunctions), OUTPUT_CLASS_FILE);
        }
    }

    for (uint64_t Object : structObjects) {
        dumpToFile(DumpClassStruct(Object), OUTPUT_STRUCT_FILE);
    }
}



// 通过class名称获取class 偏移
uint64_t ByNameGetSObject(std::string name) {
    uint32_t nowItmeNum = ReadMemory<uint32_t>(gObject + 0x14);
    for (size_t i = 0; i < nowItmeNum; i++) {
        uint64_t Object = GetAllGObject(i);
        if (dumpObject(Object) == name)
            return Object;
    }
    return 0;
}

bool Fileterobjects(uint64_t Object) {
    // 获取class 偏移
    static uint64_t obj = ByNameGetSObject("Class Script/CoreUObject.Class");
    for (uint64_t sObj = GetClass(Object); sObj; sObj = GetClass(sObj)) {
        if (sObj == obj) return true;
    }
    return false;
}


int main() {
    SetConsoleTitle(L"Ue4/5 Dump SDK");
    // 设置控制台输出编码为UTF-8
    SetConsoleOutputCP(CP_UTF8);
    // 设置控制台输入编码为UTF-8  
    SetConsoleCP(CP_UTF8);
    // 初始化
    init();

    // 要dump的格式[地址] 类型 父目录 名称
    //DumpSDKAddr();

    //printf("%s\n",GetNameForObject(0x256b3a60000));
    // 在dump 之前先删除文件
/*    try {
        if (std::filesystem::remove(OUTPUTCLASS_FILE)) {
            std::cout << "文件删除成功: " << OUTPUTCLASS_FILE << std::endl;
        }
        else {
            std::cout << "文件不存在: " << OUTPUTCLASS_FILE << std::endl;
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cout << "删除失败: " << e.what() << std::endl;
    }*/
    //DumpSDK();
    printf("%s\n", GetName(96470).c_str());
    //printf("%p\n", GetAllGObject(266590));
    system("pause");

    return 0;
}
