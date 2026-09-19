// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <memory>
#include <optional>

class QTemporaryDir;
class QWebEngineProfile;

namespace goldendict::app {
struct ConfigurationLocations;

// Called once by application composition, before creating any WebEngine view.
void InitializeWebEngineStorage(
    const ConfigurationLocations& locations,
    const std::optional<QString>& explicit_root = std::nullopt);

// The returned directory must outlive the Inspector page and its profile.
std::unique_ptr<QTemporaryDir> InitializeInspectorWebEngineStorage(
    QWebEngineProfile& inspector, const QWebEngineProfile& inspected);
}  // namespace goldendict::app
