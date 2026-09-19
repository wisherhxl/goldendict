// SPDX-License-Identifier: GPL-3.0-or-later
#include "webengine_storage_paths.h"

#include "legacy_configuration_location.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QVariant>
#include <QWebEngineProfile>

#include <stdexcept>

namespace goldendict::app {
namespace {
constexpr auto kStorageRoot = "goldendict.webengineStorageRoot";

QString ValidateRoot(const QString& root) {
    if (root.isEmpty() || !QDir::isAbsolutePath(root))
        throw std::runtime_error(
            "WebEngine storage root must be nonempty and absolute");
    const auto result = QDir::cleanPath(root);
    for (QFileInfo entry(result);; entry.setFile(entry.absolutePath())) {
        if (entry.isSymLink())
            throw std::runtime_error(
                "WebEngine storage path must not traverse a symlink");
        if (entry.absoluteFilePath() == entry.absolutePath())
            break;
    }
    return result;
}

void PrepareDirectory(const QString& path) {
    ValidateRoot(path);
    if (!QDir().mkpath(path))
        throw std::runtime_error(
            "Cannot create selected WebEngine storage directory");
    QTemporaryFile probe(QDir(path).filePath(".write-probe-XXXXXX"));
    if (!probe.open() || probe.write("probe", 5) != 5 || !probe.flush())
        throw std::runtime_error(
            "Selected WebEngine storage directory is not writable");
}
}  // namespace

void InitializeWebEngineStorage(const ConfigurationLocations& locations,
                                const std::optional<QString>& explicit_root) {
    if (!explicit_root && !locations.portable)
        return;
    const auto root = ValidateRoot(
        explicit_root
            ? *explicit_root
            : QString::fromStdString(
                  (locations.current_configuration_path.parent_path() /
                   "webengine")
                      .string()));
    // Application metadata avoids constructing the singleton on invalid input
    // and prevents a second initialization from resetting a live profile.
    auto* application = QCoreApplication::instance();
    const auto previous = application->property(kStorageRoot).toString();
    if (!previous.isEmpty()) {
        if (previous != root)
            throw std::runtime_error(
                "WebEngine storage is already initialized at another root");
        return;
    }
    const auto article = QDir(root).filePath("article");
    PrepareDirectory(article);
    PrepareDirectory(QDir(root).filePath("inspectors"));
    auto* profile = QWebEngineProfile::defaultProfile();
    profile->setPersistentStoragePath(article);
    profile->setProperty(kStorageRoot, root);
    application->setProperty(kStorageRoot, root);
}

std::unique_ptr<QTemporaryDir> InitializeInspectorWebEngineStorage(
    QWebEngineProfile& inspector, const QWebEngineProfile& inspected) {
    const auto root = inspected.property(kStorageRoot).toString();
    if (root.isEmpty())
        return {};
    const auto parent = QDir(root).filePath("inspectors");
    PrepareDirectory(parent);
    auto directory = std::make_unique<QTemporaryDir>(
        QDir(parent).filePath("profile-XXXXXX"));
    if (!directory->isValid())
        throw std::runtime_error(
            "Cannot create selected Inspector storage directory");
    inspector.setPersistentStoragePath(directory->path());
    return directory;
}
}  // namespace goldendict::app
