// dllmain.cpp : Definiert den Einstiegspunkt für die DLL-Anwendung.
#include "pch.h"

struct MemoryHeader
{
    MemoryHeader* prev;
    MemoryHeader* next;
};

struct MemoryBlock
{
    MemoryBlock* prev;
    MemoryBlock* next;
    size_t size;
    union
    {
        size_t flags;
        struct {
            unsigned char ucPaddingWide[7];
            unsigned char ucPaddingSmall : 6;
            bool bInUse : 1;
            bool bFreeCriticalSection : 1;
        };
    };
    MemoryHeader* header;
    MemoryBlock* thisBlock;
};

CRITICAL_SECTION mem_cs;
MemoryHeader* first;
size_t dwPageSize;

struct dyncode_r10 {
    unsigned __int64 argflags;
    void* functionDest;
    unsigned __int64 arg1;
    unsigned __int64 argcount;
    unsigned __int64 arg2;
    void* freeAddress;
    uint8_t codeBuffer[22];
    bool bHasCriticalSection;
    bool bReleaseCsAfterArguments;
};

struct dyncode_r10_cs
{
    dyncode_r10 r10;
    CRITICAL_SECTION cs;
};

struct redirect_r10 {
    volatile void* dstfunction;
    void* freeAddress;
    uint8_t codeBuffer[22];
};

void InitMemSystem() {
    InitializeCriticalSection(&mem_cs);
    first = NULL;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    dwPageSize = si.dwPageSize;
}

void uninitMemSystem() {
    DeleteCriticalSection(&mem_cs);
    while (first)
    {
        MemoryHeader* next = first->next;
        VirtualFree(first, 0, MEM_RELEASE);
        first = next;
    }
}

MemoryBlock* findMemoryBlock(void* addr) {
    return *((MemoryBlock**)((char*)addr - 8));
}

size_t getRealAllocationBaseOffset(void* addr) {
    return (char*)addr - (char*)(findMemoryBlock(addr) + 1);
}

DLLEXPORT(size_t) asmMsize(void* addr) {
    MemoryBlock* block = findMemoryBlock(addr);
    unsigned long long ullOffset = getRealAllocationBaseOffset(addr);
    return block->size - ullOffset;
}

DLLEXPORT(void) asmInitialize() {
    InitMemSystem();
}

DLLEXPORT(void) asmUninitialize() {
    uninitMemSystem();
}

size_t roundToNext(size_t val, size_t multiple) {
    if (val % multiple) {
        val -= (val % multiple);
        val += multiple;
    }
    return val;
}

void ActivateBlock(MemoryBlock* block) {
    block->bInUse = true;
    block->bFreeCriticalSection = false;
}

void DisableBlock(MemoryBlock* block) {
    block->bInUse = false;
    if (block->bFreeCriticalSection)DeleteCriticalSection(&((dyncode_r10_cs*)(block + 1))->cs);
}

DLLEXPORT(void) DestroyArgboundAt(void* addr) {
    dyncode_r10_cs* csptr = (dyncode_r10_cs*)((char*)addr - offsetof(dyncode_r10, codeBuffer));
    if (csptr->r10.bHasCriticalSection)DeleteCriticalSection(&csptr->cs);
}

MemoryBlock* createBlockAt(void* base, MemoryBlock* prev, MemoryBlock* next, MemoryHeader* header, size_t size) {
    MemoryBlock* block = (MemoryBlock*)base;
    block->prev = prev;
    block->next = next;
    block->size = size;
    block->header = header;
    block->bInUse = false;
    block->thisBlock = block;
    if (prev)prev->next = block;
    if (next)next->prev = block;
    return block;
}

