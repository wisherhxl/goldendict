// ------------------------------------------------------
//  Copyright (C) 2021 SHANGHAI INSTITUTE OF LASER TECHNOLOGY.
//                  - All Rights Reserved -
//           
//  Unauthorized copying of this file, via any medium is strictly prohibited
//  Proprietary and confidential
//  
//  Written by Xiling Huang <huangxiling@silt.top>
//  Created:     2021-03-17    16:20
// ------------------------------------------------------

#ifndef TIGER_VERSION_HPP
#define TIGER_VERSION_HPP

#define TI_ORG_NAME "SILT"
#define TI_PROJECT_NAME "Tiger - A SILT PLATFORM"

#define TI_VERSION_MAJOR    3
#define TI_VERSION_MINOR    0
#define TI_VERSION_REVISION 8
#define TI_UPDATE_YEAR 2024
#define TI_UPDATE_MONTH 12

#define TIAUX_STR_EXP(__A)  #__A
#define TIAUX_STR(__A)      TIAUX_STR_EXP(__A)

#define TIAUX_STRW_EXP(__A)  L ## #__A
#define TIAUX_STRW(__A)      TIAUX_STRW_EXP(__A)

#define TI_UPDATE_TIME TIAUX_STR(TI_UPDATE_YEAR) "-" TIAUX_STR(TI_UPDATE_MONTH)
#define TI_VERSION_STATUS "-(" TI_UPDATE_TIME ")"
#define TI_VERSION          TIAUX_STR(TI_VERSION_MAJOR) "." TIAUX_STR(TI_VERSION_MINOR) "." TIAUX_STR(TI_VERSION_REVISION) TI_VERSION_STATUS

/* old  style version constants*/
#define TI_MAJOR_VERSION    TI_VERSION_MAJOR
#define TI_MINOR_VERSION    TI_VERSION_MINOR
#define TI_SUBMINOR_VERSION TI_VERSION_REVISION

#endif // TIGER_VERSION_HPP
