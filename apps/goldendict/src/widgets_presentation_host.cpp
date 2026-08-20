// SPDX-License-Identifier: GPL-3.0-or-later

#include "widgets_presentation_host.h"

#include <algorithm>

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QResizeEvent>
#include <QToolButton>

WidgetsPresentationHost::WidgetsPresentationHost(QWidget* parent)
    : QStackedWidget(parent) {
    setContentsMargins(0, 0, 0, 0);
}

void WidgetsPresentationHost::InstallActive(QWidget* active) {
    Q_ASSERT(active_ == nullptr && active != nullptr);
    active_ = active;
    setSizePolicy(active->sizePolicy());
    addWidget(active);
    setCurrentWidget(active);
}

bool WidgetsPresentationHost::AttachInactive(QWidget* inactive) {
    if (inactive == nullptr || inactive_ != nullptr)
        return false;
    inactive_ = inactive;
    inactive->setEnabled(false);
    inactive->setFocusPolicy(Qt::NoFocus);
    addWidget(inactive);
    setCurrentWidget(active_);
    return indexOf(inactive) >= 0;
}

void WidgetsPresentationHost::DetachInactive(QWidget* inactive) noexcept {
    if (inactive_ != inactive)
        return;
    setCurrentWidget(active_);
    removeWidget(inactive);
    inactive_ = nullptr;
}

QWidget* WidgetsPresentationHost::ActivePage() const noexcept {
    return active_;
}

QWidget* WidgetsPresentationHost::InactivePage() const noexcept {
    return inactive_;
}

bool WidgetsPresentationHost::Prepared() const noexcept {
    return active_ != nullptr && inactive_ != nullptr &&
           indexOf(active_) == 0 && indexOf(inactive_) == 1 &&
           currentWidget() == active_ && !inactive_->isEnabled();
}

bool WidgetsPresentationHost::AuditMaintenanceSwitch() noexcept {
    if (!Prepared())
        return false;
    const int count_before = count();
    QWidget* const active_parent = active_->parentWidget();
    QWidget* const inactive_parent = inactive_->parentWidget();
    setCurrentWidget(inactive_);
    const bool switched = currentWidget() == inactive_ &&
                          count() == count_before &&
                          active_->parentWidget() == active_parent &&
                          inactive_->parentWidget() == inactive_parent;
    setCurrentWidget(active_);
    return switched && currentWidget() == active_ && count() == count_before;
}

void WidgetsPresentationHost::PreserveFirstShownWidth() noexcept {
    preserve_width_ = true;
}

void WidgetsPresentationHost::RefreshPreservedWidth() noexcept {
    preserved_width_ = preserve_width_ && isVisible() && active_ != nullptr
                           ? active_->sizeHint().width()
                           : -1;
    updateGeometry();
    if (parentWidget() != nullptr && parentWidget()->layout() != nullptr)
        parentWidget()->layout()->activate();
    if (window() != nullptr && window()->layout() != nullptr)
        window()->layout()->activate();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
}

QSize WidgetsPresentationHost::sizeHint() const {
    QSize result = active_ == nullptr ? QSize{} : active_->sizeHint();
    if (preserve_width_ && preserved_width_ >= 0)
        result.setWidth(preserved_width_);
    return result;
}

QSize WidgetsPresentationHost::minimumSizeHint() const {
    return active_ == nullptr ? QSize{}
                              : QSize(0, active_->minimumSizeHint().height());
}

void WidgetsPresentationHost::showEvent(QShowEvent* event) {
    QStackedWidget::showEvent(event);
    if (preserve_width_ && preserved_width_ < 0)
        preserved_width_ = width();
}

struct DictionaryBarPresentationHost::PageState final {
    QWidget* page = nullptr;
    QHBoxLayout* layout = nullptr;
    QList<QAction*> actions;
    QList<QToolButton*> buttons;
    QToolButton* overflow = nullptr;
    QMenu* overflow_menu = nullptr;
};

DictionaryBarPresentationHost::DictionaryBarPresentationHost(QWidget* parent)
    : QStackedWidget(parent) {
    setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto* page = new QWidget(this);
    active_ = new PageState;
    active_->page = page;
    active_->layout = new QHBoxLayout(page);
    active_->layout->setContentsMargins(0, 0, 0, 0);
    active_->layout->setSpacing(0);
    active_->overflow = new QToolButton(page);
    active_->overflow->setText(QStringLiteral("…"));
    active_->overflow->setPopupMode(QToolButton::InstantPopup);
    active_->overflow_menu = new QMenu(active_->overflow);
    active_->overflow->setMenu(active_->overflow_menu);
    active_->overflow->hide();
    active_->layout->addWidget(active_->overflow);
    addWidget(page);
    setCurrentWidget(page);
}

DictionaryBarPresentationHost::~DictionaryBarPresentationHost() {
    delete inactive_;
    delete active_;
}

QWidget* DictionaryBarPresentationHost::ActivePage() const noexcept {
    return active_->page;
}

