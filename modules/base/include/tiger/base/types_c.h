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

#ifndef TIGER_CORE_TYPES_H
#define TIGER_CORE_TYPES_H

#ifdef __cplusplus

#ifdef TI__VALIDATE_UNUNITIALIZED_VARS
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define TI_STRUCT_INITIALIZER {0,}
#else
#if defined(__GNUC__) && __GNUC__ == 4  // GCC 4.x warns on "= {}" initialization, fixed in GCC 5.0
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define TI_STRUCT_INITIALIZER {}
#endif

#else
#define TI_STRUCT_INITIALIZER {0}
#endif


#ifdef HAVE_IPL
#  ifndef __IPL_H__
#    if defined _WIN32
#      include <ipl.h>
#    else
#      include <ipl/ipl.h>
#    endif
#  endif
#elif defined __IPL_H__
#  define HAVE_IPL
#endif

#include "tiger/base/tidef.h"

#ifndef SKIP_INCLUDES
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#endif // SKIP_INCLUDES

#if defined _WIN32
#  define TI_CDECL __cdecl
#  define TI_STDCALL __stdcall
#else
#  define TI_CDECL
#  define TI_STDCALL
#endif

#ifndef TI_DEFAULT
#  ifdef __cplusplus
#    define TI_DEFAULT(val) = val
#  else
#    define TI_DEFAULT(val)
#  endif
#endif

#ifndef TI_EXTERN_C_FUNCPTR
#  ifdef __cplusplus
#    define TI_EXTERN_C_FUNCPTR(x) extern "C" { typedef x; }
#  else
#    define TI_EXTERN_C_FUNCPTR(x) typedef x
#  endif
#endif

#ifndef TIAPI
#  define TIAPI(rettype) TI_EXTERN_C TI_EXPORTS rettype TI_CDECL
#endif

#ifndef TI_IMPL
#  define TI_IMPL TI_EXTERN_C
#endif

/****************************************************************************************\
*                             Common macros and inline functions                         *
\****************************************************************************************/

#define TI_SWAP(a,b,t) ((t) = (a), (a) = (b), (b) = (t))

/** min & max without jumps */
#define  TI_IMIN(a, b)  ((a) ^ (((a)^(b)) & (((a) < (b)) - 1)))

#define  TI_IMAX(a, b)  ((a) ^ (((a)^(b)) & (((a) > (b)) - 1)))

/** absolute value without jumps */
#ifndef __cplusplus
#  define  TI_IABS(a)     (((a) ^ ((a) < 0 ? -1 : 0)) - ((a) < 0 ? -1 : 0))
#else
#  define  TI_IABS(a)     abs(a)
#endif
#define  TI_CMP(a,b)    (((a) > (b)) - ((a) < (b)))
#define  TI_SIGN(a)     TI_CMP((a),0)

#define tiInvSqrt(value) ((float)(1./sqrt(value)))
#define tiSqrt(value)  ((float)sqrt(value))

/** @} */

#endif /*TIGER_CORE_TYPES_H*/

/* End of file. */