DLLEXPORT(void*) asmMalloc(size_t size) {
    EnterCriticalSection(&mem_cs);
    size = roundToNext(size, 16);
    MemoryBlock* dst_block = NULL;
    for (MemoryHeader* current = first; current; current = current->next) {
        for (MemoryBlock* block = (MemoryBlock*)((char*)current + sizeof(MemoryHeader)); block; block = block->next) {
            if (block->bInUse)continue;
            if (block->size > size) {
                //Leeren Block gefunden => Überprüfen
                if (!dst_block || dst_block->size > block->size)dst_block = block;
            }
            else if (block->size == size) {
                //Block passt perfekt => Direkt zurückgeben
                dst_block = block;
                goto foundblock;
            }
        }
    }
    if (dst_block) {
        //freier block gefunden
        if (dst_block->size - size >= sizeof(MemoryBlock) + 16) {
            //ein freier Speicherblock kann am ende eingefügt werden
            createBlockAt((char*)dst_block + size + sizeof(MemoryBlock), dst_block, dst_block->next, dst_block->header, dst_block->size - size - sizeof(MemoryBlock));
            dst_block->size = size;
        }
    foundblock:
        ActivateBlock(dst_block);
        LeaveCriticalSection(&mem_cs);
        return dst_block + 1;
    }
    //Kein Block gefunden
    size_t alloc_size = roundToNext(size + (sizeof(MemoryHeader) + sizeof(MemoryBlock)), dwPageSize);
    void* vMemoryBlock = VirtualAlloc(
        NULL,
        alloc_size,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    if (!vMemoryBlock) {
        LeaveCriticalSection(&mem_cs);
        return NULL;
    }
    MemoryHeader* header = (MemoryHeader*)vMemoryBlock;
    header->next = first;
    header->prev = NULL;
    first = header;
    size_t size_second = 0;
    size_t size_first = alloc_size - sizeof(MemoryBlock) - sizeof(MemoryHeader);
    if (size_first - size >= sizeof(MemoryBlock) + 16) {
        size_second = size_first - sizeof(MemoryBlock) - size;
        size_first -= (sizeof(MemoryBlock) + size_second);
    }
    MemoryBlock* first_block = createBlockAt(
        header + 1,
        NULL,
        NULL,
        header,
        size_first);
    if (size_second) {
        MemoryBlock* second_block = createBlockAt(
            (char*)first_block + size_first + sizeof(MemoryBlock),
            first_block,
            NULL,
            header,
            size_second);
        first_block->next = second_block;
    }
    ActivateBlock(first_block);
    LeaveCriticalSection(&mem_cs);
    return first_block + 1;
}

DLLEXPORT(void) asmFree(void* addr) {
    EnterCriticalSection(&mem_cs);
    MemoryBlock* block = findMemoryBlock(addr);
    DisableBlock(block);
    if (block->next && (!block->next->bInUse)) {
        //nachfolger „verschmelzen“
        block->size += (block->next->size + sizeof(MemoryBlock));
        block->next = block->next->next;
        if (block->next)block->next->prev = block;
    }
    if (block->prev && (!block->prev->bInUse)) {
        //vorgänger „verschmelzen“
        block->prev->size += (block->size + sizeof(MemoryBlock));
        block->prev->next = block->next;
        if (block->next)block->next->prev = block->prev;
        block = block->prev;
    }
    if (!(block->prev || block->next)) {
        //Speicherblock ist vollständig leer => zurück an Windows
        MemoryHeader* header = block->header;
        if (!header->prev)first = header->next;
        else {
            header->prev->next = header->next;
            if (header->next)header->next->prev = header->prev;
        }
        VirtualFree(header, 0, MEM_RELEASE);
    }
    LeaveCriticalSection(&mem_cs);
}

void asmMovMemptr(void* src, void* dst) {
    memcpy((size_t*)dst - 1, (size_t*)src - 1, sizeof(size_t));
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        InitMemSystem();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        if (!lpReserved)uninitMemSystem();
        break;
    }
    return TRUE;
}

extern "C" void* GetArgpushAddr();
extern "C" void* GetRedirectableAddr();

void generate_jmp_code(uint8_t* output, void* jump_address, void* r10_value) {
    // 1. mov rax, imm64 (mov rax, jump_address)
    output[0] = 0x48;           // REX.W prefix for 64-bit operand
    output[1] = 0xB8;           // Opcode for "mov rax, imm64"
    memcpy(&output[2], &jump_address, sizeof(jump_address)); // Kopiere jump_address als imm64

    // 2. mov r10, imm64 (mov r10, r10_value)
    output[10] = 0x49;          // REX.W prefix for R10
    output[11] = 0xBA;          // Opcode for "mov r10, imm64"
    memcpy(&output[12], &r10_value, sizeof(r10_value)); // Kopiere r10_value als imm64

    // 3. jmp rax
    output[20] = 0xFF;          // Opcode for "jmp r/m64"
    output[21] = 0xE0;          // ModR/M byte for "jmp rax"
}

DLLEXPORT(void*) createRedirectableFunction(
    _In_opt_ void* memoryLocation,
    _In_ void* functionStartAddress
) {
    redirect_r10* rdr = (redirect_r10*)memoryLocation;
    if (!rdr) {
        rdr = (redirect_r10*)asmMalloc(sizeof(redirect_r10));
        if (!rdr)return NULL;
    }
    rdr->dstfunction = memoryLocation;
    rdr->freeAddress = (MemoryBlock*)rdr - 1;
    generate_jmp_code(rdr->codeBuffer, GetRedirectableAddr(), rdr);
    if (memoryLocation)FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    else FlushInstructionCache(GetCurrentProcess(), rdr->codeBuffer, sizeof(rdr->codeBuffer));
    return rdr->codeBuffer;
}

DLLEXPORT(void) setRedirectableDest(
    _In_ void* redirectable_function,
    _In_ void* newDest
) {
    redirect_r10* rdr = (redirect_r10*)((char*)redirectable_function - offsetof(redirect_r10, codeBuffer));
    rdr->dstfunction = newDest;
}

enum Argfloat_flags : size_t {
    argfloat_arg4 = 0x1,
    argfloat_arg3 = 0x2,
    argfloat_arg2 = 0x4,
    argfloat_arg1 = 0x8
};

enum ThreadSafetyLevel : unsigned char {
    threadunsafe,
    arginitialize_threadsafe,
    function_threadsafe
};

