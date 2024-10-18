/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                          License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Copyright (C) 2013, Tiger Foundation, all rights reserved.
// Copyright (C) 2015, Itseez Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#ifndef TIGER_CORE_TIDEF_H
#define TIGER_CORE_TIDEF_H

#include "tiger/base/version.hpp"

//! @addtogroup core_utils
//! @{

#ifdef TIGER_INCLUDE_PORT_FILE  // User-provided header file with custom platform configuration
#include TIGER_INCLUDE_PORT_FILE
#endif

#if !defined TI_DOXYGEN && !defined TI_IGNORE_DEBUG_BUILD_GUARD
#if (defined(_MSC_VER) && (defined(DEBUG) || defined(_DEBUG))) || \
    (defined(_GLIBCXX_DEBUG) || defined(_GLIBCXX_DEBUG_PEDANTIC))
// Guard to prevent using of binary incompatible binaries / runtimes
#define TI__DEBUG_NS_BEGIN namespace debug_build_guard {
#define TI__DEBUG_NS_END }

namespace ti
{
    namespace debug_build_guard
    {
    }

    using namespace debug_build_guard;
}
#endif
#endif

#ifndef TI__DEBUG_NS_BEGIN
#define TI__DEBUG_NS_BEGIN
#define TI__DEBUG_NS_END
#endif


#ifdef __TIGER_BUILD
#include "ticonfig.h"
#endif

#ifndef __TI_EXPAND
#define __TI_EXPAND(x) x
#endif

#ifndef __TI_CAT
#define __TI_CAT__(x, y) x ## y
#define __TI_CAT_(x, y) __TI_CAT__(x, y)
#define __TI_CAT(x, y) __TI_CAT_(x, y)
#endif

#define __TI_VA_NUM_ARGS_HELPER(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define __TI_VA_NUM_ARGS(...) __TI_EXPAND(__TI_VA_NUM_ARGS_HELPER(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

#ifdef TI_Func
// keep current value (through Tiger port file)
#elif defined __GNUC__ || (defined (__cpluscplus) && (__cpluscplus >= 201103))
#define TI_Func __func__
#elif defined __clang__ && (__clang_minor__ * 100 + __clang_major__ >= 305)
#define TI_Func __func__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION >= 199901)
#define TI_Func __func__
#elif defined _MSC_VER
#define TI_Func __FUNCTION__
#elif defined(__INTEL_COMPILER) && (_INTEL_COMPILER >= 600)
#define TI_Func __FUNCTION__
#elif defined __IBMCPP__ && __IBMCPP__ >=500
#define TI_Func __FUNCTION__
#elif defined __BORLAND__ && (__BORLANDC__ >= 0x550)
#define TI_Func __FUNC__
#else
#define TI_Func "<unknown>"
#endif

//! @cond IGNORED

//////////////// static assert /////////////////
#define TIAUX_CONCAT_EXP(a, b) a##b
#define TIAUX_CONCAT(a, b) TIAUX_CONCAT_EXP(a,b)

#if defined(__clang__)
#  ifndef __has_extension
#    define __has_extension __has_feature /* compatibility, for older versions of clang */
#  endif
#  if __has_extension(cxx_static_assert)
#    define TI_StaticAssert(condition, reason)    static_assert((condition), reason " " #condition)
#  elif __has_extension(c_static_assert)
#    define TI_StaticAssert(condition, reason)    _Static_assert((condition), reason " " #condition)
#  endif
#elif defined(__GNUC__)
#  if (defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L)
#    define TI_StaticAssert(condition, reason)    static_assert((condition), reason " " #condition)
#  endif
#elif defined(_MSC_VER)
#  if _MSC_VER >= 1600 /* MSVC 10 */
#    define TI_StaticAssert(condition, reason)    static_assert((condition), reason " " #condition)
#  endif
#endif
#ifndef TI_StaticAssert
#  if !defined(__clang__) && defined(__GNUC__) && (__GNUC__*100 + __GNUC_MINOR__ > 302)
#    define TI_StaticAssert(condition, reason) ({ extern int __attribute__((error("TI_StaticAssert: " reason " " #condition))) TI_StaticAssert(); ((condition) ? 0 : TI_StaticAssert()); })
#  else
namespace ti {
     template <bool x> struct TI_StaticAssert_failed;
     template <> struct TI_StaticAssert_failed<true> { enum { val = 1 }; };
     template<int x> struct TI_StaticAssert_test {};
}
#    define TI_StaticAssert(condition, reason)\
       typedef cv::TI_StaticAssert_test< sizeof(cv::TI_StaticAssert_failed< static_cast<bool>(condition) >) > TIAUX_CONCAT(TI_StaticAssert_failed_at_, __LINE__)