QWidget* DictionaryBarPresentationHost::AttachInactivePage() {
    if (inactive_ != nullptr)
        return nullptr;
    auto* page = new QWidget(this);
    page->setEnabled(false);
    page->setFocusPolicy(Qt::NoFocus);
    inactive_ = new PageState;
    inactive_->page = page;
    inactive_->layout = new QHBoxLayout(page);
    inactive_->layout->setContentsMargins(0, 0, 0, 0);
    inactive_->layout->setSpacing(0);
    inactive_->overflow = new QToolButton(page);
    inactive_->overflow->setText(QStringLiteral("…"));
    inactive_->overflow->setPopupMode(QToolButton::InstantPopup);
    inactive_->overflow_menu = new QMenu(inactive_->overflow);
    inactive_->overflow->setMenu(inactive_->overflow_menu);
    inactive_->overflow->hide();
    inactive_->layout->addWidget(inactive_->overflow);
    addWidget(page);
    setCurrentWidget(active_->page);
    return page;
}

void DictionaryBarPresentationHost::DetachInactivePage(QWidget* page) noexcept {
    if (inactive_ == nullptr || inactive_->page != page)
        return;
    setCurrentWidget(active_->page);
    removeWidget(page);
    delete inactive_;
    inactive_ = nullptr;
}

DictionaryBarPresentationHost::PageState*
DictionaryBarPresentationHost::StateFor(QWidget* page) const noexcept {
    if (active_->page == page)
        return active_;
    return inactive_ != nullptr && inactive_->page == page ? inactive_
                                                           : nullptr;
}

QAction* DictionaryBarPresentationHost::AddAction(QWidget* page,
                                                  const QString& label) {
    auto* state = StateFor(page);
    if (state == nullptr)
        return nullptr;
    auto* action = new QAction(label, page);
    auto* button = new QToolButton(page);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    state->actions.push_back(action);
    state->buttons.push_back(button);
    state->layout->insertWidget(state->layout->count() - 1, button);
    UpdateOverflow(state);
    return action;
}

void DictionaryBarPresentationHost::ClearActive() {
    for (auto* button : active_->buttons)
        delete button;
    for (auto* action : active_->actions)
        delete action;
    active_->buttons.clear();
    active_->actions.clear();
    active_->overflow_menu->clear();
    active_->overflow->hide();
}

QList<QAction*> DictionaryBarPresentationHost::ActiveActions() const {
    return active_->actions;
}

QWidget* DictionaryBarPresentationHost::ActiveWidgetForAction(
    QAction* action) const noexcept {
    const int index = active_->actions.indexOf(action);
    return index < 0 ? nullptr : active_->buttons[index];
}

bool DictionaryBarPresentationHost::Prepared() const noexcept {
    return inactive_ != nullptr && count() == 2 &&
           indexOf(active_->page) == 0 && indexOf(inactive_->page) == 1 &&
           currentWidget() == active_->page && !inactive_->page->isEnabled();
}

bool DictionaryBarPresentationHost::AuditMaintenanceSwitch() noexcept {
    if (!Prepared())
        return false;
    const int count_before = count();
    QWidget* const active_parent = active_->page->parentWidget();
    QWidget* const inactive_parent = inactive_->page->parentWidget();
    setCurrentWidget(inactive_->page);
    const bool switched = currentWidget() == inactive_->page &&
                          count() == count_before &&
                          active_->page->parentWidget() == active_parent &&
                          inactive_->page->parentWidget() == inactive_parent;
    setCurrentWidget(active_->page);
    return switched && currentWidget() == active_->page;
}

QSize DictionaryBarPresentationHost::sizeHint() const {
    if (active_ == nullptr)
        return {};
    QSize result;
    for (const auto* button : active_->buttons) {
        const QSize button_size = button->sizeHint();
        result.rwidth() += button_size.width();
        result.setHeight(std::max(result.height(), button_size.height()));
    }
    result.rwidth() += active_->overflow->sizeHint().width();
    const auto margins = active_->layout->contentsMargins();
    result.rwidth() += margins.left() + margins.right();
    result.rheight() += margins.top() + margins.bottom();
    return result;
}

QSize DictionaryBarPresentationHost::minimumSizeHint() const {
    return active_ == nullptr
               ? QSize{}
               : QSize(0, active_->overflow->sizeHint().height());
}

void DictionaryBarPresentationHost::resizeEvent(QResizeEvent* event) {
    QStackedWidget::resizeEvent(event);
    UpdateOverflow(active_);
    if (inactive_ != nullptr)
        UpdateOverflow(inactive_);
}

void DictionaryBarPresentationHost::UpdateOverflow(PageState* state) {
    if (state == nullptr || state->buttons.empty())
        return;
    int used = 0;
    int first_hidden = state->buttons.size();
    const int reserve = state->overflow->sizeHint().width();
    for (int index = 0; index < state->buttons.size(); ++index) {
        const int button_width = state->buttons[index]->sizeHint().width();
        if (used + button_width + reserve > this->width()) {
            first_hidden = index;
            break;
        }
        used += button_width;
    }
    state->overflow_menu->clear();
    for (int index = 0; index < state->buttons.size(); ++index) {
        const bool visible = index < first_hidden;
        state->buttons[index]->setVisible(visible);
        if (!visible)
            state->overflow_menu->addAction(state->actions[index]);
    }
    state->overflow->setVisible(first_hidden < state->buttons.size());
}
