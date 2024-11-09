#ifndef _ASMLIB_INCLUDED

#include <stdarg.h>

#ifndef ASM_DONTLINK
#pragma comment(lib,"asmlib.lib")
#define _ASM_DYNAMIC_LINKAGE
#else
#define _ASM_NO_LINKAGE
#endif

#ifdef __cplusplus
#define _ASM_IMPORTPREFIX_1 extern "C"
#define _ASM_AUTO_CONSTEXPR constexpr
#else
#define _ASM_IMPORTPREFIX_1
#define _ASM_AUTO_CONSTEXPR
#endif // __cplusplus

#ifdef _MSC_FULL_VER
#ifdef _ASM_DYNAMIC_LINKAGE
#define _ASM_IMPORTPREFIX_2 __declspec(dllimport)
#else
#define _ASM_IMPORTPREFIX_2
#endif // _ASM_DYNAMIC_LINKAGE
#else
#define _ASM_IMPORTPREFIX_2
#endif // 

#ifdef _Out_
#define _SAL(...) __VA_ARGS__
#else
#define _SAL(...)
#endif // _Out_

#define _ASM_INT64_T long long
#define _ASM_UINT64_T unsigned _ASM_INT64_T

#define ASM_REDIRECT_SIZE 40
#define ASM_CALLABLE_FUNCTION_SIZE 72
#define ASM_CALLABLE_FUNCTION_SIZE_THREADSAFE 112

#define _ASM_DLLIMPORT _ASM_IMPORTPREFIX_1 _ASM_IMPORTPREFIX_2

// Retrieves the size of the memory block specified in <addr>.
// Note that this size might be more than what was requested by asmMalloc
// NEVER pass a pointer that was not (directly or indirectly) allocated by asmMalloc
_ASM_DLLIMPORT _ASM_UINT64_T asmMsize(
	_In_ void* addr
);

#ifndef _ASM_DYNAMIC_LINKAGE
_ASM_DLLIMPORT void asmInitialize();
_ASM_DLLIMPORT void asmUninitialize();
#endif // !_ASM_DYNAMIC_LINKAGE

// Destroys a Function with argbound Arguments at <addr>. 
// Should only be used on memory that was an output buffer for createCallableFunction
_ASM_DLLIMPORT void DestroyArgboundAt(
	_In_ void* addr
);

// Allocates <size> bytes of Memory
// <size> must not be 0. This leads to undefined behaviour.
// returns NULL on error.
_ASM_DLLIMPORT _SAL(_Check_return_ _Result_nullonfailure_ _Ret_opt_bytecap_(size)) void* asmMalloc(
	_SAL(_In_ _In_range_(1,0xFFFFFFFFFF)) _ASM_UINT64_T size
);

// Releases Memory at address <addr> which was previously (directly or indirectly) allocated by asmMalloc
_ASM_DLLIMPORT void asmFree(
	_SAL(_In_) void* addr
);

// Creates a Function pointer which allows for its destination address to be changed later even if it is passed by value
// <memoryLocation> is the optional location on where to place the internal structure. 
// must be at least ASM_REDIRECT_SIZE bytes. if NULL, asmMalloc is called. In this case, NULL might be returned.
// <functionStartAddress> is the initial value
// returns a void* that can be cast into a function pointer.
// returned pointer is NOT equal to <memoryLocation> if present.
_ASM_DLLIMPORT _SAL(_Result_nullonfailure_ _Ret_opt_bytecap_x_(22)) void* createRedirectableFunction(
	_SAL(_Out_writes_bytes_all_opt_(ASM_REDIRECT_SIZE)) void* memoryLocation,
	_SAL(_In_) void* functionStartAddress
);

