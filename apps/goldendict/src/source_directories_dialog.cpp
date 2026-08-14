// SPDX-License-Identifier: GPL-3.0-or-later

#include "source_directories_dialog.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>

namespace {

QPushButton* AddButton(QVBoxLayout* layout, const QString& text,
                       const QString& name) {
    auto* button = new QPushButton(text);
    button->setObjectName(name);
    layout->addWidget(button);
    return button;
}

}  // namespace

SourceDirectoriesDialog::SourceDirectoriesDialog(
    const std::vector<std::string>& dictionary_paths,
    const std::vector<goldendict::core::SoundDirectoryConfiguration>&
        sound_directories,
    QWidget* parent)
    : SourceDirectoriesDialog(dictionary_paths, sound_directories, {}, {}, {},
                              {}, {}, parent) {}

SourceDirectoriesDialog::SourceDirectoriesDialog(
    const std::vector<std::string>& dictionary_paths,
    const std::vector<goldendict::core::SoundDirectoryConfiguration>&
        sound_directories,
    const std::vector<goldendict::core::MediaWikiSourceConfiguration>&
        mediawiki_sources,
    const std::vector<goldendict::core::WebsiteSourceConfiguration>&
        website_sources,
    const std::vector<goldendict::core::ForvoSourceConfiguration>&
        forvo_sources,
    const std::vector<goldendict::core::DictServerSourceConfiguration>&
        dict_server_sources,
    ApplyCallback apply_callback, QWidget* parent)
    : QDialog(parent), apply_callback_(std::move(apply_callback)) {
    setWindowTitle(QStringLiteral("Dictionary Sources"));
    resize(700, 430);
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->addTab(CreateDictionaryTab(), QStringLiteral("Dictionary Paths"));
    tabs->addTab(CreateSoundTab(), QStringLiteral("Sound Dirs"));
    tabs->addTab(
        CreateOnlineTab(QStringLiteral("MediaWiki API base addresses:"),
                        {QStringLiteral("Enabled"), QStringLiteral("Name"),
                         QStringLiteral("Base URL")},
                        &mediawiki_sources_, QStringLiteral("mediaWiki")),
        QStringLiteral("MediaWiki"));
    tabs->addTab(CreateOnlineTab(
                     QStringLiteral("%GDWORD% is replaced by the query word:"),
                     {QStringLiteral("Enabled"), QStringLiteral("Name"),
                      QStringLiteral("URL Template")},
                     &website_sources_, QStringLiteral("website")),
                 QStringLiteral("Websites"));
    tabs->addTab(
        CreateOnlineTab(
            QStringLiteral("Language codes are comma-separated and ordered. "
                           "Credentials are not stored here."),
            {QStringLiteral("Enabled"), QStringLiteral("Name"),
             QStringLiteral("API Base URL"), QStringLiteral("Languages")},
            &forvo_sources_, QStringLiteral("forvo")),
        QStringLiteral("Forvo"));
    tabs->addTab(CreateOnlineTab(
                     QStringLiteral("Credential-free DICT server settings:"),
                     {QStringLiteral("Enabled"), QStringLiteral("Name"),
                      QStringLiteral("Host"), QStringLiteral("Port"),
                      QStringLiteral("Database"), QStringLiteral("Strategy")},
                     &dict_server_sources_, QStringLiteral("dictServer")),
                 QStringLiteral("DICT Servers"));
    layout->addWidget(tabs);
    validation_error_ = new QLabel(this);
    validation_error_->setObjectName(QStringLiteral("sourceValidationError"));
    validation_error_->setStyleSheet(QStringLiteral("color: #b00020"));
    validation_error_->setWordWrap(true);
    validation_error_->hide();
    layout->addWidget(validation_error_);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("Apply"));
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SourceDirectoriesDialog::Apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    for (const auto& path : dictionary_paths)
        AddDictionaryPath(QString::fromStdString(path));
    for (const auto& directory : sound_directories) {
        AddSoundDirectory(QString::fromStdString(directory.path),
                          QString::fromStdString(directory.name));
    }
    const auto add_online_item = [](QTreeWidget* tree, const std::string& id,
                                    bool enabled, const QStringList& fields) {
        auto* item = new QTreeWidgetItem(tree);
        item->setData(0, Qt::UserRole, QString::fromStdString(id));
        item->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
        for (int column = 1; column < fields.size() + 1; ++column)
            item->setText(column, fields[column - 1]);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    };
    for (const auto& source : mediawiki_sources)
        add_online_item(mediawiki_sources_, source.id, source.enabled,
                        {QString::fromStdString(source.name),
                         QString::fromStdString(source.base_url)});
    for (const auto& source : website_sources)
        add_online_item(website_sources_, source.id, source.enabled,
                        {QString::fromStdString(source.name),
                         QString::fromStdString(source.url_template)});
    for (const auto& source : forvo_sources) {
        QStringList languages;
        for (const auto& language : source.language_codes)
            languages.push_back(QString::fromStdString(language));
        add_online_item(forvo_sources_, source.id, source.enabled,
                        {QString::fromStdString(source.name),
                         QString::fromStdString(source.api_base_url),
                         languages.join(QStringLiteral(","))});
    }
    for (const auto& source : dict_server_sources)
        add_online_item(
            dict_server_sources_, source.id, source.enabled,
            {QString::fromStdString(source.name),
             QString::fromStdString(source.host),
             QString::number(static_cast<unsigned int>(source.port)),
             QString::fromStdString(source.database),
             QString::fromStdString(source.strategy)});
    UpdateDictionaryButtons();
    UpdateSoundButtons();
    UpdateOnlineButtons(mediawiki_sources_);
    UpdateOnlineButtons(website_sources_);
    UpdateOnlineButtons(forvo_sources_);
    UpdateOnlineButtons(dict_server_sources_);
}

