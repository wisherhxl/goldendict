// SPDX-License-Identifier: GPL-3.0-or-later
#include "history_application.h"

#include <QMessageBox>
#include <algorithm>
#include "main_window.h"

namespace goldendict::app {

void RefreshHistoryPresentation(
    MainWindow& window, const std::vector<core::HistoryEntry>& history) {
    std::vector<HistoryViewItem> items;
    items.reserve(history.size());
    for (const auto& entry : history) {
        items.push_back({QString::fromStdString(entry.word), entry.group_id});
    }
    window.SetHistoryItems(items);
}

void InstallHistoryRecording(MainWindow& window,
                             const core::CoreConfiguration& configuration,
                             std::vector<core::HistoryEntry>& history,
                             QString history_path) {
    QObject::connect(
        &window, &MainWindow::LookupSubmitted, &window,
        [&window, &configuration, &history, history_path](
            const QString& word, std::uint32_t group_id) {
            if (!configuration.preferences.store_history ||
                configuration.preferences.maximum_history_entries == 0U) {
                return;
            }
            auto updated = history;
            const std::string encoded = word.toStdString();
            updated.erase(std::remove_if(
                              updated.begin(), updated.end(),
                              [&word](const auto& entry) {
                                  return QString::fromStdString(entry.word)
                                             .compare(word,
                                                      Qt::CaseInsensitive) == 0;
                              }),
                          updated.end());
            updated.insert(updated.begin(), {group_id, encoded});
            if (updated.size() >
                configuration.preferences.maximum_history_entries) {
                updated.resize(
                    configuration.preferences.maximum_history_entries);
            }
            try {
                goldendict::core::SaveHistory(history_path.toStdString(),
                                              updated);
                history = std::move(updated);
                RefreshHistoryPresentation(window, history);
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict history"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
}

void InstallHistoryImport(MainWindow& window,
                          const core::CoreConfiguration& configuration,
                          std::vector<core::HistoryEntry>& history,
                          QString history_path) {
    QObject::connect(
        &window, &MainWindow::ImportHistoryRequested, &window,
        [&window, &configuration, &history, history_path](
            const QString& path, std::uint32_t group_id) {
            try {
                auto imported = goldendict::core::ImportHistoryText(
                    path.toStdString(),
                    configuration.preferences.maximum_history_entries,
                    group_id);
                goldendict::core::SaveHistory(history_path.toStdString(),
                                              imported);
                history = std::move(imported);
                RefreshHistoryPresentation(window, history);
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict history"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
}

}  // namespace goldendict::app
