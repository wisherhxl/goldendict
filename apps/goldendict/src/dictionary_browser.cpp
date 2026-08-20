// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_browser.h"

#include <limits>
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "goldendict/core/desktop_facade.h"

DictionaryBrowser::DictionaryBrowser(QWidget* parent) : QDialog(parent) {
    setObjectName(QStringLiteral("dictionaryBrowser"));
    setWindowTitle(QStringLiteral("Dictionary Information and Headwords"));
    resize(640, 520);

    auto* layout = new QVBoxLayout(this);
    dictionaries_ = new QComboBox(this);
    dictionaries_->setObjectName(QStringLiteral("dictionarySelector"));
    layout->addWidget(dictionaries_);

    auto* details = new QFormLayout();
    identifier_ = new QLabel(this);
    identifier_->setObjectName(QStringLiteral("dictionaryIdentifier"));
    identifier_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    edition_ = new QLabel(this);
    edition_->setObjectName(QStringLiteral("dictionaryEdition"));
    edition_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    source_ = new QLabel(this);
    source_->setObjectName(QStringLiteral("dictionarySource"));
    source_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    source_->setWordWrap(true);
    source_language_ = new QLabel(this);
    source_language_->setObjectName(QStringLiteral("dictionarySourceLanguage"));
    source_language_->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                              Qt::TextSelectableByKeyboard);
    target_language_ = new QLabel(this);
    target_language_->setObjectName(QStringLiteral("dictionaryTargetLanguage"));
    target_language_->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                              Qt::TextSelectableByKeyboard);
    description_ = new QPlainTextEdit(this);
    description_->setObjectName(QStringLiteral("dictionaryDescription"));
    description_->setReadOnly(true);
    description_->setUndoRedoEnabled(false);
    description_->setMaximumHeight(120);
    article_count_ = new QLabel(this);
    article_count_->setObjectName(QStringLiteral("dictionaryArticleCount"));
    article_count_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headword_count_ = new QLabel(this);
    headword_count_->setObjectName(QStringLiteral("dictionaryHeadwordCount"));
    headword_count_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    details->addRow(QStringLiteral("Identifier:"), identifier_);
    details->addRow(QStringLiteral("Edition:"), edition_);
    details->addRow(QStringLiteral("Source:"), source_);
    details->addRow(QStringLiteral("Translates from:"), source_language_);
    details->addRow(QStringLiteral("Translates to:"), target_language_);
    details->addRow(QStringLiteral("Description:"), description_);
    details->addRow(QStringLiteral("Articles:"), article_count_);
    details->addRow(QStringLiteral("Headwords:"), headword_count_);
    layout->addLayout(details);

    auto* source_actions = new QHBoxLayout();
    copy_source_ = new QPushButton(QStringLiteral("Copy Source Path"), this);
    copy_source_->setObjectName(QStringLiteral("copyDictionarySource"));
    open_source_directory_ =
        new QPushButton(QStringLiteral("Open Containing Folder"), this);
    open_source_directory_->setObjectName(
        QStringLiteral("openDictionarySourceDirectory"));
    source_actions->addWidget(copy_source_);
    source_actions->addWidget(open_source_directory_);
    source_actions->addStretch(1);
    layout->addLayout(source_actions);

    auto* filter_controls = new QHBoxLayout();
    filter_mode_ = new QComboBox(this);
    filter_mode_->setObjectName(QStringLiteral("headwordFilterMode"));
    filter_mode_->addItem(
        QStringLiteral("Prefix"),
        static_cast<int>(goldendict::core::HeadwordFilterMode::kPrefix));
    filter_mode_->addItem(
        QStringLiteral("Wildcards"),
        static_cast<int>(goldendict::core::HeadwordFilterMode::kWildcard));
    filter_mode_->addItem(
        QStringLiteral("Regular expression"),
        static_cast<int>(
            goldendict::core::HeadwordFilterMode::kRegularExpression));
    match_case_ = new QCheckBox(QStringLiteral("Match case"), this);
    match_case_->setObjectName(QStringLiteral("headwordMatchCase"));
    match_case_->setEnabled(false);
    filter_controls->addWidget(filter_mode_);
    filter_controls->addWidget(match_case_);
    filter_controls->addStretch(1);
    layout->addLayout(filter_controls);

    prefix_ = new QLineEdit(this);
    prefix_->setObjectName(QStringLiteral("headwordPrefix"));
    prefix_->setPlaceholderText(QStringLiteral("Enter a headword prefix"));
    prefix_->setClearButtonEnabled(true);
    layout->addWidget(prefix_);
    headwords_ = new QListWidget(this);
    headwords_->setObjectName(QStringLiteral("headwordList"));
    headwords_->setAlternatingRowColors(true);
    headwords_->setUniformItemSizes(true);
    layout->addWidget(headwords_, 1);
    result_status_ = new QLabel(this);
    result_status_->setObjectName(QStringLiteral("headwordStatus"));
    layout->addWidget(result_status_);
    export_headwords_ =
        new QPushButton(QStringLiteral("Export All Headwords..."), this);
    export_headwords_->setObjectName(QStringLiteral("exportHeadwords"));
    export_headwords_->setEnabled(false);
    layout->addWidget(export_headwords_);

    connect(dictionaries_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() {
                RefreshDictionaryInfo();
                RefreshHeadwords();
            });
    connect(prefix_, &QLineEdit::textChanged, this,
            &DictionaryBrowser::RefreshHeadwords);
    connect(filter_mode_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this]() {
                const auto mode =
                    static_cast<goldendict::core::HeadwordFilterMode>(
                        filter_mode_->currentData().toInt());
                match_case_->setEnabled(
                    mode != goldendict::core::HeadwordFilterMode::kPrefix);
                prefix_->setPlaceholderText(
                    mode == goldendict::core::HeadwordFilterMode::kPrefix
                        ? QStringLiteral("Enter a headword prefix")
                        : QStringLiteral(
                              "Enter a pattern with a leading literal prefix"));
                RefreshHeadwords();
            });
    connect(match_case_, &QCheckBox::toggled, this,
            &DictionaryBrowser::RefreshHeadwords);
    connect(headwords_, &QListWidget::itemActivated, this,
            [this](const QListWidgetItem* item) {
                if (item != nullptr) {
                    emit HeadwordSelected(item->text());
                }
            });
    connect(export_headwords_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save headwords to file"),
            DefaultExportFileName(),
            QStringLiteral("Text files (*.txt);;All files (*.*)"));
        if (path.isEmpty()) {
            result_status_->setText(
                QStringLiteral("Headword export cancelled"));
            return;
        }
        StartHeadwordExport(path);
    });
    connect(copy_source_, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(CurrentSourcePath());
    });
    connect(open_source_directory_, &QPushButton::clicked, this, [this]() {
        const QString directory = CurrentSourceDirectory();
        if (!directory.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
        }
    });
}