QWidget* SourceDirectoriesDialog::CreateDictionaryTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->addWidget(new QLabel(
        QStringLiteral("Paths to search for dictionary files:"), tab));
    auto* row = new QHBoxLayout();
    dictionary_paths_ = new QListWidget(tab);
    dictionary_paths_->setObjectName(QStringLiteral("dictionaryPathList"));
    row->addWidget(dictionary_paths_, 1);
    auto* controls = new QVBoxLayout();
    add_dictionary_ = AddButton(controls, QStringLiteral("&Add..."),
                                QStringLiteral("addDictionaryPath"));
    remove_dictionary_ = AddButton(controls, QStringLiteral("&Remove"),
                                   QStringLiteral("removeDictionaryPath"));
    dictionary_up_ = AddButton(controls, QStringLiteral("Move &Up"),
                               QStringLiteral("moveDictionaryPathUp"));
    dictionary_down_ = AddButton(controls, QStringLiteral("Move &Down"),
                                 QStringLiteral("moveDictionaryPathDown"));
    controls->addStretch();
    row->addLayout(controls);
    layout->addLayout(row, 1);
    connect(add_dictionary_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Choose a directory"));
        if (!path.isEmpty())
            AddDictionaryPath(path);
    });
    connect(remove_dictionary_, &QPushButton::clicked, this, [this]() {
        delete dictionary_paths_->takeItem(dictionary_paths_->currentRow());
        UpdateDictionaryButtons();
    });
    connect(dictionary_up_, &QPushButton::clicked, this,
            [this]() { MoveDictionaryPath(-1); });
    connect(dictionary_down_, &QPushButton::clicked, this,
            [this]() { MoveDictionaryPath(1); });
    connect(dictionary_paths_, &QListWidget::currentRowChanged, this,
            [this]() { UpdateDictionaryButtons(); });
    return tab;
}

QWidget* SourceDirectoriesDialog::CreateSoundTab() {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->addWidget(
        new QLabel(QStringLiteral("Make dictionaries from groups of audio "
                                  "files by adding paths here:"),
                   tab));
    auto* row = new QHBoxLayout();
    sound_directories_ = new QTreeWidget(tab);
    sound_directories_->setObjectName(QStringLiteral("soundDirectoryList"));
    sound_directories_->setHeaderLabels(
        {QStringLiteral("Path"), QStringLiteral("Name")});
    sound_directories_->header()->setStretchLastSection(true);
    sound_directories_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    row->addWidget(sound_directories_, 1);
    auto* controls = new QVBoxLayout();
    add_sound_ = AddButton(controls, QStringLiteral("&Add..."),
                           QStringLiteral("addSoundDirectory"));
    remove_sound_ = AddButton(controls, QStringLiteral("&Remove"),
                              QStringLiteral("removeSoundDirectory"));
    sound_up_ = AddButton(controls, QStringLiteral("Move &Up"),
                          QStringLiteral("moveSoundDirectoryUp"));
    sound_down_ = AddButton(controls, QStringLiteral("Move &Down"),
                            QStringLiteral("moveSoundDirectoryDown"));
    controls->addStretch();
    row->addLayout(controls);
    layout->addLayout(row, 1);
    connect(add_sound_, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Choose a directory"));
        if (!path.isEmpty())
            AddSoundDirectory(path, QDir(path).dirName());
    });
    connect(remove_sound_, &QPushButton::clicked, this, [this]() {
        delete sound_directories_->takeTopLevelItem(
            sound_directories_->indexOfTopLevelItem(
                sound_directories_->currentItem()));
        UpdateSoundButtons();
    });
    connect(sound_up_, &QPushButton::clicked, this,
            [this]() { MoveSoundDirectory(-1); });
    connect(sound_down_, &QPushButton::clicked, this,
            [this]() { MoveSoundDirectory(1); });
    connect(sound_directories_, &QTreeWidget::currentItemChanged, this,
            [this]() { UpdateSoundButtons(); });
    return tab;
}