// Changes a Redirectables destination address
// <memoryLocation> is the pointer returned by createRedirectableFunction
// <functionStartAddress> is the new value
// This operation is atomic, therefore it wont corrupt threads which execute this function during modification,
// BUT wheter a thread executes the old or the new function is undefined.
_ASM_DLLIMPORT void setRedirectableDest(
	_SAL(_Inout_) void* memoryLocation,
	_SAL(_In_) void* functionStartAddress
);

// Changes an argbound function's Properties
// <callable> is the pointer returned by createCallableFunction.
// <functionAddress> is the new destination address.
// <argfloat_flags> is the new floating point flags of the function. See Asm_Argfloat_flags.
// <argcount> is the amount of arguments the function at functionAddress takes (at least 2)
// vararg-functions are not allowed if the amount of arguments is non-constant.
// the varargs are 2 arguments which should be passed in first into the function.
_ASM_DLLIMPORT void setCallableProperties(
	_SAL(_Inout_) void* callable,
	_SAL(_In_) void* functionAddress,
	_SAL(_In_ _In_range_(0, 0xF)) _ASM_UINT64_T argfloat_flags,
	_SAL(_In_ _In_range_(2, 0xFFFF)) _ASM_UINT64_T argcount,
	...);

// Queries Properties of an argbound Function. <callable> is the only mandatory pointer.
// For the meaning of the different outputs, see setCallableProperties.
// Any of its parameters might be NULL, in which case its value is not retrieved.
// threadsafety is a read-only property due to it internally changing the structure's size. arg1 and arg2 are the first and second parameter to be pushed.
// This function always writes 8 bytes to said address, even if the type of the argument is smaller. 
// In these cases the upper (8-sizeof(yourType)) bytes are set to an undefined value.
// ASM_UINT64_AS_TYPE can cast the arguments to the desired types. See the ASM_UINT64_AS_TYPE macro.
_ASM_DLLIMPORT void getCallableProperties(
	_SAL(_In_) void* callable,
	_SAL(_Outptr_opt_) void** functionAddress,
	_SAL(_Out_writes_bytes_all_opt_(8) _Out_range_(0, 0xF)) _ASM_UINT64_T* argfloat_flags,
	_SAL(_Out_writes_bytes_all_opt_(8) _Out_range_(2, 0xFFFF)) _ASM_UINT64_T* argcount_dst,
	_SAL(_Out_opt_ _Out_range_(0, 2))unsigned char* threadsafety,
	_SAL(_Out_writes_bytes_all_opt_(8)) _ASM_UINT64_T* arg1,
	_SAL(_Out_writes_bytes_all_opt_(8)) _ASM_UINT64_T* arg2
);

// creates a new argbound function.
// <memoryLocation> is a buffer atleast ASM_CALLABLE_FUNCTION_SIZE bytes if theadsafety==threadunsafe or ASM_CALLABLE_FUNCTION_SIZE_THREADSAGE in all other cases.
// the size of <memoryLocation> can be calculated by queryCallableFunctionSize.
// may be NULL. in this case createCallableFunction querys the buffer from asmMalloc.
// Other properties carry the same meaning as in setCallableProperties
// returns a buffer that might be cast into a function pointer. returned value is NOT <memoryLocation> if set.
_ASM_DLLIMPORT _SAL(_Result_nullonfailure_ _Ret_opt_bytecap_x_(22)) void* createCallableFunction(
	_SAL(_Out_writes_bytes_all_(ASM_CALLABLE_FUNCTION_SIZE)) void* memoryLocation,
	_SAL(_In_) void* functionAddress,
	_SAL(_In_ _In_range_(0,0xF))_ASM_UINT64_T argfloat_flags,
	_SAL(_In_ _In_range_(2, 0xFFFF)) _ASM_UINT64_T argcount_dst,
	_SAL(_In_ _In_range_(0, 2))unsigned char theadsafety,
	...
);

