// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_WIDGETS_PRESENTATION_HOST_H_
#define GOLDENDICT_APPS_GOLDENDICT_WIDGETS_PRESENTATION_HOST_H_

#include <QList>
#include <QPointer>
#include <QStackedWidget>
#include <QString>

class QAction;
class QToolButton;

class WidgetsPresentationHost final : public QStackedWidget {
   public:
    explicit WidgetsPresentationHost(QWidget* parent = nullptr);

    void InstallActive(QWidget* active);
    bool AttachInactive(QWidget* inactive);
    void DetachInactive(QWidget* inactive) noexcept;
    QWidget* ActivePage() const noexcept;
    QWidget* InactivePage() const noexcept;
    bool Prepared() const noexcept;
    bool AuditMaintenanceSwitch() noexcept;
    void PreserveFirstShownWidth() noexcept;
    void RefreshPreservedWidth() noexcept;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

   private:
    void showEvent(QShowEvent* event) override;

    QPointer<QWidget> active_;
    QPointer<QWidget> inactive_;
    bool preserve_width_ = false;
    int preserved_width_ = -1;
};

class DictionaryBarPresentationHost final : public QStackedWidget {
   public:
    explicit DictionaryBarPresentationHost(QWidget* parent = nullptr);
    ~DictionaryBarPresentationHost() override;

    QWidget* ActivePage() const noexcept;
    QWidget* AttachInactivePage();
    void DetachInactivePage(QWidget* page) noexcept;
    QAction* AddAction(QWidget* page, const QString& label);
    void ClearActive();
    QList<QAction*> ActiveActions() const;
    QWidget* ActiveWidgetForAction(QAction* action) const noexcept;
    bool Prepared() const noexcept;
    bool AuditMaintenanceSwitch() noexcept;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

   protected:
    void resizeEvent(QResizeEvent* event) override;

   private:
    struct PageState;
    PageState* StateFor(QWidget* page) const noexcept;
    void UpdateOverflow(PageState* state);

    PageState* active_ = nullptr;
    PageState* inactive_ = nullptr;
};

#endif
