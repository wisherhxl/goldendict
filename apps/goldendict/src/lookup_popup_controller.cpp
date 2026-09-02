// SPDX-License-Identifier: GPL-3.0-or-later

#include "lookup_popup_controller.h"

#include <algorithm>
#include <utility>

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>
#include <QWidget>

namespace goldendict::app {
namespace {

constexpr int kMaximumPopupRows = 17;

}  // namespace

LookupPopupController::LookupPopupController(
    QWidget* window, QLineEdit* query, ActivationCallback activation)
    : QObject(window),
      window_(window),
      query_(query),
      popup_(new QListWidget(window)),
      activation_(std::move(activation)) {
    popup_->setObjectName(QStringLiteral("translateBoxPopup"));
#if defined(Q_OS_WIN)
    popup_->setWindowFlags(Qt::ToolTip);
#else
    popup_->setAutoFillBackground(true);
#endif
    popup_->setFocusPolicy(Qt::NoFocus);
    popup_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    popup_->setSelectionMode(QAbstractItemView::SingleSelection);
    popup_->hide();

    query_->installEventFilter(this);
    window_->installEventFilter(this);
    connect(query_, &QLineEdit::textChanged, this, [this]() {
        if (enabled_ && query_->hasFocus())
            popup_enabled_ = true;
        UpdateVisibility();
    });
    connect(popup_, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                if (item == nullptr)
                    return;
                popup_->setCurrentItem(item);
                AcceptCurrentItem(QApplication::keyboardModifiers());
            });
    connect(popup_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) {
                if (item == nullptr)
                    return;
                popup_->setCurrentItem(item);
                AcceptCurrentItem(QApplication::keyboardModifiers());
            });
}

void LookupPopupController::SetEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_)
        HidePopup();
}

void LookupPopupController::SetItems(const QStringList& items) {
    popup_->clear();
    popup_->addItems(items);
    for (int row = 0; row < popup_->count(); ++row)
        popup_->item(row)->setToolTip(popup_->item(row)->text());
    if (popup_->count() > 0)
        popup_->scrollToTop();
    UpdateVisibility();
}

void LookupPopupController::TogglePopup() {
    if (!enabled_)
        return;
    popup_enabled_ = !popup_enabled_;
    UpdateVisibility();
}

void LookupPopupController::HidePopup() {
    popup_enabled_ = false;
    popup_->hide();
}

QListWidget* LookupPopupController::popup() const {
    return popup_;
}

bool LookupPopupController::eventFilter(QObject* watched, QEvent* event) {
#if defined(Q_OS_WIN)
    const bool popup_keeps_window_active = popup_->isActiveWindow();
#else
    const bool popup_keeps_window_active = false;
#endif
    if (watched == window_ &&
        (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
         (event->type() == QEvent::WindowDeactivate &&
          !popup_keeps_window_active))) {
        HidePopup();
        return false;
    }
    if (watched != query_ || !enabled_)
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::ShortcutOverride) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier &&
            popup_->isVisible()) {
            event->accept();
            return true;
        }
    }
    if (event->type() == QEvent::FocusOut) {
#if defined(Q_OS_WIN)
        const auto* focus = static_cast<QFocusEvent*>(event);
        if (focus->reason() == Qt::ActiveWindowFocusReason &&
            popup_->isActiveWindow()) {
            return false;
        }
#endif
        HidePopup();
        return false;
    }
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);

    const auto* key = static_cast<QKeyEvent*>(event);
    const bool forward_tab_traversal =
        key->key() == Qt::Key_Tab &&
        key->modifiers() == Qt::ControlModifier;
    const bool reverse_tab_traversal =
        (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) &&
        key->modifiers() ==
            (Qt::ControlModifier | Qt::ShiftModifier);
    if (forward_tab_traversal || reverse_tab_traversal)
        return QObject::eventFilter(watched, event);
    if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
        popup_->isVisible() && popup_->currentItem() != nullptr) {
        AcceptCurrentItem(key->modifiers());
        return true;
    }
    if (key->key() == Qt::Key_Escape) {
        HidePopup();
        return true;
    }

    switch (key->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            if (!ShowPopupIfHidden())
                QApplication::sendEvent(popup_, event);
            return true;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            if (!ShowPopupIfHidden()) {
                const int navigation_key = key->key() == Qt::Key_Tab
                                               ? Qt::Key_Down
                                               : Qt::Key_Up;
                QKeyEvent navigation(QEvent::KeyPress, navigation_key,
                                     Qt::NoModifier);
                QApplication::sendEvent(popup_, &navigation);
            }
            return true;
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

void LookupPopupController::AcceptCurrentItem(
    Qt::KeyboardModifiers modifiers) {
    const auto* item = popup_->currentItem();
    if (item == nullptr || !popup_->isVisible())
        return;
    const QString text = item->text();
    HidePopup();
    if (activation_)
        activation_(text, modifiers);
}

bool LookupPopupController::ShowPopupIfHidden() {
    if (popup_->isVisible())
        return false;
    popup_enabled_ = true;
    UpdateVisibility();
    return true;
}

void LookupPopupController::UpdateVisibility() {
    if (!enabled_ || !popup_enabled_ || query_->text().trimmed().isEmpty() ||
        popup_->count() == 0) {
        popup_->hide();
        return;
    }
    PositionAndShow();
}

void LookupPopupController::PositionAndShow() {
    const int rows = std::min(kMaximumPopupRows, popup_->count());
    int row_height = popup_->sizeHintForRow(0);
    if (row_height <= 0)
        row_height = popup_->fontMetrics().height() + 4;
    int height = rows * row_height + 2 * popup_->frameWidth();
    if (popup_->horizontalScrollBar()->maximum() > 0) {
        height += popup_->style()->pixelMetric(QStyle::PM_ScrollBarExtent,
                                               nullptr, popup_);
    }

    const QPoint below = popup_->isWindow()
                             ? query_->mapToGlobal(QPoint(0, query_->height()))
                             : query_->mapTo(window_,
                                             QPoint(0, query_->height()));
    QRect geometry(below, QSize(query_->width(), height));
    if (popup_->isWindow()) {
        QScreen* screen = QGuiApplication::screenAt(below);
        if (screen == nullptr)
            screen = query_->screen();
        if (screen != nullptr) {
            const QRect available = screen->availableGeometry();
            if (geometry.bottom() > available.bottom()) {
                geometry.moveBottom(
                    query_->mapToGlobal(QPoint(0, 0)).y());
            }
            geometry.setLeft(std::clamp(
                geometry.left(), available.left(),
                std::max(available.left(),
                         available.right() - geometry.width() + 1)));
        }
    } else if (geometry.bottom() > window_->height()) {
        const int above = query_->mapTo(window_, QPoint(0, 0)).y() - height;
        if (above >= 0)
            geometry.moveTop(above);
        else
            geometry.setBottom(window_->height());
    }
    popup_->setGeometry(geometry);
    popup_->show();
    popup_->raise();
    query_->setFocus(Qt::OtherFocusReason);
}

}  // namespace goldendict::app
