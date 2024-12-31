/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-04-09
 */

#ifdef LIBRARY_PROTO
#define PROTO_EXPORTS __declspec(dllexport)
#else
#define PROTO_EXPORTS __declspec(dllimport)
#endif

#ifndef PROTOBUF_USE_DLLS
#define PROTOBUF_USE_DLLS
#endif