DictionaryBrowser::~DictionaryBrowser() {
    QuiesceBindingConsumer(true);
}

void DictionaryBrowser::RunExportSmokeCheck(
    const QString& path, const QString& prefix, const QByteArray& expected,
    std::function<void(bool)> completion) {
    prefix_->setText(prefix);
    QTimer::singleShot(
        0, this,
        [this, path, expected, completion = std::move(completion)]() mutable {
            StartHeadwordExport(path, [path, expected,
                                       completion = std::move(completion)](
                                          bool exported) mutable {
                QFile file(path);
                const bool opened = file.open(QIODevice::ReadOnly);
                completion(exported && opened && file.readAll() == expected);
            });
        });
}

bool DictionaryBrowser::StartLifecycleExportForTest(const QString& path) {
    StartHeadwordExport(path);
    return export_operation_ != nullptr;
}

void DictionaryBrowser::SetFacade(goldendict::core::DesktopFacade* facade) {
    facade_ = facade;
    catalog_ = facade_ == nullptr
                   ? std::vector<goldendict::core::DictionaryIdentity>{}
                   : facade_->GetDictionaryService().GetCatalog();
    dictionaries_->clear();
    for (const auto& dictionary : catalog_) {
        dictionaries_->addItem(QString::fromStdString(dictionary.name),
                               QString::fromStdString(dictionary.id));
    }
    RefreshDictionaryInfo();
    RefreshHeadwords();
}

