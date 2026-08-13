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
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Dictionary Sources"));
    resize(700, 430);
    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->addTab(CreateDictionaryTab(), QStringLiteral("Dictionary Paths"));
    tabs->addTab(CreateSoundTab(), QStringLiteral("Sound Dirs"));
    layout->addWidget(tabs);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("Apply"));
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    for (const auto& path : dictionary_paths)
        AddDictionaryPath(QString::fromStdString(path));
    for (const auto& directory : sound_directories) {
        AddSoundDirectory(QString::fromStdString(directory.path),
                          QString::fromStdString(directory.name));
    }
    UpdateDictionaryButtons();
    UpdateSoundButtons();
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
