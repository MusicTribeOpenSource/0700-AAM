// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <cstdlib>
#include <cstdio>
#include <new>
#ifdef _WIN32
#include <malloc.h>
#endif
// Include once per executable. Every matching deallocation form uses the same
// allocator, including the sized forms selected by GCC/libstdc++ under ASan.
static thread_local bool forbidAllocation = false;
static void checkAllocation() {
    if (forbidAllocation) { std::fputs("Heap allocation during rendering\n", stderr); std::abort(); }
}
void* operator new(std::size_t n) {
    checkAllocation();
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new[](n); } catch (...) { return nullptr; } }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t a) {
    checkAllocation();
#ifdef _WIN32
    if (auto* p = _aligned_malloc(n ? n : 1, static_cast<std::size_t>(a))) return p;
#else
    void* p = nullptr;
    if (posix_memalign(&p, static_cast<std::size_t>(a), n ? n : 1) == 0) return p;
#endif
    throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept {
#ifdef _WIN32
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new(n, a); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new[](n, a); } catch (...) { return nullptr; } }
void operator delete(void* p, std::align_val_t a, const std::nothrow_t&) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, std::align_val_t a, const std::nothrow_t&) noexcept { ::operator delete(p, a); }