void DictionaryBrowser::SetBindingRegistry(
    const goldendict::widgets::WidgetsFacadeBindingRegistry*
        registry) noexcept {
    registry_ = registry;
    binding_acquisition_enabled_ = registry != nullptr;
}

void DictionaryBrowser::QuiesceBindingConsumer(bool shutting_down) noexcept {
    binding_acquisition_enabled_ = false;
    registry_ = nullptr;
    if (export_poll_timer_ != nullptr) {
        export_poll_timer_->stop();
        export_poll_timer_->disconnect(this);
        delete export_poll_timer_;
        export_poll_timer_ = nullptr;
    }
    if (export_progress_ != nullptr) {
        export_progress_->disconnect(this);
        export_progress_->close();
        delete export_progress_;
        export_progress_ = nullptr;
    }
    if (export_operation_ != nullptr) {
        export_operation_->Cancel();
        static_cast<void>(export_operation_->Await());
        export_operation_.reset();
        if (!shutting_down && result_status_ != nullptr)
            result_status_->setText(
                QStringLiteral("Headword export cancelled"));
    }
    export_binding_ = {};
    if (!shutting_down) {
        dictionaries_->setEnabled(true);
        export_headwords_->setEnabled(false);
    }
}

void DictionaryBrowser::RunSmokeCheck(const QString& expected_dictionary,
                                      const QString& prefix,
                                      const QString& expected_headword,
                                      std::function<void(bool)> completion) {
    prefix_->setText(prefix);
    QTimer::singleShot(
        0, this,
        [this, expected_dictionary, prefix, expected_headword,
         completion = std::move(completion)]() mutable {
            const auto matches =
                headwords_->findItems(expected_headword, Qt::MatchFixedString);
            copy_source_->click();
            const bool passed =
                dictionaries_->currentText() == expected_dictionary &&
                !identifier_->text().isEmpty() && !source_->text().isEmpty() &&
                article_count_->text() == QStringLiteral("3") &&
                headword_count_->text() == QStringLiteral("3") &&
                source_language_->text() == QStringLiteral("Not specified") &&
                target_language_->text() == QStringLiteral("Not specified") &&
                description_->isReadOnly() &&
                description_->toPlainText() ==
                    QStringLiteral("Not specified") &&
                copy_source_->isEnabled() &&
                open_source_directory_->isEnabled() &&
                QApplication::clipboard()->text() == source_->text() &&
                matches.size() == 1;
            if (!passed) {
                completion(false);
                return;
            }
            headwords_->setCurrentItem(matches.front());
            filter_mode_->setCurrentIndex(1);
            prefix_->setText(QStringLiteral("app*"));
            const bool wildcard_passed =
                match_case_->isEnabled() && headwords_->count() == 2 &&
                headwords_->currentItem() != nullptr &&
                headwords_->currentItem()->text() == expected_headword;
            filter_mode_->setCurrentIndex(2);
            prefix_->setText(QStringLiteral("app(?:le|lication)"));
            const bool regex_passed = headwords_->count() == 2;
            prefix_->setText(QStringLiteral("app("));
            const bool invalid_passed = headwords_->count() == 0 &&
                                        export_headwords_->isEnabled() &&
                                        !result_status_->text().isEmpty();
            filter_mode_->setCurrentIndex(0);
            prefix_->setText(prefix);
            const auto restored =
                headwords_->findItems(expected_headword, Qt::MatchFixedString);
            if (restored.size() != 1) {
                completion(false);
                return;
            }
            emit HeadwordSelected(restored.front()->text());
            completion(wildcard_passed && regex_passed && invalid_passed);
        });
}