#  endif
#endif

// Suppress warning "-Wdeprecated-declarations" / C4996
#if defined(_MSC_VER)
#define TI_DO_PRAGMA(x) __pragma(x)
#elif defined(__GNUC__)
    #define TI_DO_PRAGMA(x) _Pragma (#x)
#else
    #define TI_DO_PRAGMA(x)
#endif

#ifdef _MSC_VER
#define TI_SUPPRESS_DEPRECATED_START \
    TI_DO_PRAGMA(warning(push)) \
    TI_DO_PRAGMA(warning(disable: 4996))
#define TI_SUPPRESS_DEPRECATED_END TI_DO_PRAGMA(warning(pop))
#elif defined (__clang__) || ((__GNUC__)  && (__GNUC__*100 + __GNUC_MINOR__ > 405))
#define TI_SUPPRESS_DEPRECATED_START \
    TI_DO_PRAGMA(GCC diagnostic push) \
    TI_DO_PRAGMA(GCC diagnostic ignored "-Wdeprecated-declarations")
#define TI_SUPPRESS_DEPRECATED_END TI_DO_PRAGMA(GCC diagnostic pop)
#else
#define TI_SUPPRESS_DEPRECATED_START
#define TI_SUPPRESS_DEPRECATED_END
#endif

#define TI_UNUSED(name) (void)name

//! @endcond

// undef problematic defines sometimes defined by system headers (windows.h in particular)
#undef small
#undef min
#undef max
#undef abs
#undef Complex

#if defined __cplusplus
#include <limits>
#else
#include <limits.h>
#endif

#if defined __ICL
#  define TI_ICC   __ICL
#elif defined __ICC
#  define TI_ICC   __ICC
#elif defined __ECL
#  define TI_ICC   __ECL
#elif defined __ECC
#  define TI_ICC   __ECC
#elif defined __INTEL_COMPILER
#  define TI_ICC   __INTEL_COMPILER
#endif

#ifndef TI_INLINE
#  if defined __cplusplus
#    define TI_INLINE static inline
#  elif defined _MSC_VER
#    define TI_INLINE __inline
#  else
#    define TI_INLINE static
#  endif
#endif

#ifndef TI_ALWAYS_INLINE
#if defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#define TI_ALWAYS_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define TI_ALWAYS_INLINE __forceinline
#else
#define TI_ALWAYS_INLINE inline
#endif
#endif

#if defined TI_DISABLE_OPTIMIZATION || (defined TI_ICC && !defined TI_ENABLE_UNROLLED)
#  define TI_ENABLE_UNROLLED 0
#else
#  define TI_ENABLE_UNROLLED 1
#endif

#ifdef __GNUC__
#  define TI_DECL_ALIGNED(x) __attribute__ ((aligned (x)))
#elif defined _MSC_VER
#  define TI_DECL_ALIGNED(x) __declspec(align(x))
#else
#  define TI_DECL_ALIGNED(x)
#endif

/* CPU features and intrinsics support */
#define TI_CPU_NONE             0
#define TI_CPU_MMX              1
#define TI_CPU_SSE              2
#define TI_CPU_SSE2             3
#define TI_CPU_SSE3             4
#define TI_CPU_SSSE3            5
#define TI_CPU_SSE4_1           6
#define TI_CPU_SSE4_2           7
#define TI_CPU_POPCNT           8
#define TI_CPU_FP16             9
#define TI_CPU_AVX              10
#define TI_CPU_AVX2             11
#define TI_CPU_FMA3             12

