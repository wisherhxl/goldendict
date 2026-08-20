// SPDX-License-Identifier: GPL-3.0-or-later

#include "group_editor.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr qint64 kMaximumIconFileBytes = 48 * 1024;

QPushButton* AddButton(QBoxLayout* layout, const QString& text) {
    auto* button = new QPushButton(text);
    layout->addWidget(button);
    return button;
}

}  // namespace

GroupEditor::GroupEditor(
    std::vector<goldendict::core::DictionaryGroupConfiguration> groups,
    std::vector<goldendict::core::DictionaryIdentity> catalog, QWidget* parent)
    : QDialog(parent),
      groups_(std::move(groups)),
      catalog_(std::move(catalog)) {
    setWindowTitle(QStringLiteral("Dictionary Groups"));
    resize(760, 480);
    auto* outer = new QVBoxLayout(this);
    group_tabs_ = new QTabBar(this);
    group_tabs_->setMovable(false);
    group_tabs_->setUsesScrollButtons(true);
    outer->addWidget(group_tabs_);

    auto* page = new QWidget(this);
    auto* page_layout = new QVBoxLayout(page);
    auto* lists = new QHBoxLayout();
    auto* available_layout = new QVBoxLayout();
    available_layout->addWidget(
        new QLabel(QStringLiteral("Dictionaries available:"), page));
    available_ = new QListWidget(page);
    available_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    available_layout->addWidget(available_);
    lists->addLayout(available_layout, 1);
    auto* transfer = new QVBoxLayout();
    transfer->addStretch();
    add_dictionary_ = AddButton(transfer, QStringLiteral(">"));
    add_dictionary_->setToolTip(QStringLiteral("Add selected dictionaries"));
    remove_dictionary_ = AddButton(transfer, QStringLiteral("<"));
    remove_dictionary_->setToolTip(
        QStringLiteral("Remove selected dictionaries"));
    transfer->addStretch();
    lists->addLayout(transfer);
    auto* member_layout = new QVBoxLayout();
    member_layout->addWidget(
        new QLabel(QStringLiteral("Group dictionaries:"), page));
    members_ = new QTreeWidget(page);
    members_->setHeaderLabels({QStringLiteral("Dictionary"),
                               QStringLiteral("Muted"),
                               QStringLiteral("Popup muted")});
    members_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    members_->setRootIsDecorated(false);
    member_layout->addWidget(members_);
    auto* member_buttons = new QHBoxLayout();
    dictionary_up_ = AddButton(member_buttons, QStringLiteral("Move Up"));
    dictionary_down_ = AddButton(member_buttons, QStringLiteral("Move Down"));
    member_layout->addLayout(member_buttons);
    lists->addLayout(member_layout, 2);
    page_layout->addLayout(lists, 1);

    auto* form = new QFormLayout();
    name_ = new QLineEdit(page);
    icon_ = new QLineEdit(page);
    icon_->setReadOnly(true);
    auto* icon_row = new QHBoxLayout();
    icon_row->addWidget(icon_);
    auto* choose_icon = AddButton(icon_row, QStringLiteral("From File..."));
    auto* clear_icon = AddButton(icon_row, QStringLiteral("Clear"));
    favorites_folder_ = new QLineEdit(page);
    shortcut_ = new QLineEdit(page);
    shortcut_->setPlaceholderText(QStringLiteral("For example Ctrl+1"));
    form->addRow(QStringLiteral("Group name:"), name_);
    form->addRow(QStringLiteral("Group icon:"), icon_row);
    form->addRow(QStringLiteral("Favorites folder:"), favorites_folder_);
    form->addRow(QStringLiteral("Shortcut:"), shortcut_);
    page_layout->addLayout(form);
    outer->addWidget(page, 1);

    auto* group_buttons = new QHBoxLayout();
    auto* add_group = AddButton(group_buttons, QStringLiteral("Add Group"));
    remove_group_ = AddButton(group_buttons, QStringLiteral("Remove Group"));
    group_up_ = AddButton(group_buttons, QStringLiteral("Group Left"));
    group_down_ = AddButton(group_buttons, QStringLiteral("Group Right"));
    group_buttons->addStretch();
    outer->addLayout(group_buttons);
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    outer->addWidget(buttons);

    connect(add_group, &QPushButton::clicked, this, &GroupEditor::AddGroup);
    connect(remove_group_, &QPushButton::clicked, this,
            &GroupEditor::RemoveGroup);
    connect(group_up_, &QPushButton::clicked, this,
            [this]() { MoveGroup(-1); });
    connect(group_down_, &QPushButton::clicked, this,
            [this]() { MoveGroup(1); });
    connect(add_dictionary_, &QPushButton::clicked, this,
            &GroupEditor::AddDictionaries);
    connect(remove_dictionary_, &QPushButton::clicked, this,
            &GroupEditor::RemoveDictionaries);
    connect(dictionary_up_, &QPushButton::clicked, this,
            [this]() { MoveDictionary(-1); });
    connect(dictionary_down_, &QPushButton::clicked, this,
            [this]() { MoveDictionary(1); });
    connect(choose_icon, &QPushButton::clicked, this, &GroupEditor::ChooseIcon);
    connect(clear_icon, &QPushButton::clicked, this, [this]() {
        icon_->clear();
        if (current_group_ >= 0) {
            groups_[static_cast<std::size_t>(current_group_)]
                .encoded_icon_data.clear();
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        StoreCurrentGroup();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(name_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (current_group_ >= 0) {
            group_tabs_->setTabText(
                current_group_,
                text.isEmpty() ? QStringLiteral("Unnamed") : text);
        }
    });
    connect(group_tabs_, &QTabBar::currentChanged, this, [this](int index) {
        if (index == current_group_)
            return;
        StoreCurrentGroup();
        current_group_ = index;
        LoadCurrentGroup();
    });
    if (!groups_.empty())
        current_group_ = 0;
    RefreshGroupTabs();
    LoadCurrentGroup();
}

std::vector<goldendict::core::DictionaryGroupConfiguration>
GroupEditor::Groups() const {
    return groups_;
}

bool GroupEditor::RunSmokeEdits() {
    if (catalog_.empty())
        return false;
    groups_.clear();
    groups_.push_back({7U, "Fixture Group", "", {"unavailable-id"}});
    groups_.push_back({9U, "Delete Me", "", {catalog_.front().id}});
    current_group_ = 0;
    RefreshGroupTabs();
    LoadCurrentGroup();
    name_->setText(QStringLiteral("Renamed Fixture Group"));
    favorites_folder_->setText(QStringLiteral("Fixture/Favorites"));
    shortcut_->setText(QStringLiteral("Ctrl+7"));
    auto& edited = groups_.front();
    edited.icon = "fixture.svg";
    edited.encoded_icon_data =
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+"
        "A8AAQUBAScY42YAAAAASUVORK5CYII=";
    if (available_->count() == 0)
        return false;
    available_->item(0)->setSelected(true);
    AddDictionaries();
    if (members_->topLevelItemCount() != 2)
        return false;
    members_->topLevelItem(1)->setCheckState(1, Qt::Checked);
    members_->topLevelItem(0)->setCheckState(2, Qt::Checked);
    members_->setCurrentItem(members_->topLevelItem(1));
    MoveDictionary(-1);
    MoveGroup(1);
    current_group_ = 0;
    LoadCurrentGroup();
    RemoveGroup();
    StoreCurrentGroup();
    return groups_.size() == 1U && groups_.front().id == 7U &&
           groups_.front().dictionary_ids.front() == catalog_.front().id;
}

void GroupEditor::AddGroup() {
    StoreCurrentGroup();
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("Add dictionary group"),
                              QStringLiteral("Group name:"), QLineEdit::Normal,
                              QString(), &accepted)
            .trimmed();
    if (!accepted)
        return;
    groups_.push_back({NextGroupId(), name.toStdString(), "", {}});
    current_group_ = static_cast<int>(groups_.size() - 1U);
    RefreshGroupTabs();
    LoadCurrentGroup();
}

void GroupEditor::RemoveGroup() {
    if (current_group_ < 0)
        return;
    groups_.erase(groups_.begin() + current_group_);
    current_group_ =
        groups_.empty()
            ? -1
            : std::min(current_group_, static_cast<int>(groups_.size() - 1U));
    RefreshGroupTabs();
    LoadCurrentGroup();
}

void GroupEditor::MoveGroup(int offset) {
    StoreCurrentGroup();
    const int destination = current_group_ + offset;
    if (current_group_ < 0 || destination < 0 ||
        destination >= static_cast<int>(groups_.size()))
        return;
    std::iter_swap(groups_.begin() + current_group_,
                   groups_.begin() + destination);
    current_group_ = destination;
    RefreshGroupTabs();
    LoadCurrentGroup();
}

void GroupEditor::AddDictionaries() {
    if (current_group_ < 0)
        return;
    auto& group = groups_[static_cast<std::size_t>(current_group_)];
    for (auto* item : available_->selectedItems()) {
        const std::string id =
            item->data(Qt::UserRole).toString().toStdString();
        if (std::find(group.dictionary_ids.begin(), group.dictionary_ids.end(),
                      id) == group.dictionary_ids.end())
            group.dictionary_ids.push_back(id);
    }
    LoadCurrentGroup();
}

void GroupEditor::RemoveDictionaries() {
    if (current_group_ < 0)
        return;
    StoreCurrentGroup();
    auto& group = groups_[static_cast<std::size_t>(current_group_)];
    std::unordered_set<std::string> removed;
    for (auto* item : members_->selectedItems())
        removed.insert(item->data(0, Qt::UserRole).toString().toStdString());
    const auto erase_ids = [&removed](auto* values) {
        values->erase(std::remove_if(values->begin(), values->end(),
                                     [&removed](const auto& id) {
                                         return removed.count(id) != 0U;
                                     }),
                      values->end());
    };
    erase_ids(&group.dictionary_ids);
    erase_ids(&group.muted_dictionary_ids);
    erase_ids(&group.popup_muted_dictionary_ids);
    LoadCurrentGroup();
}

void GroupEditor::MoveDictionary(int offset) {
    if (current_group_ < 0 || members_->selectedItems().size() != 1)
        return;
    StoreCurrentGroup();
    auto& ids =
        groups_[static_cast<std::size_t>(current_group_)].dictionary_ids;
    const int index =
        members_->indexOfTopLevelItem(members_->selectedItems().front());
    const int destination = index + offset;
    if (destination < 0 || destination >= static_cast<int>(ids.size()))
        return;
    std::iter_swap(ids.begin() + index, ids.begin() + destination);
    LoadCurrentGroup();
    members_->setCurrentItem(members_->topLevelItem(destination));
}

void GroupEditor::ChooseIcon() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose a group icon"), QString(),
        QStringLiteral(
            "Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg);;All files (*.*)"));
    if (path.isEmpty() || current_group_ < 0)
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) ||
        file.size() > kMaximumIconFileBytes) {
        QMessageBox::warning(
            this, QStringLiteral("Dictionary Groups"),
            QStringLiteral("The icon cannot be read or is too large."));
        return;
    }
    auto& group = groups_[static_cast<std::size_t>(current_group_)];
    group.icon = QFileInfo(path).fileName().toStdString();
    group.encoded_icon_data = file.readAll().toBase64().toStdString();
    icon_->setText(QString::fromStdString(group.icon));
}