void DictionaryBrowser::RefreshDictionaryInfo() {
    const int index = dictionaries_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= catalog_.size()) {
        identifier_->clear();
        edition_->clear();
        source_->clear();
        source_language_->clear();
        target_language_->clear();
        description_->clear();
        article_count_->clear();
        headword_count_->clear();
        export_headwords_->setToolTip(QString());
        copy_source_->setEnabled(false);
        open_source_directory_->setEnabled(false);
        return;
    }
    const auto& dictionary = catalog_[static_cast<std::size_t>(index)];
    identifier_->setText(QString::fromStdString(dictionary.id));
    edition_->setText(dictionary.edition.empty()
                          ? QStringLiteral("Not specified")
                          : QString::fromStdString(dictionary.edition));
    source_->setText(QString::fromStdString(dictionary.source));
    source_language_->setText(
        dictionary.source_language.empty()
            ? QStringLiteral("Not specified")
            : QString::fromStdString(dictionary.source_language));
    target_language_->setText(
        dictionary.target_language.empty()
            ? QStringLiteral("Not specified")
            : QString::fromStdString(dictionary.target_language));
    description_->setPlainText(
        dictionary.description.empty()
            ? QStringLiteral("Not specified")
            : QString::fromStdString(dictionary.description));
    article_count_->setText(QString::number(dictionary.article_count));
    headword_count_->setText(QString::number(dictionary.headword_count));
    export_headwords_->setToolTip(
        dictionary.supports_headword_enumeration
            ? QString()
            : QStringLiteral(
                  "This dictionary does not support complete headword export"));
    const QFileInfo source(CurrentSourcePath());
    copy_source_->setEnabled(!CurrentSourcePath().isEmpty());
    open_source_directory_->setEnabled(source.exists());
}

QString DictionaryBrowser::CurrentSourcePath() const {
    const int index = dictionaries_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= catalog_.size()) {
        return {};
    }
    return QString::fromStdString(
        catalog_[static_cast<std::size_t>(index)].source);
}

QString DictionaryBrowser::CurrentSourceDirectory() const {
    const QFileInfo source(CurrentSourcePath());
    if (!source.exists()) {
        return {};
    }
    return source.isDir() ? source.absoluteFilePath() : source.absolutePath();
}

void DictionaryBrowser::RefreshHeadwords() {
    auto binding =
        !binding_acquisition_enabled_ || registry_ == nullptr
            ? goldendict::widgets::WidgetsFacadeBindingRegistry::Lease{}
            : registry_->Acquire();
    auto* bound_facade = binding                        ? binding->facade
                         : binding_acquisition_enabled_ ? facade_
                                                        : nullptr;
    const QString selected_headword = headwords_->currentItem() == nullptr
                                          ? QString()
                                          : headwords_->currentItem()->text();
    headwords_->clear();
    export_headwords_->setEnabled(false);
    const int index = dictionaries_->currentIndex();
    const auto mode = static_cast<goldendict::core::HeadwordFilterMode>(
        filter_mode_->currentData().toInt());
    const QString prefix = mode == goldendict::core::HeadwordFilterMode::kPrefix
                               ? prefix_->text().trimmed()
                               : prefix_->text();
    if (bound_facade == nullptr || index < 0 ||
        static_cast<std::size_t>(index) >= catalog_.size()) {
        result_status_->setText(QStringLiteral("No dictionary selected"));
        return;
    }
    export_headwords_->setEnabled(catalog_[static_cast<std::size_t>(index)]
                                      .supports_headword_enumeration &&
                                  export_operation_ == nullptr);
    if (prefix.isEmpty()) {
        result_status_->setText(
            mode == goldendict::core::HeadwordFilterMode::kPrefix
                ? QStringLiteral("Enter a prefix to browse")
                : QStringLiteral("Enter a pattern to filter"));
        return;
    }

    goldendict::core::SuggestionQuery query;
    query.text = prefix.toStdString();
    query.dictionary_ids = {catalog_[static_cast<std::size_t>(index)].id};
    query.result_limit = 100U;
    query.filter_mode = mode;
    query.match_case = match_case_->isChecked();
    const auto response = bound_facade->GetDictionaryService().Suggest(query);
    for (const auto& suggestion : response.suggestions) {
        headwords_->addItem(QString::fromStdString(suggestion.headword));
    }
    if (!response.errors.empty() && response.suggestions.empty()) {
        result_status_->setText(
            QString::fromStdString(response.errors.front().message));
        return;
    }
    if (!selected_headword.isEmpty()) {
        const auto retained =
            headwords_->findItems(selected_headword, Qt::MatchFixedString);
        if (!retained.empty()) {
            headwords_->setCurrentItem(retained.front());
        }
    }
    result_status_->setText(
        tr("%1 headword(s); at most 100 shown")
            .arg(static_cast<qulonglong>(response.suggestions.size())));
}

