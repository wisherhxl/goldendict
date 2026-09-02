// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_LOOKUP_POPUP_CONTROLLER_H_
#define GOLDENDICT_APPS_GOLDENDICT_LOOKUP_POPUP_CONTROLLER_H_

#include <functional>

#include <QObject>
#include <QStringList>

class QEvent;
class QLineEdit;
class QListWidget;
class QWidget;

namespace goldendict::app {

// Owns only the toolbar lookup field's popup presentation. Dictionary lookup,
// suggestion production, and activation policy remain MainWindow/Core duties.
class LookupPopupController final : public QObject {
   public:
    using ActivationCallback =
        std::function<void(const QString&, Qt::KeyboardModifiers)>;

    LookupPopupController(QWidget* window, QLineEdit* query,
                          ActivationCallback activation);

    void SetEnabled(bool enabled);
    void SetItems(const QStringList& items);
    void TogglePopup();
    void HidePopup();

    [[nodiscard]] QListWidget* popup() const;

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    void AcceptCurrentItem(Qt::KeyboardModifiers modifiers);
    bool ShowPopupIfHidden();
    void UpdateVisibility();
    void PositionAndShow();

    QWidget* window_ = nullptr;
    QLineEdit* query_ = nullptr;
    QListWidget* popup_ = nullptr;
    ActivationCallback activation_;
    bool enabled_ = true;
    bool popup_enabled_ = false;
};

}  // namespace goldendict::app

#endif  // GOLDENDICT_APPS_GOLDENDICT_LOOKUP_POPUP_CONTROLLER_H_
