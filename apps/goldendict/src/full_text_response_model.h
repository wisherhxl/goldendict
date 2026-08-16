// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APP_FULL_TEXT_RESPONSE_MODEL_H_
#define GOLDENDICT_APP_FULL_TEXT_RESPONSE_MODEL_H_

#include <vector>

#include <QAbstractListModel>

#include "goldendict/core/dictionary_service.h"

namespace goldendict::app {

class FullTextResponseModel final : public QAbstractListModel {
    Q_OBJECT

   public:
    explicit FullTextResponseModel(QObject* parent = nullptr);
    explicit FullTextResponseModel(goldendict::core::FullTextResponse response,
                                   QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;

    void Reset(goldendict::core::FullTextResponse response);
    const goldendict::core::FullTextResult* ResultAt(
        const QModelIndex& index) const noexcept;

   private:
    std::vector<goldendict::core::FullTextResult> results_;
};

}  // namespace goldendict::app

#endif  // GOLDENDICT_APP_FULL_TEXT_RESPONSE_MODEL_H_
