#pragma once

//
// Temporary until MSVC STL supports no-exceptions mode.
// Currently terminate is a no-op in this mode, so we add termination behavior back
//
#if defined(_MSC_VER) && (defined(_KERNEL_MODE) || (defined(_HAS_EXCEPTIONS) && !_HAS_EXCEPTIONS))
#define GSL_KERNEL_MODE

#define GSL_MSVC_USE_STL_NOEXCEPTION_WORKAROUND
#include <intrin.h>
#define RANGE_CHECKS_FAILURE 0

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-noreturn"
#endif // defined(__clang__)

#else // defined(_MSC_VER) && (defined(_KERNEL_MODE) || (defined(_HAS_EXCEPTIONS) &&
// !_HAS_EXCEPTIONS))

#include <exception>

#endif // defined(_MSC_VER) && (defined(_KERNEL_MODE) || (defined(_HAS_EXCEPTIONS) &&
// !_HAS_EXCEPTIONS))

//
// make suppress attributes parse for some compilers
// Hopefully temporary until suppression standardization occurs
//
#if defined(__clang__)
#define GSL_SUPPRESS(x) [[gsl::suppress("x")]]
#else
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define GSL_SUPPRESS(x) [[gsl::suppress(x)]]
#else
#define GSL_SUPPRESS(x)
#endif // _MSC_VER
#endif // __clang__

#define GSL_STRINGIFY_DETAIL(x) #x
#define GSL_STRINGIFY(x) GSL_STRINGIFY_DETAIL(x)

#if defined(__clang__) || defined(__GNUC__)
#define GSL_LIKELY(x) __builtin_expect(!!(x), 1)
#define GSL_UNLIKELY(x) __builtin_expect(!!(x), 0)

#else

#define GSL_LIKELY(x) (!!(x))
#define GSL_UNLIKELY(x) (!!(x))
#endif // defined(__clang__) || defined(__GNUC__)

//
// GSL_ASSUME(cond)
//
// Tell the optimizer that the predicate cond must hold. It is unspecified
// whether or not cond is actually evaluated.
//
#ifdef _MSC_VER
#define GSL_ASSUME(cond) __assume(cond)
#elif defined(__GNUC__)
#define GSL_ASSUME(cond) ((cond) ? static_cast<void>(0) : __builtin_unreachable())
#else
#define GSL_ASSUME(cond) static_cast<void>((cond) ? 0 : 0)
#endif

//
// GSL.assert: assertions
//

namespace gsl
{

    namespace details
    {
#if defined(GSL_MSVC_USE_STL_NOEXCEPTION_WORKAROUND)

        typedef void(__cdecl* terminate_handler)();

    // clang-format off
    GSL_SUPPRESS(f.6) // NO-FORMAT: attribute
    // clang-format on
    [[noreturn]] inline void __cdecl default_terminate_handler()
    {
        __fastfail(RANGE_CHECKS_FAILURE);
    }

    inline gsl::details::terminate_handler& get_terminate_handler() noexcept
    {
        static terminate_handler handler = &default_terminate_handler;
        return handler;
    }

#endif // defined(GSL_MSVC_USE_STL_NOEXCEPTION_WORKAROUND)

        [[noreturn]] inline void terminate() noexcept
        {
#if defined(GSL_MSVC_USE_STL_NOEXCEPTION_WORKAROUND)
            (*gsl::details::get_terminate_handler())();
#else
            std::terminate();
#endif // defined(GSL_MSVC_USE_STL_NOEXCEPTION_WORKAROUND)
        }

    } // namespace details
} // namespace gsl

#define CONTRACT_CHECK(expr, file, line) \
    (!!(expr) ? static_cast<void>(0) : gsl::details::terminate())

#ifdef DEBUG
#define ENSURE(expr) CONTRACT_CHECK(expr, __FILE__, __LINE__)
#define EXPECT(expr) CONTRACT_CHECK(expr, __FILE__, __LINE__)

#else
#define ENSURE(expr)
#define EXPECT(expr) expr
#endif

#if defined(GSL_MSVC_USE_STL_NOEXCEPTION_WORKAROUND) && defined(__clang__)
#pragma clang diagnostic pop
#endif