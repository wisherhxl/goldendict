# SPDX-License-Identifier: GPL-3.0-or-later

ti_add_qt_app(goldendict MODULES tiger EXTRAS ${qt_link})

add_test(NAME goldendict_smoke COMMAND $<TARGET_FILE:goldendict> --smoke)