#define TI_CPU_AVX_512F         13
#define TI_CPU_AVX_512BW        14
#define TI_CPU_AVX_512CD        15
#define TI_CPU_AVX_512DQ        16
#define TI_CPU_AVX_512ER        17
#define TI_CPU_AVX_512IFMA512   18 // deprecated
#define TI_CPU_AVX_512IFMA      18
#define TI_CPU_AVX_512PF        19
#define TI_CPU_AVX_512VBMI      20
#define TI_CPU_AVX_512VL        21
#define TI_CPU_AVX_512VBMI2     22
#define TI_CPU_AVX_512VNNI      23
#define TI_CPU_AVX_512BITALG    24
#define TI_CPU_AVX_512VPOPCNTDQ 25
#define TI_CPU_AVX_5124VNNIW    26
#define TI_CPU_AVX_5124FMAPS    27

#define TI_CPU_NEON             100

#define TI_CPU_MSA              150

#define TI_CPU_VSX              200
#define TI_CPU_VSX3             201

#define TI_CPU_RVV              210

// CPU features groups
#define TI_CPU_AVX512_SKX       256
#define TI_CPU_AVX512_COMMON    257
#define TI_CPU_AVX512_KNL       258
#define TI_CPU_AVX512_KNM       259
#define TI_CPU_AVX512_CNL       260
#define TI_CPU_AVX512_CLX       261
#define TI_CPU_AVX512_ICL       262

// when adding to this list remember to update the following enum
#define TI_HARDWARE_MAX_FEATURE 512

#include "ti_cpu_dispatch.h"

#if !defined(TI_STRONG_ALIGNMENT) && defined(__arm__) && !(defined(__aarch64__) || defined(_M_ARM64))
// int*, int64* should be propertly aligned pointers on ARMv7
#define TI_STRONG_ALIGNMENT 1
#endif
#if !defined(TI_STRONG_ALIGNMENT)
#define TI_STRONG_ALIGNMENT 0
#endif

/* fundamental constants */
#define TI_PI   3.1415926535897932384626433832795
#define TI_2PI  6.283185307179586476925286766559
#define TI_LOG2 0.69314718055994530941723212145818

#if defined __ARM_FP16_FORMAT_IEEE \
    && !defined __CUDACC__
#  define TI_FP16_TYPE 1
#else
#  define TI_FP16_TYPE 0
#endif

#ifndef TIGER_ABI_COMPATIBILITY
#define TIGER_ABI_COMPATIBILITY 400
#endif

#ifdef __TIGER_BUILD
#  define DISABLE_TIGER_3_COMPATIBILITY
#  define TIGER_DISABLE_DEPRECATED_COMPATIBILITY
#endif

#ifndef TI_EXPORTS
# if (defined _WIN32 || defined WINCE || defined __CYGWIN__) && defined(TIAPI_EXPORTS)
#   define TI_EXPORTS __declspec(dllexport)
# elif defined __GNUC__ && __GNUC__ >= 4 && (defined(TIAPI_EXPORTS) || defined(__APPLE__))
#   define TI_EXPORTS __attribute__ ((visibility ("default")))
# endif
#endif

#ifndef TI_EXPORTS
# define TI_EXPORTS
#endif

#ifdef _MSC_VER
#   define TI_EXPORTS_TEMPLATE
#else
#   define TI_EXPORTS_TEMPLATE TI_EXPORTS
#endif

#ifndef TI_DEPRECATED
#  if defined(__GNUC__)
#    define TI_DEPRECATED __attribute__ ((deprecated))
#  elif defined(_MSC_VER)
#    define TI_DEPRECATED __declspec(deprecated)
#  else
#    define TI_DEPRECATED
#  endif
#endif

#ifndef TI_DEPRECATED_EXTERNAL
#  if defined(__TIGER_BUILD)
#    define TI_DEPRECATED_EXTERNAL /* nothing */
#  else
#    define TI_DEPRECATED_EXTERNAL TI_DEPRECATED
#  endif
#endif


#ifndef TI_EXTERN_C
#  ifdef __cplusplus
#    define TI_EXTERN_C extern "C"
#  else
#    define TI_EXTERN_C
#  endif
#endif