// creates a new va_list for <size> arguments.
// must be releases by asmFree. va_end does NOT release this list.
// if size is 0, a pointer with all bits set(0xFFFFFFFFFFFFFFFF) is returned.
// this behaviour allows for chaining createVAList with a loop with setVAListElement and finally VAListCall without treating no arguments as a special case.
// If you wish to allocate the va_list in manually allocated memory, see queryVAListSize.
// a va_list does not need explicit cleanup after use. use the proper free function, depending on what allocator you chose.
_ASM_DLLIMPORT va_list _SAL(_Check_return_ _Result_nullonfailure_ _Ret_opt_bytecap_(size*8)) createVAList(
	_SAL(_In_) _ASM_UINT64_T size
);

#ifdef ASM_IMPORT_ALL_FUNCTIONS
//returns the byte-size for a va_list of size <size>
_ASM_DLLIMPORT _ASM_UINT64_T queryVAListSize(
	_ASM_UINT64_T size
);
//returns the byte-size for a callable based on its threadSafety property.
_ASM_DLLIMPORT _ASM_UINT64_T queryCallableFunctionSize(
	unsigned char theadSafety
);
#else
// returns the byte-size for a va_list of size <size>.
// see createVAList.
#define queryVAListSize(size) ((size)*8)
// returns the byte-size for a callable based on its threadSafety property.
// see createCallableFunction.
#define queryCallableFunctionSize(threadSafety) (threadSafety?ASM_CALLABLE_FUNCTION_SIZE_THREADSAFE:ASM_CALLABLE_FUNCTION_SIZE)
#endif // !

// the Element at <index> in <list> is set to the 3rd argument.
// No bounds checking is performed. Writing out of bounds is undefined behaviour.
_ASM_DLLIMPORT void setVAListElement(
	_SAL(_Inout_) va_list list,
	_SAL(_In_) _ASM_UINT64_T index,
	...
);

// returns a pointer to element <index> in <va_list>
// No type information is stored. This Pointer should be cast into a pointer of the type of the argument for further processing.
// This pointer points into the va_list <list>, meaning that writing to it changes the value of element <index> to whatever was written.
// This also means the pointer becomes invalid when the owning list is freed.
// A va_list from va_start is freed when the function which invoked va_start returns.
// see the ASM_VALIST_INDEX macro for easier access.
_ASM_DLLIMPORT _SAL(_Ret_) void* getVAListIndex(
	_SAL(_In_) va_list list,
	_SAL(_In_) _ASM_UINT64_T index
);

// calls the function with the function pointer <function> with the arguments in <list> and the amount of arguments stored in <length>.
// returns the value returned by the called function. <function> can only return supported types.
// When the function <function> returns a float or a double, you must cast the pointer to VAListCall into one that returns float or double.
// In this case, call the ASM_VALISTCALL_FLOATINGPOINT macro, which does this cast internally. See ASM_VALISTCALL_FLOATINGPOINT.
// Else the upper (8-sizeof(returnTypeOfFunction)) are set to an undefined value if sizeof(returnTypeOfFunction)<8
// ASM_UINT64_AS_TYPE can do this calculation for you. See ASM_UINT64_AS_TYPE for more information.
_ASM_DLLIMPORT _ASM_UINT64_T VAListCall(
	_SAL(_In_) void* function,
	_SAL(_In_) va_list list,
	_SAL(_In_) _ASM_UINT64_T length
);


// copy <count> elements out of <src> at offset <src_base> to <dst> at offset <dst_base>
// count must be atleast 1.
// No bounds checking is performed.
_ASM_DLLIMPORT void copyVAListElements(
	_SAL(_Out_) va_list dst,
	_SAL(_In_) va_list src,
	_SAL(_In_) size_t dst_base,
	_SAL(_In_) size_t src_base,
	_SAL(_In_ _In_range_(1, 0xFFFF)) size_t count
);

