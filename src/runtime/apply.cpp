/*
 MIT License

 Copyright (c) 2019 Andy Best

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#include "Runtime.h"

extern "C" void rt_assert_tag(void*, ETypeTag);
extern "C" void* rt_compiled_function_get_ptr(void*);

extern "C" void* rt_apply_0(void* f)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*)) fn_ptr)(f);
}

extern "C" void* rt_apply_1(void* f, void* a0)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*)) fn_ptr)(a0, f);
}

extern "C" void* rt_apply_2(void* f, void* a0, void* a1)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*)) fn_ptr)(a0, a1, f);
}

extern "C" void* rt_apply_3(void* f, void* a0, void* a1, void* a2)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, f);
}

extern "C" void* rt_apply_4(void* f, void* a0, void* a1, void* a2, void* a3)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, f);
}

extern "C" void* rt_apply_5(void* f, void* a0, void* a1, void* a2, void* a3, void* a4)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, f);
}

extern "C" void* rt_apply_6(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, f);
}

extern "C" void* rt_apply_7(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, f);
}

extern "C" void* rt_apply_8(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5,
            a6, a7, f);
}

extern "C" void* rt_apply_9(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3,
            a4, a5, a6, a7, a8, f);
}

extern "C" void* rt_apply_10(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2,
            a3, a4, a5, a6, a7, a8, a9, f);
}

extern "C" void* rt_apply_11(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*)) fn_ptr)(a0,
            a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, f);
}

extern "C" void* rt_apply_12(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, f);
}

extern "C" void* rt_apply_13(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11, void* a12)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, f);
}

extern "C" void* rt_apply_14(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11, void* a12, void* a13)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, f);
}

extern "C" void* rt_apply_15(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11, void* a12, void* a13, void* a14)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, f);
}

extern "C" void* rt_apply_16(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11, void* a12, void* a13, void* a14, void* a15)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, f);
}

extern "C" void* rt_apply_17(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11, void* a12, void* a13, void* a14, void* a15, void* a16)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
            a16, f);
}

extern "C" void* rt_apply_18(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11, void* a12, void* a13, void* a14, void* a15, void* a16, void* a17)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14,
            a15, a16, a17, f);
}

extern "C" void* rt_apply_19(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11, void* a12, void* a13, void* a14, void* a15, void* a16, void* a17,
        void* a18)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*, void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12,
            a13, a14, a15, a16, a17, a18, f);
}

extern "C" void* rt_apply_20(void* f, void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7,
        void* a8, void* a9, void* a10, void* a11, void* a12, void* a13, void* a14, void* a15, void* a16, void* a17,
        void* a18, void* a19)
{
    void* fn_ptr = rt_compiled_function_get_ptr(f);
    return ((void* (*)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*, void*,
            void*, void*, void*, void*, void*, void*, void*)) fn_ptr)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11,
            a12, a13, a14, a15, a16, a17, a18, a19, f);
}