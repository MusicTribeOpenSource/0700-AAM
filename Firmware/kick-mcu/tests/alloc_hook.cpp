// SPDX-License-Identifier: AGPL-3.0-or-later
#include "alloc_hook.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
std::atomic<bool>   g_armed{ false };
std::atomic<size_t> g_count{ 0 };

void* allocate( std::size_t size )
{
    if( g_armed.load( std::memory_order_relaxed ) )
        g_count.fetch_add( 1, std::memory_order_relaxed );
    void* p = std::malloc( size != 0 ? size : 1 );
    if( p == nullptr )
        throw std::bad_alloc();
    return p;
}
} // namespace

namespace alloc_hook
{
void arm()
{
    g_count = 0;
    g_armed = true;
}

size_t disarm()
{
    g_armed = false;
    return g_count.load();
}
} // namespace alloc_hook

void* operator new( std::size_t size ) { return allocate( size ); }
void* operator new[]( std::size_t size ) { return allocate( size ); }
void  operator delete( void* p ) noexcept { std::free( p ); }
void  operator delete[]( void* p ) noexcept { std::free( p ); }
void  operator delete( void* p, std::size_t ) noexcept { std::free( p ); }
void  operator delete[]( void* p, std::size_t ) noexcept { std::free( p ); }
