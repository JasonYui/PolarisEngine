#pragma once

namespace Engine
{
/////////////////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////////////////
#define ENDIAN_LITTLE 1
#define ENDIAN_BIG 2
/////////////////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////////////////
#define ARCH_32 1
#define ARCH_64 2

/////////////////////////////////////////////////////////////////////////////////////////
// Platform
/////////////////////////////////////////////////////////////////////////////////////////
#if defined( __WIN32__ ) || defined( _WIN32 )
#   define PLATFORM_WINDOWS 1
#elif defined( __APPLE_CC__)
#   define PLATFORM_APPLE 1
#else
#   define PLATFORM_LINUX 1
#endif

/////////////////////////////////////////////////////////////////////////////////////////
// Compiler
/////////////////////////////////////////////////////////////////////////////////////////
#if defined( __clang__ )

#if defined __apple_build_version__
#   define COMPILER_APPLECLANG
#else
#   define COMPILER_CLANG
#endif
#   define COMPILER_VER (((__clang_major__)*100) + \
                         (__clang_minor__*10) + \
                         __clang_patchlevel__)
#elif defined( __GNUC__ )
#   define COMPILER_GNUC
#   define COMPILER_VER (((__GNUC__)*100) + \
                         (__GNUC_MINOR__*10) + \
                          __GNUC_PATCHLEVEL__)
#elif defined( _MSC_VER )
#   define COMPILER_MSVC
#   define COMPILER_VER _MSC_VER
#else
#   error "No known compiler. Abort! Abort!"
#endif

#if PLATFORM_WINDOWS
#if _WIN64 || WIN64
    #define ENV64BIT 1
    #define ENV32BIT 0
#else
    #define ENV64BIT 0
    #define ENV32BIT 1
#endif
#endif
}