void GroupEditor::LoadCurrentGroup() {
    const bool enabled = current_group_ >= 0;
    name_->setEnabled(enabled);
    icon_->setEnabled(enabled);
    favorites_folder_->setEnabled(enabled);
    shortcut_->setEnabled(enabled);
    members_->clear();
    if (!enabled) {
        name_->clear();
        icon_->clear();
        favorites_folder_->clear();
        shortcut_->clear();
        RefreshAvailable();
        UpdateActions();
        return;
    }
    const auto& group = groups_[static_cast<std::size_t>(current_group_)];
    name_->setText(QString::fromStdString(group.name));
    icon_->setText(QString::fromStdString(group.icon));
    favorites_folder_->setText(QString::fromStdString(group.favorites_folder));
    shortcut_->setText(QString::fromStdString(group.shortcut));
    const std::unordered_set<std::string> muted(
        group.muted_dictionary_ids.begin(), group.muted_dictionary_ids.end());
    const std::unordered_set<std::string> popup(
        group.popup_muted_dictionary_ids.begin(),
        group.popup_muted_dictionary_ids.end());
    for (const auto& id : group.dictionary_ids) {
        auto* item = new QTreeWidgetItem(members_);
        item->setText(0, DictionaryName(id));
        item->setData(0, Qt::UserRole, QString::fromStdString(id));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(1, muted.count(id) ? Qt::Checked : Qt::Unchecked);
        item->setCheckState(2, popup.count(id) ? Qt::Checked : Qt::Unchecked);
    }
    group_tabs_->setCurrentIndex(current_group_);
    RefreshAvailable();
    UpdateActions();
}

