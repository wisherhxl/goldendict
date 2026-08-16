// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APP_FULL_TEXT_QUERY_COMPOSER_H_
#define GOLDENDICT_APP_FULL_TEXT_QUERY_COMPOSER_H_

#include <QWidget>

#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace goldendict::app {

class FullTextQueryComposer final : public QWidget {
    Q_OBJECT

   public:
    explicit FullTextQueryComposer(
        const goldendict::core::ApplicationPreferences& preferences,
        QWidget* parent = nullptr);

    goldendict::core::FullTextQuery Compose() const;

   private:
    void UpdateModeDependentControls();

    QLineEdit* query_text_ = nullptr;
    QComboBox* mode_ = nullptr;
    QCheckBox* match_case_ = nullptr;
    QCheckBox* ignore_diacritics_ = nullptr;
    QCheckBox* ignore_word_order_ = nullptr;
    QCheckBox* use_maximum_word_distance_ = nullptr;
    QSpinBox* maximum_word_distance_ = nullptr;
    QCheckBox* use_maximum_articles_ = nullptr;
    QSpinBox* maximum_articles_per_dictionary_ = nullptr;
};

}  // namespace goldendict::app

#endif  // GOLDENDICT_APP_FULL_TEXT_QUERY_COMPOSER_H_
