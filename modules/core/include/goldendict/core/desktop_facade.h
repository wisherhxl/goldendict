// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_DESKTOP_FACADE_H_
#define GOLDENDICT_CORE_DESKTOP_FACADE_H_

#include "goldendict/base/goldendict_def.tp.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core {

class GOLDENDICT_EXPORTS DesktopFacade {
   public:
    virtual ~DesktopFacade();

    virtual DictionaryService& GetDictionaryService() noexcept = 0;
    virtual const DictionaryService& GetDictionaryService() const noexcept = 0;
};

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_DESKTOP_FACADE_H_
