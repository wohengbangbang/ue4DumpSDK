#include <iostream>

/*
* 
ObjectProperty 内存布局（64位）:
偏移      | 字段                              | 说明
+0x00     | void* VTable           | 虚表指针
+0x08     | FFieldClass* ClassPrivate | 指向FFieldClass (ObjectProperty)
+0x10     | FFieldVariant Owner    | 所有者
+0x20     | FField* Next           | 下一个属性
+0x28     | FName NamePrivate      | ← 属性本身的名称（字段名）
+0x30     | EObjectFlags FlagsPrivate | 标志
+0x38     | int32_t ArrayDim       | 数组维度
+0x3C     | int32_t ElementSize    | 元素大小
+0x40     | EPropertyFlags PropertyFlags | 属性标志
+0x48     | uint16_t RepIndex      | 复制索引
+0x4A     | TEnumAsByte BlueprintReplicationCondition | 蓝图复制条件
+0x4C     | int32_t Offset_Internal | 在对象中的偏移
+0x50     | FName RepNotifyFunc    | 复制通知函数
+0x58     | FProperty* PropertyLinkNext | 属性链接
+0x60     | FProperty* NextRef     | 下一个引用
+0x68     | FProperty* DestructorLinkNext | 析构链接
+0x70     | FProperty* PostConstructLinkNext | 后构造链接
+0x78     | UClass* PropertyClass  | ← 引用的对象类型（类的名称



FStructProperty 内存布局（64位）:

偏移      | 字段                              | 说明
----------|----------------------------------|------------------
+0x00     | void* VTable                     | 虚表指针
+0x08     | FFieldClass* ClassPrivate        | 指向FFieldClass (StructProperty)
+0x10     | FFieldVariant Owner              | 所有者
+0x20     | FField* Next                     | 下一个属性
+0x28     | FName NamePrivate                | 属性名称（如"MyStruct"）
+0x30     | EObjectFlags FlagsPrivate        | 标志
+0x38     | int32_t ArrayDim                 | 数组维度
+0x3C     | int32_t ElementSize              | 元素大小 = sizeof(结构体)
+0x40     | EPropertyFlags PropertyFlags     | 属性标志
+0x48     | uint16_t RepIndex                | 复制索引
+0x4A     | TEnumAsByte BlueprintReplicationCondition | 蓝图复制条件
+0x4C     | int32_t Offset_Internal          | 在对象中的偏移
+0x50     | FName RepNotifyFunc              | 复制通知函数
+0x58     | FProperty* PropertyLinkNext      | 属性链接
+0x60     | FProperty* NextRef               | 下一个引用
+0x68     | FProperty* DestructorLinkNext    | 析构链接
+0x70     | FProperty* PostConstructLinkNext | 后构造链接
+0x78     | UScriptStruct* Struct            | ← 指向结构体定义（关键！）


*/

// FName结构
struct FName {
    // 指向全局名字表 GNames 的索引
    uint32_t ComparisonIndex;
    // 如果名字重复（如 Actor_1, Actor_2），这里存储序号
    uint32_t Number;

    // 辅助方法：判断是否有效
    bool IsValid() const {
        return ComparisonIndex > 0;
    }
};

// TArray
template<class T>
struct TArray {
    uint64_t Data;      // 0x00: 指向堆内存的数据指针 (T*)
    int32_t ArrayNum;   // 0x08: 当前元素数量
    int32_t ArrayMax;   // 0x0C: 最大容量

    // 辅助方法：读取字符串内容
    std::wstring ToWString() const {
        if (!Data || ArrayNum <= 0) return L"";
        // 这里需要调用你的 ReadMemory 读取 Data 指向的缓冲区
        // 长度为 ArrayNum * sizeof(wchar_t)
        return L""; // 示意逻辑
    }
};

// FString 
typedef TArray<wchar_t> FString;


// FText
struct FText {
    // 0x00: 指向内部 ITextData 的共享引用 (TSharedPtr)
    uint64_t TextData;

    // 后面通常有 8-16 字节的填充或引用计数相关数据
    uint32_t Flags;
    uint32_t Pad;

    /*
       注意：FText 的真实字符串隐藏在 TextData 指针深处。
       通常路径是：TextData -> FTextHistory -> SourceString (FString)
    */
};

template<typename ElementType>
struct TSet {
    // 0x00: 存储实际元素的数组 (包含数据、Hash值、链表索引)
    TArray<ElementType> Elements;

    // 0x10: 哈希表相关数据 (通常是一个指向整型数组的指针)
    // 用于快速定位 Elements 数组中的索引
    uint64_t HashData;

    // 0x18: 散列相关的掩码和数量
    int32_t HashSize;
    int32_t MaxElementIndex; // 最大的有效索引位
};

template<typename KeyType, typename ValueType>
struct TPair {
    KeyType Key;     // 键
    ValueType Value; // 值
};

template<typename KeyType, typename ValueType>
struct TMap {
    // 0x00: TMap 的核心就是一个存储 TPair 的 TSet
    TSet<TPair<KeyType, ValueType>> Pairs;
};

// 对应 UE4 内部的 FWeakObjectPtr 结构
template<typename O>
struct TWeakObjectPtr {
    // 0x00: 对象在全局 GObjects 数组中的索引
    int32_t ObjectIndex;

    // 0x04: 序列号，用于验证对象是否已被销毁并被新对象替换
    int32_t ObjectSerialNumber;

    // 逻辑检查：判断该指针是否可能指向一个对象
    bool IsValid() const {
        return ObjectIndex != -1;
    }
};

template<class TClass>
class TSubclassOf
{
private:
    // 实际上内部只存储一个 UClass 指针
    struct UClass* classPtr;
public:
    // 各种构造函数和操作符重载，用于确保 ClassPtr 必须是 TClass 的子类
    // ...
};


// 基础 UObject 结构
struct UObject {
    void* vtable;                // 0x00
    int32_t ObjectFlags;         // 0x08
    int32_t InternalIndex;       // 0x0C
    struct UClass* ClassPrivate; // 0x10
    FName NamePrivate;           // 0x18
    struct UObject* OuterPrivate;// 0x20
};

// 继承自 UObject -> UField
struct UField : public UObject {
    struct UField* Next;         // 0x28 (用于遍历函数 UFunction)
};

// 继承自 UField -> UStruct (解析变量的核心)
struct UStruct : public UField {
    struct UStruct* SuperStruct;    // 0x30 (父类指针)
    struct UField* Children;        // 0x38 (包含函数、枚举等 UObject 成员)
    struct FField* ChildProperties; // 0x40 (核心：4.25 变量属性链表起点)
    int32_t PropertiesSize;         // 0x48 (类实例占用的内存大小)
    int32_t MinAlignment;           // 0x4C (内存对齐)
    // ... 后续为脚本字节码相关数据
};

// 继承自 UStruct -> UClass
struct UClass : public UStruct {
    // 0x50 - 0xAF 为 UStruct 的剩余数据和 UClass 特有标志
    char pad_01[0x60];
    struct UObject* ClassDefaultObject; // 0xB0 (CDO)
    // ... 后续为接口和配置数据
};