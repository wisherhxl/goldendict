// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_response_model.h"

#include <QString>

#include <cstddef>
#include <utility>

namespace goldendict::app {

FullTextResponseModel::FullTextResponseModel(QObject* parent)
    : QAbstractListModel(parent) {}

FullTextResponseModel::FullTextResponseModel(
    goldendict::core::FullTextResponse response, QObject* parent)
    : QAbstractListModel(parent), results_(std::move(response.results)) {}

int FullTextResponseModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(results_.size());
}

QVariant FullTextResponseModel::data(const QModelIndex& index, int role) const {
    const auto* result = ResultAt(index);
    if (result == nullptr || role != Qt::DisplayRole) {
        return {};
    }
    return QString::fromUtf8(result->headword.data(),
                             static_cast<int>(result->headword.size()));
}

void FullTextResponseModel::Reset(goldendict::core::FullTextResponse response) {
    beginResetModel();
    results_ = std::move(response.results);
    endResetModel();
}

const goldendict::core::FullTextResult* FullTextResponseModel::ResultAt(
    const QModelIndex& index) const noexcept {
    if (!index.isValid() || index.model() != this || index.column() != 0 ||
        index.row() < 0 ||
        static_cast<std::size_t>(index.row()) >= results_.size()) {
        return nullptr;
    }
    return &results_[static_cast<std::size_t>(index.row())];
}

}  // namespace goldendict::app
