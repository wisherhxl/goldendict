/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-03-18
 */

#if defined __TIGER_BUILD
#include "ti_cpu_config.h"
#include "ti_cpu_helper.h"

#ifdef TI_CPU_DISPATCH_MODE
#define TI_CPU_OPTIMIZATION_NAMESPACE __TI_CAT(opt_, TI_CPU_DISPATCH_MODE)
#define TI_CPU_OPTIMIZATION_NAMESPACE_BEGIN \
    namespace __TI_CAT(opt_, TI_CPU_DISPATCH_MODE) {
#define TI_CPU_OPTIMIZATION_NAMESPACE_END }
#else
#define TI_CPU_OPTIMIZATION_NAMESPACE cpu_baseline
#define TI_CPU_OPTIMIZATION_NAMESPACE_BEGIN namespace cpu_baseline {
#define TI_CPU_OPTIMIZATION_NAMESPACE_END }
#define TI_CPU_BASELINE_MODE 1
#endif

#define __TI_CPU_DISPATCH_CHAIN_END(fn, args, mode, ...) /* done */
#define __TI_CPU_DISPATCH(fn, args, mode, ...) \
    __TI_EXPAND(__TI_CPU_DISPATCH_CHAIN_##mode(fn, args, __VA_ARGS__))
#define __TI_CPU_DISPATCH_EXPAND(fn, args, ...) \
    __TI_EXPAND(__TI_CPU_DISPATCH(fn, args, __VA_ARGS__))
#define TI_CPU_DISPATCH(fn, args, ...) \
    __TI_CPU_DISPATCH_EXPAND(fn, args, __VA_ARGS__, END)  // expand macros

#if defined TI_ENABLE_INTRINSICS && !defined TI_DISABLE_OPTIMIZATION && \
    !defined __CUDACC__ /* do not include SSE/AVX/NEON headers for NVCC \
                           compiler */

#ifdef TI_CPU_COMPILE_SSE2
#include <emmintrin.h>
#define TI_MMX 1
#define TI_SSE 1
#define TI_SSE2 1
#endif
#ifdef TI_CPU_COMPILE_SSE3
#include <pmmintrin.h>
#define TI_SSE3 1
#endif
#ifdef TI_CPU_COMPILE_SSSE3
#include <tmmintrin.h>
#define TI_SSSE3 1
#endif
#ifdef TI_CPU_COMPILE_SSE4_1
#include <smmintrin.h>
#define TI_SSE4_1 1
#endif
#ifdef TI_CPU_COMPILE_SSE4_2
#include <nmmintrin.h>
#define TI_SSE4_2 1
#endif
#ifdef TI_CPU_COMPILE_POPCNT
#ifdef _MSC_VER
#include <nmmintrin.h>
#if defined(_M_X64)
#define TI_POPCNT_U64 _mm_popcnt_u64
#endif
#define TI_POPCNT_U32 _mm_popcnt_u32
#else
#include <popcntintrin.h>
#if defined(__x86_64__)
#define TI_POPCNT_U64 __builtin_popcountll
#endif
#define TI_POPCNT_U32 __builtin_popcount
#endif
#define TI_POPCNT 1
#endif
#ifdef TI_CPU_COMPILE_AVX
#include <immintrin.h>
#define TI_AVX 1
#endif
#ifdef TI_CPU_COMPILE_FP16
#if defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || \
    defined(_M_ARM64)
#include <arm_neon.h>
#else
#include <immintrin.h>
#endif
#define TI_FP16 1
#endif
#ifdef TI_CPU_COMPILE_AVX2
#include <immintrin.h>
#define TI_AVX2 1
#endif
#ifdef TI_CPU_COMPILE_AVX_512F
#include <immintrin.h>
#define TI_AVX_512F 1
#endif
#ifdef TI_CPU_COMPILE_AVX512_COMMON
#define TI_AVX512_COMMON 1
#define TI_AVX_512CD 1
#endif
#ifdef TI_CPU_COMPILE_AVX512_KNL
#define TI_AVX512_KNL 1
#define TI_AVX_512ER 1
#define TI_AVX_512PF 1
#endif
#ifdef TI_CPU_COMPILE_AVX512_KNM
#define TI_AVX512_KNM 1
#define TI_AVX_5124FMAPS 1
#define TI_AVX_5124VNNIW 1
#define TI_AVX_512VPOPCNTDQ 1
#endif
#ifdef TI_CPU_COMPILE_AVX512_SKX
#define TI_AVX512_SKX 1
#define TI_AVX_512VL 1
#define TI_AVX_512BW 1
#define TI_AVX_512DQ 1
#endif
#ifdef TI_CPU_COMPILE_AVX512_CNL
#define TI_AVX512_CNL 1
#define TI_AVX_512IFMA 1
#define TI_AVX_512VBMI 1
#endif
#ifdef TI_CPU_COMPILE_AVX512_CLX
#define TI_AVX512_CLX 1
#define TI_AVX_512VNNI 1
#endif
#ifdef TI_CPU_COMPILE_AVX512_ICL
#define TI_AVX512_ICL 1
#undef TI_AVX_512IFMA
#define TI_AVX_512IFMA 1
#undef TI_AVX_512VBMI
#define TI_AVX_512VBMI 1
#undef TI_AVX_512VNNI
#define TI_AVX_512VNNI 1
#define TI_AVX_512VBMI2 1
#define TI_AVX_512BITALG 1
#define TI_AVX_512VPOPCNTDQ 1
#endif
#ifdef TI_CPU_COMPILE_FMA3
#define TI_FMA3 1
#endif

#if defined _WIN32 && (defined(_M_ARM) || defined(_M_ARM64)) && \
    (defined(TI_CPU_COMPILE_NEON) || !defined(_MSC_VER))
#include <Intrin.h>
#include <arm_neon.h>
#define TI_NEON 1
#elif defined(__ARM_NEON__) || (defined(__ARM_NEON) && defined(__aarch64__))
#include <arm_neon.h>
#define TI_NEON 1
#endif

#if defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#endif

#ifdef TI_CPU_COMPILE_VSX
#include <altivec.h>
#undef vector
#undef pixel
#undef bool
#define TI_VSX 1
#endif

#ifdef TI_CPU_COMPILE_VSX3
#define TI_VSX3 1
#endif

#ifdef TI_CPU_COMPILE_MSA
#include "hal/msa_macros.h"
#define TI_MSA 1
#endif

#ifdef __EMSCRIPTEN__
#define TI_WASM_SIMD 1
#include <wasm_simd128.h>
#endif

#if defined TI_CPU_COMPILE_RVV
#define TI_RVV 1
#include <riscv_vector.h>
#endif

#endif  // TI_ENABLE_INTRINSICS && !TI_DISABLE_OPTIMIZATION && !__CUDACC__

#if defined TI_CPU_COMPILE_AVX && !defined TI_CPU_BASELINE_COMPILE_AVX
struct VZeroUpperGuard {
#ifdef __GNUC__
    __attribute__((always_inline))
#endif
    inline VZeroUpperGuard() {
        _mm256_zeroupper();
    }
#ifdef __GNUC__
    __attribute__((always_inline))
#endif
    inline ~VZeroUpperGuard() {
        _mm256_zeroupper();
    }
};

#define __TI_AVX_GUARD                  \
    VZeroUpperGuard __vzeroupper_guard; \
    TI_UNUSED(__vzeroupper_guard);
#endif

#ifdef __TI_AVX_GUARD
#define TI_AVX_GUARD __TI_AVX_GUARD
#else
#define TI_AVX_GUARD
#endif

#endif  // __TIGER_BUILD

#if !defined __TIGER_BUILD /* Compatibility code */                        \
    && !defined __CUDACC__ /* do not include SSE/AVX/NEON headers for NVCC \
                              compiler */
#if defined __SSE2__ || defined _M_X64 || \
    (defined _M_IX86_FP && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define TI_MMX 1
#define TI_SSE 1
#define TI_SSE2 1
#elif defined _WIN32 && (defined(_M_ARM) || defined(_M_ARM64)) && \
    (defined(TI_CPU_COMPILE_NEON) || !defined(_MSC_VER))
#include <Intrin.h>
#include <arm_neon.h>
#define TI_NEON 1
#elif defined(__ARM_NEON__) || (defined(__ARM_NEON) && defined(__aarch64__))
#include <arm_neon.h>
#define TI_NEON 1
#elif defined(__VSX__) && defined(__PPC64__) && defined(__LITTLE_ENDIAN__)
#include <altivec.h>
#undef vector
#undef pixel
#undef bool
#define TI_VSX 1
#endif

#ifdef __F16C__
#include <immintrin.h>
#define TI_FP16 1
#endif

#endif  // !__TIGER_BUILD && !__CUDACC (Compatibility code)

#ifndef TI_MMX
#define TI_MMX 0
#endif
#ifndef TI_SSE
#define TI_SSE 0
#endif
#ifndef TI_SSE2
#define TI_SSE2 0
#endif
#ifndef TI_SSE3
#define TI_SSE3 0
#endif
#ifndef TI_SSSE3
#define TI_SSSE3 0
#endif
#ifndef TI_SSE4_1
#define TI_SSE4_1 0
#endif
#ifndef TI_SSE4_2
#define TI_SSE4_2 0
#endif
#ifndef TI_POPCNT
#define TI_POPCNT 0
#endif
#ifndef TI_AVX
#define TI_AVX 0
#endif
#ifndef TI_FP16
#define TI_FP16 0
#endif
#ifndef TI_AVX2
#define TI_AVX2 0
#endif
#ifndef TI_FMA3
#define TI_FMA3 0
#endif
#ifndef TI_AVX_512F
#define TI_AVX_512F 0
#endif
#ifndef TI_AVX_512BW
#define TI_AVX_512BW 0
#endif
#ifndef TI_AVX_512CD
#define TI_AVX_512CD 0
#endif
#ifndef TI_AVX_512DQ
#define TI_AVX_512DQ 0
#endif
#ifndef TI_AVX_512ER
#define TI_AVX_512ER 0
#endif
#ifndef TI_AVX_512IFMA
#define TI_AVX_512IFMA 0
#endif
#define TI_AVX_512IFMA512 TI_AVX_512IFMA  // deprecated
#ifndef TI_AVX_512PF
#define TI_AVX_512PF 0
#endif
#ifndef TI_AVX_512VBMI
#define TI_AVX_512VBMI 0
#endif
#ifndef TI_AVX_512VL
#define TI_AVX_512VL 0
#endif
#ifndef TI_AVX_5124FMAPS
#define TI_AVX_5124FMAPS 0
#endif
#ifndef TI_AVX_5124VNNIW
#define TI_AVX_5124VNNIW 0
#endif
#ifndef TI_AVX_512VPOPCNTDQ
#define TI_AVX_512VPOPCNTDQ 0
#endif
#ifndef TI_AVX_512VNNI
#define TI_AVX_512VNNI 0
#endif
#ifndef TI_AVX_512VBMI2
#define TI_AVX_512VBMI2 0
#endif
#ifndef TI_AVX_512BITALG
#define TI_AVX_512BITALG 0
#endif
#ifndef TI_AVX512_COMMON
#define TI_AVX512_COMMON 0
#endif
#ifndef TI_AVX512_KNL
#define TI_AVX512_KNL 0
#endif
#ifndef TI_AVX512_KNM
#define TI_AVX512_KNM 0
#endif
#ifndef TI_AVX512_SKX
#define TI_AVX512_SKX 0
#endif
#ifndef TI_AVX512_CNL
#define TI_AVX512_CNL 0
#endif
#ifndef TI_AVX512_CLX
#define TI_AVX512_CLX 0
#endif
#ifndef TI_AVX512_ICL
#define TI_AVX512_ICL 0
#endif

#ifndef TI_NEON
#define TI_NEON 0
#endif

#ifndef TI_VSX
#define TI_VSX 0
#endif

#ifndef TI_VSX3
#define TI_VSX3 0
#endif

#ifndef TI_MSA
#define TI_MSA 0
#endif

#ifndef TI_WASM_SIMD
#define TI_WASM_SIMD 0
#endif

#ifndef TI_RVV
#define TI_RVV 0
#endif