QWidget* SourceDirectoriesDialog::CreateOnlineTab(
    const QString& description, const QStringList& headers, QTreeWidget** tree,
    const QString& object_prefix) {
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    auto* label = new QLabel(description, tab);
    label->setWordWrap(true);
    layout->addWidget(label);
    auto* row = new QHBoxLayout();
    *tree = new QTreeWidget(tab);
    (*tree)->setObjectName(object_prefix + QStringLiteral("List"));
    (*tree)->setHeaderLabels(headers);
    (*tree)->setSelectionMode(QAbstractItemView::SingleSelection);
    (*tree)->setRootIsDecorated(false);
    (*tree)->header()->setStretchLastSection(true);
    for (int column = 0; column + 1 < headers.size(); ++column)
        (*tree)->header()->setSectionResizeMode(column,
                                                QHeaderView::ResizeToContents);
    row->addWidget(*tree, 1);
    auto* controls = new QVBoxLayout();
    auto* add = AddButton(controls, QStringLiteral("&Add"),
                          object_prefix + QStringLiteral("Add"));
    auto* remove = AddButton(controls, QStringLiteral("&Remove"),
                             object_prefix + QStringLiteral("Remove"));
    auto* up = AddButton(controls, QStringLiteral("Move &Up"),
                         object_prefix + QStringLiteral("Up"));
    auto* down = AddButton(controls, QStringLiteral("Move &Down"),
                           object_prefix + QStringLiteral("Down"));
    controls->addStretch();
    row->addLayout(controls);
    layout->addLayout(row, 1);
    QTreeWidget* const source_tree = *tree;
    connect(add, &QPushButton::clicked, this, [this, source_tree]() {
        if (source_tree == mediawiki_sources_)
            AddMediaWiki();
        else if (source_tree == website_sources_)
            AddWebsite();
        else if (source_tree == forvo_sources_)
            AddForvo();
        else
            AddDictServer();
    });
    connect(remove, &QPushButton::clicked, this, [this, source_tree]() {
        const int selected_row =
            source_tree->indexOfTopLevelItem(source_tree->currentItem());
        if (selected_row >= 0)
            delete source_tree->takeTopLevelItem(selected_row);
        UpdateOnlineButtons(source_tree);
    });
    connect(up, &QPushButton::clicked, this,
            [this, source_tree]() { MoveOnline(source_tree, -1); });
    connect(down, &QPushButton::clicked, this,
            [this, source_tree]() { MoveOnline(source_tree, 1); });
    connect(source_tree, &QTreeWidget::currentItemChanged, this,
            [this, source_tree]() { UpdateOnlineButtons(source_tree); });
    return tab;
}

namespace {

QString NewOnlineId() {
    return QStringLiteral("online.") +
           QUuid::createUuid().toString(QUuid::Id128);
}

QTreeWidgetItem* AddOnlineItem(QTreeWidget* tree, const QStringList& fields) {
    auto* item = new QTreeWidgetItem(tree);
    item->setData(0, Qt::UserRole, NewOnlineId());
    item->setCheckState(0, Qt::Unchecked);
    for (int column = 1; column < fields.size() + 1; ++column)
        item->setText(column, fields[column - 1]);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    tree->setCurrentItem(item, 1);
    tree->editItem(item, 1);
    return item;
}

std::string ItemId(const QTreeWidgetItem* item) {
    return item->data(0, Qt::UserRole).toString().toStdString();
}

bool ItemEnabled(const QTreeWidgetItem* item) {
    return item->checkState(0) == Qt::Checked;
}

}  // namespace

void SourceDirectoriesDialog::AddMediaWiki() {
    AddOnlineItem(mediawiki_sources_,
                  {QStringLiteral("New MediaWiki"),
                   QStringLiteral("https://example.com/w")});
    UpdateOnlineButtons(mediawiki_sources_);
}

void SourceDirectoriesDialog::AddWebsite() {
    AddOnlineItem(website_sources_,
                  {QStringLiteral("New Website"),
                   QStringLiteral("https://example.com/?q=%GDWORD%")});
    UpdateOnlineButtons(website_sources_);
}

