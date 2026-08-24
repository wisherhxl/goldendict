// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_HELP_WINDOW_H_
#define GOLDENDICT_APPS_GOLDENDICT_HELP_WINDOW_H_

#include <memory>

#include <QDialog>
#include <QString>

class QHelpEngine;
class QTemporaryDir;
class QTextBrowser;
class QUrl;

namespace goldendict::app {

enum class HelpIntent {
    kReference,
    kFullTextSearch,
    kDictionaryHeadwords,
};

QString HelpIdentifier(HelpIntent intent);
QString SelectHelpCollectionName(const QString& help_language,
                                 const QString& interface_language,
                                 const QString& system_locale);
QString InstalledHelpDirectory();
bool IsExternalHelpUrl(const QUrl& url);

class HelpWindow final : public QDialog {
   public:
    HelpWindow(const QString& help_directory, const QString& help_language,
               const QString& interface_language, const QString& system_locale,
               QWidget* parent = nullptr);
    ~HelpWindow() override;

    bool IsReady() const noexcept;
    QString CollectionPath() const;
    bool ShowIdentifier(const QString& identifier);

   private:
    class Browser;

    std::unique_ptr<QTemporaryDir> collection_directory_;
    QHelpEngine* help_engine_ = nullptr;
    Browser* browser_ = nullptr;
    QString collection_path_;
};

}  // namespace goldendict::app

#endif  // GOLDENDICT_APPS_GOLDENDICT_HELP_WINDOW_H_