QString DictionaryBrowser::DefaultExportFileName() const {
    const int index = dictionaries_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= catalog_.size()) {
        return QStringLiteral("headwords.txt");
    }
    QString name =
        QString::fromStdString(catalog_[static_cast<std::size_t>(index)].name);
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")),
                 QStringLiteral("_"));
    name = name.trimmed();
    return (name.isEmpty() ? QStringLiteral("headwords") : name) +
           QStringLiteral(".txt");
}

void DictionaryBrowser::StartHeadwordExport(
    const QString& path, std::function<void(bool)> completion) {
    auto binding =
        !binding_acquisition_enabled_ || registry_ == nullptr
            ? goldendict::widgets::WidgetsFacadeBindingRegistry::Lease{}
            : registry_->Acquire();
    auto* bound_facade = binding                        ? binding->facade
                         : binding_acquisition_enabled_ ? facade_
                                                        : nullptr;
    const int index = dictionaries_->currentIndex();
    if (bound_facade == nullptr || export_operation_ != nullptr || index < 0 ||
        static_cast<std::size_t>(index) >= catalog_.size() ||
        !catalog_[static_cast<std::size_t>(index)]
             .supports_headword_enumeration) {
        result_status_->setText(
            QStringLiteral("Selected dictionary cannot export headwords"));
        if (completion)
            completion(false);
        return;
    }
    goldendict::core::HeadwordExportRequest request;
    request.dictionary_id = catalog_[static_cast<std::size_t>(index)].id;
    request.destination_path = path.toStdString();
    export_operation_ = bound_facade->StartHeadwordExport(std::move(request));
    export_binding_ = std::move(binding);
    export_headwords_->setEnabled(false);
    dictionaries_->setEnabled(false);
    export_progress_ = new QProgressDialog(
        QStringLiteral("Export headwords..."), QStringLiteral("Cancel"), 0,
        static_cast<int>(std::min<std::size_t>(
            catalog_[static_cast<std::size_t>(index)].headword_count,
            static_cast<std::size_t>(std::numeric_limits<int>::max()))),
        this);
    export_progress_->setWindowModality(Qt::WindowModal);
    export_progress_->setMinimumDuration(0);
    connect(export_progress_, &QProgressDialog::canceled, this,
            [this]() { export_operation_->Cancel(); });
    export_poll_timer_ = new QTimer(this);
    connect(
        export_poll_timer_, &QTimer::timeout, this,
        [this, completion = std::move(completion)]() mutable {
            export_progress_->setValue(static_cast<int>(
                std::min<std::size_t>(export_operation_->ExportedHeadwords(),
                                      std::numeric_limits<int>::max())));
            if (!export_operation_->IsFinished())
                return;
            export_poll_timer_->stop();
            const auto result = export_operation_->Await();
            const bool success = static_cast<bool>(result);
            disconnect(export_progress_, &QProgressDialog::canceled, this,
                       nullptr);
            export_progress_->close();
            export_operation_.reset();
            export_binding_ = {};
            export_progress_->deleteLater();
            export_progress_ = nullptr;
            export_poll_timer_->deleteLater();
            export_poll_timer_ = nullptr;
            dictionaries_->setEnabled(true);
            RefreshHeadwords();
            if (success) {
                result_status_->setText(tr("Exported %1 headword(s)")
                                            .arg(static_cast<qulonglong>(
                                                result.exported_headwords)));
            } else if (result.error ==
                       goldendict::core::HeadwordExportErrorCode::kCancelled) {
                result_status_->setText(
                    QStringLiteral("Headword export cancelled"));
            } else {
                result_status_->setText(
                    tr("Could not export headwords: %1")
                        .arg(QString::fromStdString(result.message)));
            }
            if (completion)
                completion(success);
        });
    export_poll_timer_->start(25);
}
