// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_COMMAND_LINE_LOOKUP_H_
#define GOLDENDICT_COMMAND_LINE_LOOKUP_H_

#include <QString>
#include <QStringList>

#include <optional>

namespace goldendict::app {

class InitialLookupRequest {
   public:
    InitialLookupRequest(const InitialLookupRequest&) = delete;
    InitialLookupRequest& operator=(const InitialLookupRequest&) = delete;
    InitialLookupRequest(InitialLookupRequest&&) noexcept = default;
    InitialLookupRequest& operator=(InitialLookupRequest&&) noexcept = default;

    QString TakeWord() noexcept;

   private:
    friend std::optional<InitialLookupRequest> ParseInitialLookup(
        const QStringList& arguments);

    explicit InitialLookupRequest(QString word);

    QString word_;
};

std::optional<InitialLookupRequest> ParseInitialLookup(
    const QStringList& arguments);

}  // namespace goldendict::app

#endif  // GOLDENDICT_COMMAND_LINE_LOOKUP_H_