void GroupEditor::RefreshGroupTabs() {
    const QSignalBlocker blocker(group_tabs_);
    while (group_tabs_->count() > 0)
        group_tabs_->removeTab(0);
    for (const auto& group : groups_)
        group_tabs_->addTab(QString::fromStdString(group.name));
    if (current_group_ >= 0)
        group_tabs_->setCurrentIndex(current_group_);
}

void GroupEditor::StoreCurrentGroup() {
    if (current_group_ < 0)
        return;
    auto& group = groups_[static_cast<std::size_t>(current_group_)];
    group.name = name_->text().trimmed().toStdString();
    group.icon = icon_->text().toStdString();
    group.favorites_folder = favorites_folder_->text().toStdString();
    group.shortcut = shortcut_->text().toStdString();
    const auto retain_orphans = [&group](const auto& values) {
        std::vector<std::string> retained;
        for (const auto& id : values) {
            if (std::find(group.dictionary_ids.begin(),
                          group.dictionary_ids.end(),
                          id) == group.dictionary_ids.end())
                retained.push_back(id);
        }
        return retained;
    };
    group.muted_dictionary_ids = retain_orphans(group.muted_dictionary_ids);
    group.popup_muted_dictionary_ids =
        retain_orphans(group.popup_muted_dictionary_ids);
    for (int index = 0; index < members_->topLevelItemCount(); ++index) {
        const auto* item = members_->topLevelItem(index);
        const std::string id =
            item->data(0, Qt::UserRole).toString().toStdString();
        if (item->checkState(1) == Qt::Checked)
            group.muted_dictionary_ids.push_back(id);
        if (item->checkState(2) == Qt::Checked)
            group.popup_muted_dictionary_ids.push_back(id);
    }
}