/* special informative macros for wrapper generators */
#define TI_EXPORTS_W TI_EXPORTS
#define TI_EXPORTS_W_SIMPLE TI_EXPORTS
#define TI_EXPORTS_AS(synonym) TI_EXPORTS
#define TI_EXPORTS_W_MAP TI_EXPORTS
#define TI_IN_OUT
#define TI_OUT
#define TI_PROP
#define TI_PROP_RW
#define TI_WRAP
#define TI_WRAP_AS(synonym)
#define TI_WRAP_MAPPABLE(mappable)
#define TI_WRAP_PHANTOM(phantom_header)
#define TI_WRAP_DEFAULT(val)

/****************************************************************************************\
*                                  Matrix type (Mat)                                     *
\****************************************************************************************/

#define TI_MAT_CN_MASK          ((TI_CN_MAX - 1) << TI_CN_SHIFT)
#define TI_MAT_CN(flags)        ((((flags) & TI_MAT_CN_MASK) >> TI_CN_SHIFT) + 1)
#define TI_MAT_TYPE_MASK        (TI_DEPTH_MAX*TI_CN_MAX - 1)
#define TI_MAT_TYPE(flags)      ((flags) & TI_MAT_TYPE_MASK)
#define TI_MAT_CONT_FLAG_SHIFT  14
#define TI_MAT_CONT_FLAG        (1 << TI_MAT_CONT_FLAG_SHIFT)
#define TI_IS_MAT_CONT(flags)   ((flags) & TI_MAT_CONT_FLAG)
#define TI_IS_CONT_MAT          TI_IS_MAT_CONT
#define TI_SUBMAT_FLAG_SHIFT    15
#define TI_SUBMAT_FLAG          (1 << TI_SUBMAT_FLAG_SHIFT)
#define TI_IS_SUBMAT(flags)     ((flags) & TI_MAT_SUBMAT_FLAG)

/** Size of each channel item,
   0x28442211 = 0010 1000 0100 0100 0010 0010 0001 0001 ~ array of sizeof(arr_type_elem) */
#define TI_ELEM_SIZE1(type) ((0x28442211 >> TI_MAT_DEPTH(type)*4) & 15)

#define TI_ELEM_SIZE(type) (TI_MAT_CN(type)*TI_ELEM_SIZE1(type))

#ifndef MIN
#  define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif

///////////////////////////////////////// Enum operators ///////////////////////////////////////

/**

Provides compatibility operators for both classical and C++11 enum classes,
as well as exposing the C++11 enum class members for backwards compatibility

@code
    // Provides operators required for flag enums
    TI_ENUM_FLAGS(AccessFlag)

    // Exposes the listed members of the enum class AccessFlag to the current namespace
    TI_ENUM_CLASS_EXPOSE(AccessFlag, ACCESS_READ [, ACCESS_WRITE [, ...] ]);
@endcode
*/

