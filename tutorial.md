# Sections

# What you need
- Basic understanding of C
- Basic understanding of how function pointers work in C
- Basic understanding of your IDE's functions
- If you don't understand something, just ask [ChatGPT](https://chatgpt.com/)

# Getting everything ready
**Experienced developers can skip this section.**

Before we can get to coding, we must first get the devkit ready:
- Go into your favourite IDE (I will be using visual studio 2022 here)
- Create a new project(Console Application in Visual Studio, or something similar in your IDE) and remember its directory
- Download "asmlib_devkit.zip" from the newest release
- Unpack it into your project's directory
- (Depending on your IDE, you might need to reopen it for everything to configure properly)
- Now you are ready to go

# Passing dynamic arguments
I wrote a small program to demonstrate this functionality.

Don't worry if you don't understand what it does immediately, we will walk through it step-by-step
```
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <asmlib.h>

int main()
{
    unsigned int len;
    printf("Enter count of arguments:");
    scanf("%u", &len);
    printf("Enter %u arguments(max. 63 characters):\n", len);
    va_list list = createVAList(len);
    for (unsigned int i = 0; i < len; ++i) {
        ASM_VALIST_INDEX(list, i, char*) = (char*)asmMalloc(64);
        scanf("%s", ASM_VALIST_INDEX(list, i, char*));
    }
    VAListCall(printf, list, len);
}
```
<details>
  <summary>Before you complain about scanf</summary>
  Yes, i know this demo has a security vulnerability(Buffer overflow).
  However, this is not the point of this demo, and i will be ignoring it for the sake of simplicity.
  DO NOT copy-paste this code into something that will actually be executed somewhere.
</details>
If you come from C++, you might not be familiar with [scanf](https://en.cppreference.com/w/c/io/fscanf) and [printf](https://en.cppreference.com/w/c/io/fprintf). You should have a basic understanding of how they work, 
because i will be using them for this tutorial. However, most things can be achieved with the c++-types and functions aswell.

### the header

As you might have noticed, in addition to the standard headers, we have asmlib.h. This header will be used for all functionality offered by asmlib.

Anytime you build something with asmlib, you need to include this header. It is included in the devkit.
It will automatically place a #[pragma comment](https://learn.microsoft.com/en-us/cpp/preprocessor/comment-c-cpp?view=msvc-170) to link against asmlib.lib (also included in the devkit).

If you want to link manually, define ASM_DONTLINK

now that we have the header stuff out the way, we can take a look at the code.
the first 4 lines are just some simple C stuff. lets skip that.

The first interesting thing happens in line 5 of main:

### createVAList

`createVAList` creates a new va_list. This function will be your door opener when making calls to functions with a dynamic amount of arguments.

it takes a single argument: the amount of arguments you want the va_list to hold space for. 

It has 2 Interesting behaviours, which you should be aware of:

1. It internally calls asmMalloc to reserve space for the va_list.
This leads to the following:
- If the allocation fails, NULL is returned. You must check the return value before using it.
- You must call asmFree and pass the va_list by value when you are done with using it
⚠ Calling the va_end-Macro will not free the va_list properly and lead to a memory leak ⚠
2. When size is 0, it returns a pointer with all bits set(0xFFFFFFFFFFFFFFFF)

Why?

It allows you to chain createVAList with VAListCall without treating 0 as a special case:

Lets take a look at this function which calls a function by parsing its argument count from an array:
```
int callWithIntArgs(void* function, int* integers, size_t countOfIntegers){
    va_list list = createVAList(countOfIntegers);
    if (!list)return -1;
    for (size_t i=0;i<countOfIntegers;++i){
        ASM_VALIST_INDEX(list, i, int) = integers[i];
    }
    int iRes = ASM_VALISTCALL(int, function, list, countOfIntegers);
    asmFree(list);
    return iRes;
}
```
If the special case for 0 did not exist, you would have to insert either a guard-clause to immediately call ASM_VALISTCALL if countOfIntegers is 0.

Would you have to write it every time? yes. So what would that make it? If you said boilerplate code, you would be correct. 
Remember, we are trying to avoid boilerplate code, we are not coding in Java after all.

### ASM_VALIST_INDEX
When writing to/reading from a va_list, ASM_VALIST_INDEX is your best friend
Lets take a closer look:
```
ASM_VALIST_INDEX(list, i, char*) = (char*)asmMalloc(64);
```
ignore asmMalloc for now. We will discuss the properties of asmMalloc-allocated Heap space later. For now, think of it as (malloc)[https://en.cppreference.com/w/c/memory/malloc].

Lets go through each ASM_VALIST_INDEX argument:
1. this is the list. pretty self explanatory. It is a valid va_list. You can also use a va_list created by va_start.
2. i is the index of the argument you want to access. 0 stands for the first argument, 1 for the second, etc.
3. this is the type you would usually pass to (va_arg)[https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/va-arg-va-copy-va-end-va-start?view=msvc-170]
   it determines what the expression evaluates to.
This macro evaluates to a lvalue, which allows you to assign a value to it (Set the argument) or read it.

Imagine this macro as list[index], and it evaluating to type.

If you use the &-Operator to get an Argument's address, the returned pointer's memory is owned by the va_list and its lifespan is tied to that of the va_list:
1. If retrieved by createVAList, the list stays valid until asmFree is called.
2. If retrieved by va_start, the list stays valid until the stackframe of the function that called va_start is destroyed, be it by the function returning, or exiting in other ways(RaiseException, longjmp, ...)

### asmMalloc
Another allocator? can't we just make it simple and use malloc?

No, we can't!

Why?

Memory allocated by malloc is not allocated with the PAGE_EXECUTE right, so if you casted a pointer returned by malloc into a function pointer, and called it, your program would crash regardless on wheter valid instructions can be found at said address. However, asmlib needs to generate dynamic instructions.

It also allows for internal Functions to return "shifted" pointers, which you can cast into function pointers directly without incrementing them by an arbitrary value. Why is this important? In order to prevent Heap-Fragmentation and improve efficiency, internal data is stored in the same allocation as the newly-generated assembly instructions. Because the function pointer has to point to the newly-generated assembly instructions, it would have to be offset by the size of the internal data, which would open doors to tons of bugs in your code. Also, you dont have to store an extra pointer just for the sake of passing it to asmFree.

asmMalloc is thread-safe, however, it does not have a per-region, but rather a central lock, which might lead to decreased performance if called too many times.

asmMalloc is not highly optimized and rather primitive, consider using a faster alternative when allocating memory for something outside of asmlib

⚠ Do NOT pass 0 as the size. This leads to undefined behaviour. ⚠

⚠ asmMalloc is especially vulnerable to Buffer-Overflows, because its memory is allocated with the PAGE_EXECUTE right, which would lead to remote code execution once a function which was overrun by the Buffer-Overflow is called. Do not use asmMalloc for untrusted data, and call asmMsize if you are unsure on wheter data would fit. ⚠
