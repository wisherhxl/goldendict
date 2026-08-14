// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_FAVORITES_TREE_WIDGET_H_
#define GOLDENDICT_APPS_GOLDENDICT_FAVORITES_TREE_WIDGET_H_

#include <functional>

#include <QList>
#include <QTreeWidget>

class QDropEvent;

class FavoritesTreeWidget final : public QTreeWidget {
   public:
    using MoveRequest =
        std::function<void(const QList<int>&, const QList<int>&, int)>;

    explicit FavoritesTreeWidget(QWidget* parent = nullptr);
    void SetMoveRequest(MoveRequest request);

   protected:
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supported_actions) override;

   private:
    MoveRequest move_request_;
    QList<int> dragged_path_;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_FAVORITES_TREE_WIDGET_H_