#define __TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST)                                              \
static const EnumType MEMBER_CONST = EnumType::MEMBER_CONST;
#define __TI_ENUM_CLASS_EXPOSE_2(EnumType, MEMBER_CONST, ...)                                         \
__TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST);                                                     \
__TI_EXPAND(__TI_ENUM_CLASS_EXPOSE_1(EnumType, __VA_ARGS__));
#define __TI_ENUM_CLASS_EXPOSE_3(EnumType, MEMBER_CONST, ...)                                         \
__TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST);                                                     \
__TI_EXPAND(__TI_ENUM_CLASS_EXPOSE_2(EnumType, __VA_ARGS__));
#define __TI_ENUM_CLASS_EXPOSE_4(EnumType, MEMBER_CONST, ...)                                         \
__TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST);                                                     \
__TI_EXPAND(__TI_ENUM_CLASS_EXPOSE_3(EnumType, __VA_ARGS__));
#define __TI_ENUM_CLASS_EXPOSE_5(EnumType, MEMBER_CONST, ...)                                         \
__TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST);                                                     \
__TI_EXPAND(__TI_ENUM_CLASS_EXPOSE_4(EnumType, __VA_ARGS__));
#define __TI_ENUM_CLASS_EXPOSE_6(EnumType, MEMBER_CONST, ...)                                         \
__TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST);                                                     \
__TI_EXPAND(__TI_ENUM_CLASS_EXPOSE_5(EnumType, __VA_ARGS__));
#define __TI_ENUM_CLASS_EXPOSE_7(EnumType, MEMBER_CONST, ...)                                         \
__TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST);                                                     \
__TI_EXPAND(__TI_ENUM_CLASS_EXPOSE_6(EnumType, __VA_ARGS__));
#define __TI_ENUM_CLASS_EXPOSE_8(EnumType, MEMBER_CONST, ...)                                         \
__TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST);                                                     \
__TI_EXPAND(__TI_ENUM_CLASS_EXPOSE_7(EnumType, __VA_ARGS__));
#define __TI_ENUM_CLASS_EXPOSE_9(EnumType, MEMBER_CONST, ...)                                         \
__TI_ENUM_CLASS_EXPOSE_1(EnumType, MEMBER_CONST);                                                     \
__TI_EXPAND(__TI_ENUM_CLASS_EXPOSE_8(EnumType, __VA_ARGS__));
#define __TI_ENUM_FLAGS_LOGICAL_NOT(EnumType)                                                         \
static inline bool operator!(const EnumType& val)                                                     \
{                                                                                                     \
    typedef std::underlying_type<EnumType>::type UnderlyingType;                                      \
    return !static_cast<UnderlyingType>(val);                                                         \
}
#define __TI_ENUM_FLAGS_LOGICAL_NOT_EQ(Arg1Type, Arg2Type)                                            \
static inline bool operator!=(const Arg1Type& a, const Arg2Type& b)                                   \
{                                                                                                     \
    return static_cast<int>(a) != static_cast<int>(b);                                                \
}
#define __TI_ENUM_FLAGS_LOGICAL_EQ(Arg1Type, Arg2Type)                                                \
static inline bool operator==(const Arg1Type& a, const Arg2Type& b)                                   \
{                                                                                                     \
    return static_cast<int>(a) == static_cast<int>(b);                                                \
}
#define __TI_ENUM_FLAGS_BITWISE_NOT(EnumType)                                                         \
static inline EnumType operator~(const EnumType& val)                                                 \
{                                                                                                     \
    typedef std::underlying_type<EnumType>::type UnderlyingType;                                      \
    return static_cast<EnumType>(~static_cast<UnderlyingType>(val));                                  \
}
#define __TI_ENUM_FLAGS_BITWISE_OR(EnumType, Arg1Type, Arg2Type)                                      \
static inline EnumType operator|(const Arg1Type& a, const Arg2Type& b)                                \
{                                                                                                     \
    typedef std::underlying_type<EnumType>::type UnderlyingType;                                      \
    return static_cast<EnumType>(static_cast<UnderlyingType>(a) | static_cast<UnderlyingType>(b));    \
}
#define __TI_ENUM_FLAGS_BITWISE_AND(EnumType, Arg1Type, Arg2Type)                                     \
static inline EnumType operator&(const Arg1Type& a, const Arg2Type& b)                                \
{                                                                                                     \
    typedef std::underlying_type<EnumType>::type UnderlyingType;                                      \
    return static_cast<EnumType>(static_cast<UnderlyingType>(a) & static_cast<UnderlyingType>(b));    \
}
#define __TI_ENUM_FLAGS_BITWISE_XOR(EnumType, Arg1Type, Arg2Type)                                     \
static inline EnumType operator^(const Arg1Type& a, const Arg2Type& b)                                \
{                                                                                                     \
    typedef std::underlying_type<EnumType>::type UnderlyingType;                                      \
    return static_cast<EnumType>(static_cast<UnderlyingType>(a) ^ static_cast<UnderlyingType>(b));    \
}
#define __TI_ENUM_FLAGS_BITWISE_OR_EQ(EnumType, Arg1Type)                                             \
static inline EnumType& operator|=(EnumType& _this, const Arg1Type& val)                              \
{                                                                                                     \
    _this = static_cast<EnumType>(static_cast<int>(_this) | static_cast<int>(val));                   \
    return _this;                                                                                     \
}
#define __TI_ENUM_FLAGS_BITWISE_AND_EQ(EnumType, Arg1Type)                                            \
static inline EnumType& operator&=(EnumType& _this, const Arg1Type& val)                              \
{                                                                                                     \
    _this = static_cast<EnumType>(static_cast<int>(_this) & static_cast<int>(val));                   \
    return _this;                                                                                     \
}
#define __TI_ENUM_FLAGS_BITWISE_XOR_EQ(EnumType, Arg1Type)                                            \
static inline EnumType& operator^=(EnumType& _this, const Arg1Type& val)                              \
{                                                                                                     \
    _this = static_cast<EnumType>(static_cast<int>(_this) ^ static_cast<int>(val));                   \
    return _this;                                                                                     \
}
#define TI_ENUM_CLASS_EXPOSE(EnumType, ...)                                                           \
__TI_EXPAND(__TI_CAT(__TI_ENUM_CLASS_EXPOSE_, __TI_VA_NUM_ARGS(__VA_ARGS__))(EnumType, __VA_ARGS__));
#define TI_ENUM_FLAGS(EnumType)                                                                       \
__TI_ENUM_FLAGS_LOGICAL_NOT      (EnumType)                                                           \
__TI_ENUM_FLAGS_LOGICAL_EQ       (EnumType, int)                                                      \
__TI_ENUM_FLAGS_LOGICAL_NOT_EQ   (EnumType, int)                                                      \
                                                                                                      \
