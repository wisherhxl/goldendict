// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_browser.h"

#include <utility>

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSaveFile>
#include <QTimer>
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
    description_ = new QLabel(this);
    description_->setObjectName(QStringLiteral("dictionaryDescription"));
    description_->setTextFormat(Qt::PlainText);
    description_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    description_->setWordWrap(true);
    details->addRow(QStringLiteral("Identifier:"), identifier_);
    details->addRow(QStringLiteral("Edition:"), edition_);
    details->addRow(QStringLiteral("Source:"), source_);
    details->addRow(QStringLiteral("Description:"), description_);
    layout->addLayout(details);

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
        new QPushButton(QStringLiteral("Export Displayed Headwords..."), this);
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
    connect(headwords_, &QListWidget::itemActivated, this,
            [this](const QListWidgetItem* item) {
                if (item != nullptr) {
                    emit HeadwordSelected(item->text());
                }
            });
    connect(export_headwords_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export displayed headwords"), QString(),
            QStringLiteral("Text files (*.txt);;All files (*.*)"));
        if (path.isEmpty()) {
            return;
        }
        result_status_->setText(
            ExportHeadwordsToFile(path)
                ? tr("Exported %1 headword(s)").arg(headwords_->count())
                : QStringLiteral("Could not export headwords"));
    });
}

void DictionaryBrowser::RunExportSmokeCheck(
    const QString& path, const QString& prefix, const QByteArray& expected,
    std::function<void(bool)> completion) {
    prefix_->setText(prefix);
    QTimer::singleShot(
        0, this,
        [this, path, expected, completion = std::move(completion)]() mutable {
            const bool exported = ExportHeadwordsToFile(path);
            QFile file(path);
            const bool opened = file.open(QIODevice::ReadOnly);
            completion(exported && opened && file.readAll() == expected);
        });
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

void DictionaryBrowser::RunSmokeCheck(const QString& expected_dictionary,
                                      const QString& prefix,
                                      const QString& expected_headword,
                                      std::function<void(bool)> completion) {
    prefix_->setText(prefix);
    QTimer::singleShot(0, this,
                       [this, expected_dictionary, expected_headword,
                        completion = std::move(completion)]() mutable {
                           const auto matches = headwords_->findItems(
                               expected_headword, Qt::MatchFixedString);
                           const bool passed = dictionaries_->currentText() ==
                                                   expected_dictionary &&
                                               !identifier_->text().isEmpty() &&
                                               !source_->text().isEmpty() &&
                                               matches.size() == 1;
                           if (!passed) {
                               completion(false);
                               return;
                           }
                           emit HeadwordSelected(matches.front()->text());
                           completion(true);
                       });
}

void DictionaryBrowser::RefreshDictionaryInfo() {
    const int index = dictionaries_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= catalog_.size()) {
        identifier_->clear();
        edition_->clear();
        source_->clear();
        description_->clear();
        return;
    }
    const auto& dictionary = catalog_[static_cast<std::size_t>(index)];
    identifier_->setText(QString::fromStdString(dictionary.id));
    edition_->setText(dictionary.edition.empty()
                          ? QStringLiteral("Not specified")
                          : QString::fromStdString(dictionary.edition));
    source_->setText(QString::fromStdString(dictionary.source));
    description_->setText(dictionary.description.empty()
                              ? QStringLiteral("Not specified")
                              : QString::fromStdString(dictionary.description));
}

void DictionaryBrowser::RefreshHeadwords() {
    headwords_->clear();
    export_headwords_->setEnabled(false);
    const int index = dictionaries_->currentIndex();
    const QString prefix = prefix_->text().trimmed();
    if (facade_ == nullptr || index < 0 ||
        static_cast<std::size_t>(index) >= catalog_.size()) {
        result_status_->setText(QStringLiteral("No dictionary selected"));
        return;
    }
    if (prefix.isEmpty()) {
        result_status_->setText(QStringLiteral("Enter a prefix to browse"));
        return;
    }

    goldendict::core::SuggestionQuery query;
    query.text = prefix.toStdString();
    query.dictionary_ids = {catalog_[static_cast<std::size_t>(index)].id};
    query.result_limit = 100U;
    const auto response = facade_->GetDictionaryService().Suggest(query);
    for (const auto& suggestion : response.suggestions) {
        headwords_->addItem(QString::fromStdString(suggestion.headword));
    }
    if (!response.errors.empty() && response.suggestions.empty()) {
        result_status_->setText(
            QString::fromStdString(response.errors.front().message));
        return;
    }
    export_headwords_->setEnabled(headwords_->count() > 0);
    result_status_->setText(
        tr("%1 headword(s); at most 100 shown")
            .arg(static_cast<qulonglong>(response.suggestions.size())));
}

bool DictionaryBrowser::ExportHeadwordsToFile(const QString& path) {
    if (headwords_->count() == 0) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QByteArray::fromHex("efbbbf")) != 3) {
        return false;
    }
    for (int index = 0; index < headwords_->count(); ++index) {
        QByteArray line = headwords_->item(index)->text().toUtf8();
        line.replace('\n', ' ');
        line.replace('\r', ' ');
        line.push_back('\n');
        if (file.write(line) != line.size()) {
            return false;
        }
    }
    return file.commit();
}