DLLEXPORT(void) setCallableProperties(
    _In_ void* callable,
    _In_ void* functionAddress,
    size_t argfloat_flags,
    unsigned __int64 argcount_dst,
    ...
) {
    va_list arglist;
    va_start(arglist, argcount_dst);
    dyncode_r10_cs* csptr = (dyncode_r10_cs*)((char*)callable - offsetof(dyncode_r10, codeBuffer));
    if (csptr->r10.bHasCriticalSection)EnterCriticalSection(&csptr->cs);
    csptr->r10.arg1 = va_arg(arglist, unsigned __int64);
    csptr->r10.arg2 = va_arg(arglist, unsigned __int64);
    csptr->r10.argcount = argcount_dst;
    csptr->r10.argflags = argfloat_flags;
    csptr->r10.functionDest = functionAddress;
    if (csptr->r10.bHasCriticalSection)LeaveCriticalSection(&csptr->cs);
}

DLLEXPORT(void) getCallableProperties(
    _In_ void* callable,
    _Out_opt_ void** functionAddress,
    _Out_opt_ size_t* argfloat_flags,
    _Out_opt_ unsigned __int64* argcount_dst,
    _Out_opt_ unsigned char* threadsafety,
    _Out_opt_ unsigned __int64* arg1,
    _Out_opt_ unsigned __int64* arg2
) {
    dyncode_r10* r10= (dyncode_r10*)((char*)callable - offsetof(dyncode_r10, codeBuffer));
    if (functionAddress)*functionAddress = r10->functionDest;
    if (argfloat_flags)*argfloat_flags = r10->argflags;
    if (argcount_dst)*argcount_dst = r10->argcount;
    if (threadsafety)*threadsafety = (r10->bHasCriticalSection ?
        (r10->bReleaseCsAfterArguments ? arginitialize_threadsafe : function_threadsafe) :
        threadunsafe);
    if (arg1)*arg1 = r10->arg1;
    if (arg2)*arg2 = r10->arg2;
}

DLLEXPORT(size_t) queryCallableFunctionSize(
    unsigned char threadsafety
) {
    return (threadsafety ? sizeof(dyncode_r10_cs) : sizeof(dyncode_r10));
}

DLLEXPORT(void*) createCallableFunction(
    _In_opt_ void* memoryLocation,
    _In_ void* functionAddress,
    size_t argfloat_flags,
    unsigned __int64 argcount_dst,
    unsigned char threadsafety,
    ...) {
    va_list arglist;
    va_start(arglist, argcount_dst);
    dyncode_r10* r10 = (dyncode_r10*)memoryLocation;
    if (!memoryLocation) {
        r10 = (dyncode_r10*)asmMalloc((threadsafety?sizeof(dyncode_r10_cs):sizeof(dyncode_r10)));
        if (!r10)return NULL;
    }
    r10->arg1 = va_arg(arglist, unsigned __int64);
    r10->arg2 = va_arg(arglist, unsigned __int64);
    r10->functionDest = functionAddress;
    r10->argflags = argfloat_flags;
    r10->freeAddress = (MemoryBlock*)r10 - 1;
    r10->argcount = argcount_dst;
    r10->bHasCriticalSection = threadsafety;
    r10->bReleaseCsAfterArguments = (threadsafety == arginitialize_threadsafe);
    if (threadsafety)InitializeCriticalSection(&((dyncode_r10_cs*)(r10))->cs);
    generate_jmp_code(r10->codeBuffer, GetArgpushAddr(), r10);
    if (memoryLocation)FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    else {
        FlushInstructionCache(GetCurrentProcess(), r10->codeBuffer, sizeof(r10->codeBuffer));
        findMemoryBlock(r10)->bFreeCriticalSection = true;
    }
    return r10->codeBuffer;
}

DLLEXPORT(va_list) createVAList(size_t size) {
    if (!size)return (va_list)0xFFFFFFFFFFFFFFFF;
    return (va_list)asmMalloc(size * 8);
}

DLLEXPORT(size_t) queryVAListSize(size_t size) {
    return size * 8;
}

DLLEXPORT(void) setVAListElement(va_list list, size_t index, ...) {
    va_list own;
    va_start(own, index);
    ((unsigned __int64*)list)[index] = va_arg(own, unsigned __int64);
}

DLLEXPORT(void*) getVAListIndex(va_list list, size_t index) {
    return &((unsigned __int64*)list)[index];
}

DLLEXPORT(void) copyVAListElements(va_list dst, va_list src, size_t dst_base, size_t src_base, size_t count) {
    for (size_t offset = 0; offset < count; ++offset) {
        setVAListElement(dst, src_base + offset, getVAListIndex(src, dst_base + offset));
    }
}

DLLEXPORT(void) insertVAListElements(va_list dst, size_t offset, size_t countOfArgs, ...) {
    va_list own;
    va_start(own, countOfArgs);
    copyVAListElements(dst, own, offset, 0, countOfArgs);
}