__TI_ENUM_FLAGS_BITWISE_NOT      (EnumType)                                                           \
__TI_ENUM_FLAGS_BITWISE_OR       (EnumType, EnumType, EnumType)                                       \
__TI_ENUM_FLAGS_BITWISE_AND      (EnumType, EnumType, EnumType)                                       \
__TI_ENUM_FLAGS_BITWISE_XOR      (EnumType, EnumType, EnumType)                                       \
                                                                                                      \
__TI_ENUM_FLAGS_BITWISE_OR_EQ    (EnumType, EnumType)                                                 \
__TI_ENUM_FLAGS_BITWISE_AND_EQ   (EnumType, EnumType)                                                 \
__TI_ENUM_FLAGS_BITWISE_XOR_EQ   (EnumType, EnumType)
/****************************************************************************************\
*                                    static analysys                                     *
\****************************************************************************************/

// In practice, some macro are not processed correctly (noreturn is not detected).
// We need to use simplified definition for them.
#ifndef TI_STATIC_ANALYSIS
# if defined(__KLOCWORK__) || defined(__clang_analyzer__) || defined(__COVERITY__)
#   define TI_STATIC_ANALYSIS 1
# endif
#else
# if defined(TI_STATIC_ANALYSIS) && !(__TI_CAT(1, TI_STATIC_ANALYSIS) == 1)  // defined and not empty
#   if 0 == TI_STATIC_ANALYSIS
#     undef TI_STATIC_ANALYSIS
#   endif
# endif
#endif

/****************************************************************************************\
*                                    Thread sanitizer                                    *
\****************************************************************************************/
#ifndef TI_THREAD_SANITIZER
# if defined(__has_feature)
#   if __has_feature(thread_sanitizer)
#     define TI_THREAD_SANITIZER
#   endif
# endif
#endif

/****************************************************************************************\
*          exchange-add operation for atomic operations on reference counters            *
\****************************************************************************************/

#ifdef TI_XADD
// allow to use user-defined macro
#elif defined __GNUC__ || defined __clang__
#  if defined __clang__ && __clang_major__ >= 3 && !defined __ANDROID__ && !defined __EMSCRIPTEN__ && !defined(__CUDACC__)  && !defined __INTEL_COMPILER
#    ifdef __ATOMIC_ACQ_REL
#      define TI_XADD(addr, delta) __c11_atomic_fetch_add((_Atomic(int)*)(addr), delta, __ATOMIC_ACQ_REL)
#    else
#      define TI_XADD(addr, delta) __atomic_fetch_add((_Atomic(int)*)(addr), delta, 4)
#    endif
#  else
#    if defined __ATOMIC_ACQ_REL && !defined __clang__
       // version for gcc >= 4.7
