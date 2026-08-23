// SPDX-License-Identifier: GPL-3.0-or-later

#include "command_line_lookup.h"

#include <QByteArray>
#include <QChar>

#include <utility>

#include "goldendict/core/dictionary_service.h"

namespace goldendict::app {
namespace {

constexpr auto kGoldenDictScheme = "goldendict://";
constexpr auto kDictScheme = "dict://";

bool IsHexDigit(QChar character) {
    const ushort value = character.unicode();
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

bool HasStrictPercentEncoding(const QString& encoded) {
    for (qsizetype index = 0; index < encoded.size(); ++index) {
        if (encoded[index] != QLatin1Char('%')) {
            continue;
        }
        if (index + 2 >= encoded.size() || !IsHexDigit(encoded[index + 1]) ||
            !IsHexDigit(encoded[index + 2])) {
            return false;
        }
        index += 2;
    }
    return true;
}

bool IsBoundedText(const QString& text) {
    if (text.contains(QChar::Null)) {
        return false;
    }
    const QByteArray encoded = text.toUtf8();
    return encoded.size() <= static_cast<qsizetype>(
                                 goldendict::core::kMaximumLookupTextBytes) &&
           QString::fromUtf8(encoded) == text;
}

std::optional<QString> DecodeLegacyUri(QString operand, const QString& scheme) {
    operand.remove(0, scheme.size());
    if (operand.startsWith(QLatin1Char('/'))) {
        operand.remove(0, 1);
    }
    if (operand.size() > 1 && operand.endsWith(QLatin1Char('/'))) {
        operand.chop(1);
    }
    if (!HasStrictPercentEncoding(operand)) {
        return std::nullopt;
    }
    const QByteArray decoded =
        QByteArray::fromPercentEncoding(operand.toUtf8());
    const QString decoded_text = QString::fromUtf8(decoded);
    if (decoded_text.toUtf8() != decoded) {
        return std::nullopt;
    }
    const QString word = decoded_text.trimmed();
    if (word.isEmpty() || !IsBoundedText(word)) {
        return std::nullopt;
    }
    return word;
}

}  // namespace

bool IsNormalizedLookupText(const QString& text) {
    return !text.isEmpty() && text == text.trimmed() && IsBoundedText(text);
}

InitialLookupRequest::InitialLookupRequest(QString word)
    : word_(std::move(word)) {}

const QString& InitialLookupRequest::Word() const noexcept {
    return word_;
}

QString InitialLookupRequest::TakeWord() noexcept {
    return std::exchange(word_, {});
}

std::optional<InitialLookupRequest> ParseInitialLookup(
    const QStringList& arguments) {
    if (arguments.size() != 2) {
        return std::nullopt;
    }
    QString operand = arguments[1];
    if (operand.startsWith(QLatin1Char('-')) || operand.trimmed().isEmpty() ||
        !IsBoundedText(operand)) {
        return std::nullopt;
    }

    std::optional<QString> word;
    const QString golden_dict_scheme = QString::fromLatin1(kGoldenDictScheme);
    const QString dict_scheme = QString::fromLatin1(kDictScheme);
    if (operand.startsWith(golden_dict_scheme)) {
        word = DecodeLegacyUri(std::move(operand), golden_dict_scheme);
    } else if (operand.startsWith(dict_scheme)) {
        word = DecodeLegacyUri(std::move(operand), dict_scheme);
    } else if (operand.contains(QStringLiteral("://"))) {
        return std::nullopt;
    } else {
        word = operand.trimmed();
    }
    if (!word || !IsNormalizedLookupText(*word)) {
        return std::nullopt;
    }
    return InitialLookupRequest(std::move(*word));
}

}  // namespace goldendict::app