void GroupEditor::RefreshAvailable() {
    available_->clear();
    std::unordered_set<std::string> used;
    if (current_group_ >= 0) {
        const auto& ids =
            groups_[static_cast<std::size_t>(current_group_)].dictionary_ids;
        used.insert(ids.begin(), ids.end());
    }
    for (const auto& dictionary : catalog_) {
        if (used.count(dictionary.id) != 0U)
            continue;
        auto* item = new QListWidgetItem(
            QString::fromStdString(dictionary.name), available_);
        item->setData(Qt::UserRole, QString::fromStdString(dictionary.id));
        item->setToolTip(QString::fromStdString(dictionary.id));
    }
}

void GroupEditor::UpdateActions() {
    const bool enabled = current_group_ >= 0;
    remove_group_->setEnabled(enabled);
    group_up_->setEnabled(enabled && current_group_ > 0);
    group_down_->setEnabled(enabled && current_group_ + 1 <
                                           static_cast<int>(groups_.size()));
    add_dictionary_->setEnabled(enabled);
    remove_dictionary_->setEnabled(enabled);
    dictionary_up_->setEnabled(enabled);
    dictionary_down_->setEnabled(enabled);
}

std::uint32_t GroupEditor::NextGroupId() const {
    std::unordered_set<std::uint32_t> ids;
    for (const auto& group : groups_)
        ids.insert(group.id);
    for (std::uint32_t id = 1U; id != std::numeric_limits<std::uint32_t>::max();
         ++id)
        if (ids.count(id) == 0U)
            return id;
    return std::numeric_limits<std::uint32_t>::max();
}

QString GroupEditor::DictionaryName(const std::string& id) const {
    const auto found = std::find_if(
        catalog_.begin(), catalog_.end(),
        [&id](const auto& dictionary) { return dictionary.id == id; });
    return found == catalog_.end() ? QStringLiteral("%1 (unavailable)")
                                         .arg(QString::fromStdString(id))
                                   : QString::fromStdString(found->name);
}