#      define TI_XADD(addr, delta) (int)__atomic_fetch_add((unsigned*)(addr), (unsigned)(delta), __ATOMIC_ACQ_REL)
#    else
#      define TI_XADD(addr, delta) (int)__sync_fetch_and_add((unsigned*)(addr), (unsigned)(delta))
#    endif
#  endif
#elif defined _MSC_VER && !defined RC_INVOKED
#  include <intrin.h>
#  define TI_XADD(addr, delta) (int)_InterlockedExchangeAdd((long volatile*)addr, delta)
#else
#ifdef TIGER_FORCE_UNSAFE_XADD
    TI_INLINE TI_XADD(int* addr, int delta) { int tmp = *addr; *addr += delta; return tmp; }
#else
    #error "Tiger: can't define safe TI_XADD macro for current platform (unsupported). Define TI_XADD macro through custom port header (see TIGER_INCLUDE_PORT_FILE)"
#endif
#endif


/****************************************************************************************\
*                                  TI_NORETURN attribute                                 *
\****************************************************************************************/

#ifndef TI_NORETURN
#  if defined(__GNUC__)
#    define TI_NORETURN __attribute__((__noreturn__))
#  elif defined(_MSC_VER) && (_MSC_VER >= 1300)
#    define TI_NORETURN __declspec(noreturn)
#  else
#    define TI_NORETURN /* nothing by default */
#  endif
#endif


/****************************************************************************************\
*                                  TI_NODISCARD attribute                                *
* encourages the compiler to issue a warning if the return value is discarded (C++17)    *
\****************************************************************************************/
#ifndef TI_NODISCARD
#  if defined(__GNUC__)
#    define TI_NODISCARD __attribute__((__warn_unused_result__)) // at least available with GCC 3.4
#  elif defined(__clang__) && defined(__has_attribute)
#    if __has_attribute(__warn_unused_result__)
#      define TI_NODISCARD __attribute__((__warn_unused_result__))
#    endif
#  endif
#endif
#ifndef TI_NODISCARD
#  define TI_NODISCARD /* nothing by default */
#endif


/****************************************************************************************\
*                                    C++ 11                                              *
\****************************************************************************************/
#ifndef TI_CXX11
#  if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1800)
#    define TI_CXX11 1
#  endif
#else
#  if TI_CXX11 == 0
#    undef TI_CXX11
#  endif
#endif
#ifndef TI_CXX11
#  error "Tiger 4.x+ requires enabled C++11 support"
#endif

#define TI_CXX_MOVE_SEMANTICS 1
#define TI_CXX_MOVE(x) std::move(x)
#define TI_CXX_STD_ARRAY 1
#include <array>
#ifndef TI_OVERRIDE
#  define TI_OVERRIDE override
#endif
#ifndef TI_FINAL
#  define TI_FINAL final
#endif

#ifndef TI_NOEXCEPT
#  if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900/*MSVS 2015*/)
#    define TI_NOEXCEPT noexcept
#  endif
#endif
#ifndef TI_NOEXCEPT
#  define TI_NOEXCEPT
#endif

#ifndef TI_CONSTEXPR
#  if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900/*MSVS 2015*/)
#    define TI_CONSTEXPR constexpr
#  endif
#endif
#ifndef TI_CONSTEXPR
#  define TI_CONSTEXPR
#endif

// Integer types portatibility
#ifdef TIGER_STDINT_HEADER
#include TIGER_STDINT_HEADER
#elif defined(__cplusplus)
#if defined(_MSC_VER) && _MSC_VER < 1600 /* MSVS 2010 */
namespace ti {
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
}
#elif defined(_MSC_VER) || __cplusplus >= 201103L
#include <cstdint>

namespace ti
{
    using std::int8_t;
    using std::uint8_t;
    using std::int16_t;
    using std::uint16_t;
    using std::int32_t;
    using std::uint32_t;
    using std::int64_t;
    using std::uint64_t;
}
#else
#include <stdint.h>
namespace ti {
typedef ::int8_t int8_t;
typedef ::uint8_t uint8_t;
typedef ::int16_t int16_t;
typedef ::uint16_t uint16_t;
typedef ::int32_t int32_t;
typedef ::uint32_t uint32_t;
typedef ::int64_t int64_t;
typedef ::uint64_t uint64_t;
}
#endif
#else // pure C
#include <stdint.h>
#endif

//! @}

#endif // TIGER_CORE_TIDEF_H
