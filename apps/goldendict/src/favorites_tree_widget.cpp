// SPDX-License-Identifier: GPL-3.0-or-later

#include "favorites_tree_widget.h"

#include <utility>

#include <QAbstractItemView>
#include <QDropEvent>

FavoritesTreeWidget::FavoritesTreeWidget(QWidget* parent)
    : QTreeWidget(parent) {
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
}

void FavoritesTreeWidget::SetMoveRequest(MoveRequest request) {
    move_request_ = std::move(request);
}

void FavoritesTreeWidget::startDrag(Qt::DropActions supported_actions) {
    const auto* item = currentItem();
    dragged_path_ = item == nullptr
                        ? QList<int>{}
                        : item->data(0, Qt::UserRole + 1).value<QList<int>>();
    QTreeWidget::startDrag(supported_actions);
}

void FavoritesTreeWidget::dropEvent(QDropEvent* event) {
    if (event->source() != this || event->dropAction() != Qt::MoveAction ||
        dragged_path_.empty() || !move_request_) {
        event->ignore();
        return;
    }

    QTreeWidgetItem* target = itemAt(event->position().toPoint());
    QList<int> destination_path;
    int destination_index = 0;
    if (target != nullptr && dropIndicatorPosition() == OnItem) {
        if (!target->data(0, Qt::UserRole).toBool()) {
            event->ignore();
            return;
        }
        destination_path =
            target->data(0, Qt::UserRole + 1).value<QList<int>>();
    } else if (target != nullptr) {
        QTreeWidgetItem* parent = target->parent();
        destination_path =
            parent == nullptr
                ? QList<int>{}
                : parent->data(0, Qt::UserRole + 1).value<QList<int>>();
        destination_index = parent == nullptr ? indexOfTopLevelItem(target)
                                              : parent->indexOfChild(target);
        if (dropIndicatorPosition() == BelowItem) {
            ++destination_index;
        }
    }

    event->ignore();
    move_request_(dragged_path_, destination_path, destination_index);
    dragged_path_.clear();
}