void SourceDirectoriesDialog::AddForvo() {
    AddOnlineItem(forvo_sources_, {QStringLiteral("New Forvo"),
                                   QStringLiteral("https://apifree.forvo.com"),
                                   QStringLiteral("en")});
    UpdateOnlineButtons(forvo_sources_);
}

void SourceDirectoriesDialog::AddDictServer() {
    AddOnlineItem(dict_server_sources_,
                  {QStringLiteral("New DICT Server"),
                   QStringLiteral("dict.org"), QStringLiteral("2628"),
                   QStringLiteral("*"), QStringLiteral("prefix")});
    UpdateOnlineButtons(dict_server_sources_);
}

void SourceDirectoriesDialog::MoveOnline(QTreeWidget* tree, int offset) {
    const int row = tree->indexOfTopLevelItem(tree->currentItem());
    if (row < 0 || row + offset < 0 ||
        row + offset >= tree->topLevelItemCount())
        return;
    auto* item = tree->takeTopLevelItem(row);
    tree->insertTopLevelItem(row + offset, item);
    tree->setCurrentItem(item);
    UpdateOnlineButtons(tree);
}

void SourceDirectoriesDialog::UpdateOnlineButtons(QTreeWidget* tree) {
    const QString prefix = tree->objectName().chopped(4);
    const int row = tree->indexOfTopLevelItem(tree->currentItem());
    findChild<QPushButton*>(prefix + QStringLiteral("Add"))
        ->setEnabled(static_cast<std::size_t>(tree->topLevelItemCount()) <
                     goldendict::core::kMaximumOnlineSources);
    findChild<QPushButton*>(prefix + QStringLiteral("Remove"))
        ->setEnabled(row >= 0);
    findChild<QPushButton*>(prefix + QStringLiteral("Up"))->setEnabled(row > 0);
    findChild<QPushButton*>(prefix + QStringLiteral("Down"))
        ->setEnabled(row >= 0 && row + 1 < tree->topLevelItemCount());
}

void SourceDirectoriesDialog::AddDictionaryPath(const QString& path) {
    dictionary_paths_->addItem(path);
    dictionary_paths_->setCurrentRow(dictionary_paths_->count() - 1);
    UpdateDictionaryButtons();
}

void SourceDirectoriesDialog::AddSoundDirectory(const QString& path,
                                                const QString& name) {
    auto* item = new QTreeWidgetItem({path, name});
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    sound_directories_->addTopLevelItem(item);
    sound_directories_->setCurrentItem(item);
    UpdateSoundButtons();
}

void SourceDirectoriesDialog::MoveDictionaryPath(int offset) {
    const int row = dictionary_paths_->currentRow();
    if (row < 0 || row + offset < 0 ||
        row + offset >= dictionary_paths_->count())
        return;
    auto* item = dictionary_paths_->takeItem(row);
    dictionary_paths_->insertItem(row + offset, item);
    dictionary_paths_->setCurrentRow(row + offset);
}

void SourceDirectoriesDialog::MoveSoundDirectory(int offset) {
    const int row = sound_directories_->indexOfTopLevelItem(
        sound_directories_->currentItem());
    if (row < 0 || row + offset < 0 ||
        row + offset >= sound_directories_->topLevelItemCount())
        return;
    auto* item = sound_directories_->takeTopLevelItem(row);
    sound_directories_->insertTopLevelItem(row + offset, item);
    sound_directories_->setCurrentItem(item);
}

void SourceDirectoriesDialog::UpdateDictionaryButtons() {
    const int row = dictionary_paths_->currentRow();
    add_dictionary_->setEnabled(
        static_cast<std::size_t>(dictionary_paths_->count()) <
        goldendict::core::kMaximumDictionaryPaths);
    remove_dictionary_->setEnabled(row >= 0);
    dictionary_up_->setEnabled(row > 0);
    dictionary_down_->setEnabled(row >= 0 &&
                                 row + 1 < dictionary_paths_->count());
}

void SourceDirectoriesDialog::UpdateSoundButtons() {
    const int row = sound_directories_->indexOfTopLevelItem(
        sound_directories_->currentItem());
    add_sound_->setEnabled(
        static_cast<std::size_t>(sound_directories_->topLevelItemCount()) <
        goldendict::core::kMaximumSoundDirectories);
    remove_sound_->setEnabled(row >= 0);
    sound_up_->setEnabled(row > 0);
    sound_down_->setEnabled(row >= 0 &&
                            row + 1 < sound_directories_->topLevelItemCount());
}

