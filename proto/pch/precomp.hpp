// ------------------------------------------------------
//  Copyright (C) 2021 SHANGHAI INSTITUTE OF LASER TECHNOLOGY.
//                  - All Rights Reserved -
//           
//  Unauthorized copying of this file, via any medium is strictly prohibited
//  Proprietary and confidential
//  
//  Written by Xiling Huang <huangxiling@silt.top>
//  Created:     2021-04-09    21:46
// ------------------------------------------------------

#ifdef _WIN32
  #ifdef LIBRARY_PROTO
    #define PROTO_EXPORTS __declspec(dllexport)
  #else
    #define PROTO_EXPORTS __declspec(dllimport)
  #endif
#else
  #define PROTO_EXPORTS
#endif


#ifndef PROTOBUF_USE_DLLS
#define PROTOBUF_USE_DLLS
#endif