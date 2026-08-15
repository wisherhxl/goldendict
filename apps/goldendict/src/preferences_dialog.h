// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_PREFERENCES_DIALOG_H_
#define GOLDENDICT_APPS_GOLDENDICT_PREFERENCES_DIALOG_H_

#include <functional>

#include <QDialog>
#include <QString>

#include "goldendict/core/application.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSpinBox;

class PreferencesDialog final : public QDialog {
    Q_OBJECT

   public:
    using ApplyCallback =
        std::function<QString(const goldendict::core::ApplicationPreferences&)>;

    PreferencesDialog(
        const goldendict::core::ApplicationPreferences& preferences,
        ApplyCallback apply_callback, QWidget* parent = nullptr);

   private:
    void Apply();

    goldendict::core::ApplicationPreferences preferences_;
    ApplyCallback apply_callback_;
    QCheckBox* open_after_current_ = nullptr;
    QCheckBox* open_in_background_ = nullptr;
    QCheckBox* hide_single_tab_ = nullptr;
    QCheckBox* mru_tab_order_ = nullptr;
    QCheckBox* escape_hides_main_window_ = nullptr;
    QCheckBox* double_click_translates_ = nullptr;
    QCheckBox* select_word_by_single_click_ = nullptr;
    QCheckBox* store_history_ = nullptr;
    QSpinBox* maximum_history_entries_ = nullptr;
    QCheckBox* confirm_favorites_deletion_ = nullptr;
    QCheckBox* collapse_large_articles_ = nullptr;
    QCheckBox* always_expand_optional_parts_ = nullptr;
    QSpinBox* article_size_limit_ = nullptr;
    QCheckBox* limit_input_phrase_length_ = nullptr;
    QCheckBox* ignore_diacritics_ = nullptr;
    QCheckBox* synonym_search_enabled_ = nullptr;
    QSpinBox* input_phrase_length_limit_ = nullptr;
    QGroupBox* use_proxy_server_ = nullptr;
    QComboBox* proxy_type_ = nullptr;
    QLineEdit* proxy_host_ = nullptr;
    QSpinBox* proxy_port_ = nullptr;
    QLabel* validation_error_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_PREFERENCES_DIALOG_H_