// inserts <countOfArgs> into <dst> at offset <offset>
// the arguments to insert are placed after <countOfArgs>
// <countOfArgs> must be atleast 1
_ASM_DLLIMPORT void insertVAListElements(
	_SAL(_Out_) va_list dst,
	_SAL(_In_) size_t offset,
	_SAL(_In_ _In_range_(1, 0xFFFF)) size_t countOfArgs,
	...
);

// Specifies whether an argument at position X is a floating-point (float/double) argument
// Do not set for va_args-Arguments.
// Pointers to floating-point data are NOT floating-point arguments.
enum Asm_Argfloat_flags : size_t {
	argfloat_arg4 = 0x1,
	argfloat_arg3 = 0x2,
	argfloat_arg2 = 0x4,
	argfloat_arg1 = 0x8
};

// Specifies a thread-safety level for a callable function:
// threadunsafe - no locking: uses less storage, is a bit more performant than the other options. Use if you are sure no thread calls setCallableProperties while another thread is executing said callable.
// arginitialize_threadsafe - Ensures that setCallableProperties can be called while another thread is executing said callable. See setCallableProperties.
// function_threadsafe - like arginitialize_threadsafe, but it also wraps the function and only releases the lock after the call is finished. Can be used to make thread-unsafe functions thread-safe.
enum Asm_ThreadSafetyLevel : unsigned char {
	threadunsafe,
	arginitialize_threadsafe,
	function_threadsafe
};

// Returns element <index> in va_list <list> of type <type> as a lvalue.
// Internally calls getVAListIndex. See getVAListIndex.
#define ASM_VALIST_INDEX(list, index, type) (*((type*)(getVAListIndex(list, index))))

#ifdef __cpp_static_assert
#define _COMBINE_L2(a,b) a##b
#define _COMBINE_L1(a,b) _COMBINE_L2(a,b)
#define COMBINE(a,b) _COMBINE_L1(a,b)
#define _STRINGIFY(...) #__VA_ARGS__
#define _ASSERT_MESSAGE(type) _STRINGIFY(COMBINE(Unsupported Type[, COMBINE(type, COMBINE(] at Line ,__LINE__))))
#define _ASM_ASSERT_DEFAULT(type) ((!(sizeof(type) & (sizeof(type) - 1)) ) & (sizeof(type) <= 8))
#define _ASM_ASSERT_TYPESUPPORT(type) _ASM_ASSERT_DEFAULT(type)
#ifdef __has_include
#if __has_include("type_traits")
#include <type_traits>
#define _ASM_ASSERT_TYPESUPPORT(type) (_ASM_ASSERT_DEFAULT(type) & (!((bool)(std::is_pod<type>()))))
#endif
#endif // __has_include
// static asserts if type <type> is not supported
#define ASM_ASSERT_TYPESUPPORT(type) static_assert(_ASM_ASSERT_TYPESUPPORT(type),_ASSERT_MESSAGE(type))
// static assert if <expr> does not evaluate to a supported type
#define ASM_ASSERT_EXPR(expr) ASM_ASSERT_TYPESUPPORT(decltype(expr))
#endif // __cpp_static_assert

// Converts an UINT64 <i64> returned by VAListCall or getCallableProperties into the actual object of type <type>
// i64 must be a lvalue so that its address might be taken by the "&"-operator
// ASM_VALISTCALL is usually easier to use in context with VAListCall
// This macro is primarily intended for getCallableProperties
#define ASM_UINT64_AS_TYPE(i64,type) (*((type*)(&(i64&((1 << (sizeof(type) * 8)) - 1)))))

// Like VAListCall, but behaves correctly if the function returns either a float or a double
// and returns the result with the correct type without a call to ASM_UINT64_AS_TYPE being necessary.
// For parameter meaning, see VAListCall
#define ASM_VALISTCALL(restype, function, list, length) (((restype(*)(_SAL(_In_)void*, _SAL(_In_) va_list,_SAL(_In_) size_t))(&VAListCall))(function, list, length))

#define _ASMLIB_INCLUDED
#endif // !_ASMLIB_INCLUDED