std::vector<std::string> SourceDirectoriesDialog::DictionaryPaths() const {
    std::vector<std::string> paths;
    paths.reserve(static_cast<std::size_t>(dictionary_paths_->count()));
    for (int row = 0; row < dictionary_paths_->count(); ++row)
        paths.push_back(dictionary_paths_->item(row)->text().toStdString());
    return paths;
}

std::vector<goldendict::core::SoundDirectoryConfiguration>
SourceDirectoriesDialog::SoundDirectories() const {
    std::vector<goldendict::core::SoundDirectoryConfiguration> directories;
    directories.reserve(
        static_cast<std::size_t>(sound_directories_->topLevelItemCount()));
    for (int row = 0; row < sound_directories_->topLevelItemCount(); ++row) {
        const auto* item = sound_directories_->topLevelItem(row);
        directories.push_back(
            {item->text(0).toStdString(), item->text(1).toStdString()});
    }
    return directories;
}

std::vector<goldendict::core::MediaWikiSourceConfiguration>
SourceDirectoriesDialog::MediaWikiSources() const {
    std::vector<goldendict::core::MediaWikiSourceConfiguration> sources;
    for (int row = 0; row < mediawiki_sources_->topLevelItemCount(); ++row) {
        const auto* item = mediawiki_sources_->topLevelItem(row);
        sources.push_back({ItemId(item), item->text(1).toStdString(),
                           ItemEnabled(item), item->text(2).toStdString()});
    }
    return sources;
}

std::vector<goldendict::core::WebsiteSourceConfiguration>
SourceDirectoriesDialog::WebsiteSources() const {
    std::vector<goldendict::core::WebsiteSourceConfiguration> sources;
    for (int row = 0; row < website_sources_->topLevelItemCount(); ++row) {
        const auto* item = website_sources_->topLevelItem(row);
        sources.push_back({ItemId(item), item->text(1).toStdString(),
                           ItemEnabled(item), item->text(2).toStdString()});
    }
    return sources;
}

std::vector<goldendict::core::ForvoSourceConfiguration>
SourceDirectoriesDialog::ForvoSources() const {
    std::vector<goldendict::core::ForvoSourceConfiguration> sources;
    for (int row = 0; row < forvo_sources_->topLevelItemCount(); ++row) {
        const auto* item = forvo_sources_->topLevelItem(row);
        std::vector<std::string> languages;
        for (const auto& language :
             item->text(3).split(',', Qt::KeepEmptyParts))
            languages.push_back(language.trimmed().toStdString());
        sources.push_back({ItemId(item), item->text(1).toStdString(),
                           ItemEnabled(item), item->text(2).toStdString(),
                           std::move(languages)});
    }
    return sources;
}

std::vector<goldendict::core::DictServerSourceConfiguration>
SourceDirectoriesDialog::DictServerSources() const {
    std::vector<goldendict::core::DictServerSourceConfiguration> sources;
    for (int row = 0; row < dict_server_sources_->topLevelItemCount(); ++row) {
        const auto* item = dict_server_sources_->topLevelItem(row);
        bool valid_port = false;
        const unsigned int port = item->text(3).toUInt(&valid_port);
        sources.push_back(
            {ItemId(item), item->text(1).toStdString(), ItemEnabled(item),
             item->text(2).toStdString(),
             static_cast<std::uint16_t>(valid_port && port <= 65535U ? port
                                                                     : 0U),
             item->text(4).toStdString(), item->text(5).toStdString()});
    }
    return sources;
}

void SourceDirectoriesDialog::Apply() {
    if (QWidget* focus = focusWidget())
        focus->clearFocus();
    try {
        goldendict::core::CoreConfiguration candidate;
        candidate.dictionary_paths = DictionaryPaths();
        candidate.sound_directories = SoundDirectories();
        candidate.mediawiki_sources = MediaWikiSources();
        candidate.website_sources = WebsiteSources();
        candidate.forvo_sources = ForvoSources();
        candidate.dict_server_sources = DictServerSources();
        goldendict::core::ValidateConfiguration(candidate);
        if (apply_callback_) {
            const QString error = apply_callback_(
                candidate.dictionary_paths, candidate.sound_directories,
                candidate.mediawiki_sources, candidate.website_sources,
                candidate.forvo_sources, candidate.dict_server_sources);
            if (!error.isEmpty()) {
                validation_error_->setText(error);
                validation_error_->show();
                return;
            }
        }
        accept();
    } catch (const std::exception& error) {
        validation_error_->setText(QString::fromLocal8Bit(error.what()));
        validation_error_->show();
    }
}
