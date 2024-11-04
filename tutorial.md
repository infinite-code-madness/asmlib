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

As you might have noticed, in addition to the standard headers, we have asmlib.h. This header will be used for all functionality offered by asmlib.

Anytime you build something with asmlib, you need to include this header. It is included in the devkit.
It will automatically place a #[pragma comment](https://learn.microsoft.com/de-de/cpp/preprocessor/comment-c-cpp?view=msvc-170) to link against asmlib.lib (also included in the devkit).

If you want to link manually, define ASM_DONTLINK

now that we have the header stuff out the way, we can take a look at the code.
the first 4 lines are just some simple C stuff. lets skip that.

The first interesting thing happens in line 5 of main:

`createVAList` creates a new va_list. This function will be your door opener when making calls to functions with a dynamic amount of arguments.
