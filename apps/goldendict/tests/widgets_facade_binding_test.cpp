// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgets_facade_binding.h"

int main() {
    return goldendict::widgets::WidgetsFacadeBindingRegistry::
                   RunClosedLeaseProtocolSmokeCheck()
               ? 0
               : 1;
}
