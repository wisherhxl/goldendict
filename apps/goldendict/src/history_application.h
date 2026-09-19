// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <vector>
#include "goldendict/core/application.h"
#include "goldendict/core/history_store.h"

class MainWindow;

namespace goldendict::app {

void RefreshHistoryPresentation(MainWindow& window,
                                const std::vector<core::HistoryEntry>& history);

// Install once on the window's GUI thread. Caller-owned configuration and
// history retain their identity and must outlive signal handling. The window is
// the Qt connection context; destruction disconnects. The path is copied into
// callbacks.
void InstallHistoryRecording(MainWindow& window,
                             const core::CoreConfiguration& configuration,
                             std::vector<core::HistoryEntry>& history,
                             QString history_path);
void InstallHistoryImport(MainWindow& window,
                          const core::CoreConfiguration& configuration,
                          std::vector<core::HistoryEntry>& history,
                          QString history_path);

}  // namespace goldendict::app
