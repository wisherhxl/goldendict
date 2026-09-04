// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../src/application/desktop_facade_activation_owner.h"
#include "../src/application/full_text_index_lifecycle_inspection.h"
#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

namespace {

constexpr char kRawSchema[] = "goldendict-real-dictionary-raw-observation-v1";

bool IsSupportedScenario(const QString& scenario) {
    return scenario == QStringLiteral("clean-discovery") ||
           scenario == QStringLiteral("warm-restart") ||
           scenario == QStringLiteral("explicit-rescan") ||
           scenario == QStringLiteral("changed-source") ||
           scenario == QStringLiteral("cancellation") ||
           scenario == QStringLiteral("cancellation-recovery") ||
           scenario == QStringLiteral("unavailable-companion") ||
           scenario == QStringLiteral("companion-recovery");
}

QString PhaseName(const QString& scenario) {
    if (scenario == QStringLiteral("warm-restart")) {
        return QStringLiteral("restart");
    }
    if (scenario == QStringLiteral("explicit-rescan")) {
        return QStringLiteral("rescan");
    }
    if (scenario == QStringLiteral("changed-source")) {
        return QStringLiteral("source-change");
    }
    if (scenario == QStringLiteral("cancellation") ||
        scenario == QStringLiteral("cancellation-recovery")) {
        return QStringLiteral("full-text-indexing");
    }
    if (scenario == QStringLiteral("unavailable-companion")) {
        return QStringLiteral("companion-unavailable");
    }
    if (scenario == QStringLiteral("companion-recovery")) {
        return QStringLiteral("companion-recovery");
    }
    return QStringLiteral("discovery");
}

struct Options {
    QString dictionary_root;
    QString index_root;
    QString interface_language;
    QString conditions_sha256;
    QString scenario;
    QString output;
};

std::optional<Options> ParseOptions(const QStringList& arguments) {
    if (arguments.size() != 13) {
        return std::nullopt;
    }
    QSet<QString> seen;
    Options options;
    for (int index = 1; index < arguments.size(); index += 2) {
        const QString& name = arguments[index];
        const QString& value = arguments[index + 1];
        if (value.isEmpty() || seen.contains(name)) {
            return std::nullopt;
        }
        seen.insert(name);
        if (name == QStringLiteral("--dictionary-root")) {
            options.dictionary_root = value;
        } else if (name == QStringLiteral("--index-root")) {
            options.index_root = value;
        } else if (name == QStringLiteral("--interface-language")) {
            options.interface_language = value;
        } else if (name == QStringLiteral("--conditions-sha256")) {
            options.conditions_sha256 = value;
        } else if (name == QStringLiteral("--scenario")) {
            options.scenario = value;
        } else if (name == QStringLiteral("--output")) {
            options.output = value;
        } else {
            return std::nullopt;
        }
    }
    if (seen.size() != 6 || !IsSupportedScenario(options.scenario)) {
        return std::nullopt;
    }
    return options;
}

std::string Utf8(const QString& value) {
    const QByteArray encoded = value.toUtf8();
    return std::string(encoded.constData(),
                       static_cast<std::size_t>(encoded.size()));
}

QString Text(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString FormatFromId(const std::string& id) {
    const auto separator = id.find('-');
    return Text(separator == std::string::npos ? id : id.substr(0, separator));
}

QString ErrorCode(goldendict::core::LookupErrorCode code) {
    using goldendict::core::LookupErrorCode;
    switch (code) {
        case LookupErrorCode::kInvalidQuery:
            return QStringLiteral("invalid-query");
        case LookupErrorCode::kDictionaryUnavailable:
            return QStringLiteral("dictionary-unavailable");
        case LookupErrorCode::kCancelled:
            return QStringLiteral("cancelled");
        case LookupErrorCode::kDeadlineExceeded:
            return QStringLiteral("deadline-exceeded");
        case LookupErrorCode::kUnsupported:
            return QStringLiteral("unsupported");
        case LookupErrorCode::kInternal:
            return QStringLiteral("internal");
    }
    return QStringLiteral("internal");
}

QJsonObject CatalogEntry(const goldendict::core::DictionaryIdentity& identity,
                         qint64 order) {
    QJsonArray components;
    components.append(Text(identity.source));
    return {
        {QStringLiteral("article_count"),
         static_cast<qint64>(identity.article_count)},
        {QStringLiteral("components"), components},
        {QStringLiteral("edition"), Text(identity.edition)},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("format"), FormatFromId(identity.id)},
        {QStringLiteral("headword_count"),
         static_cast<qint64>(identity.headword_count)},
        {QStringLiteral("id"), Text(identity.id)},
        {QStringLiteral("name"), Text(identity.name)},
        {QStringLiteral("order"), order},
        {QStringLiteral("source_language"), Text(identity.source_language)},
        {QStringLiteral("target_language"), Text(identity.target_language)},
    };
}

QJsonObject ErrorEntry(const goldendict::core::LookupError& error) {
    return {
        {QStringLiteral("code"), ErrorCode(error.code)},
        {QStringLiteral("dictionary_id"), Text(error.dictionary_id)},
        {QStringLiteral("message"), Text(error.message)},
    };
}

bool WriteAtomically(const QString& path, const QJsonObject& value) {
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray content =
        QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
    return output.write(content) == content.size() && output.commit();
}

std::optional<std::string> LifecycleDictionaryId(
    const goldendict::core::DictionaryService& service,
    const std::vector<goldendict::core::DictionaryIdentity>& identities) {
    for (const auto& identity : identities) {
        const auto snapshot =
            goldendict::core::application::FullTextIndexLifecycleSnapshot(
                service, identity.id);
        if (snapshot.has_value() && snapshot->format_capable()) {
            return identity.id;
        }
    }
    return std::nullopt;
}

goldendict::core::dictionary::FullTextIndexLifecycleSnapshot WaitForState(
    const goldendict::core::DictionaryService& service,
    const std::string& dictionary_id,
    goldendict::core::dictionary::FullTextIndexLifecycleState expected) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto snapshot =
            goldendict::core::application::FullTextIndexLifecycleSnapshot(
                service, dictionary_id);
        if (!snapshot.has_value()) {
            throw std::runtime_error("full-text lifecycle disappeared");
        }
        if (snapshot->state() == expected) {
            return *snapshot;
        }
        if (snapshot->state() == goldendict::core::dictionary::
                                     FullTextIndexLifecycleState::kFailed ||
            snapshot->state() == goldendict::core::dictionary::
                                     FullTextIndexLifecycleState::kCancelled ||
            snapshot->state() == goldendict::core::dictionary::
                                     FullTextIndexLifecycleState::kCurrent) {
            throw std::runtime_error(
                "full-text lifecycle reached unexpected terminal state " +
                std::to_string(static_cast<int>(snapshot->state())));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error(
        "timed out observing full-text lifecycle progress");
}

QJsonObject Phase(const QString& name, const QString& status, int sequence,
                  const std::string& dictionary_id = {}) {
    return {{QStringLiteral("dictionary_id"), Text(dictionary_id)},
            {QStringLiteral("name"), name},
            {QStringLiteral("sequence"), sequence},
            {QStringLiteral("status"), status}};
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    const auto options = ParseOptions(application.arguments());
    if (!options.has_value()) {
        std::cerr << "usage: qt6_real_dictionary_observer --dictionary-root "
                     "PATH --index-root PATH --interface-language LOCALE "
                     "--conditions-sha256 HASH --scenario SCENARIO "
                     "--output PATH\n";
        return 2;
    }

    try {
        goldendict::core::CoreConfiguration configuration;
        configuration.dictionary_paths = {Utf8(options->dictionary_root)};
        configuration.index_directory = Utf8(options->index_root);
        configuration.preferences.interface_language =
            Utf8(options->interface_language);

        QElapsedTimer timer;
        timer.start();
        goldendict::core::application::DesktopFacadeActivationOwner owner;
        auto initial_candidate = owner.PrepareCandidate(configuration);
        const bool cancellation =
            options->scenario == QStringLiteral("cancellation");
        const bool activated =
            initial_candidate && owner.Activate(initial_candidate);
        if (!activated) {
            throw std::runtime_error("could not activate initial dictionary runtime");
        }
        auto facade = owner.CurrentSnapshot();
        if (!facade) {
            throw std::runtime_error("initial dictionary runtime is unavailable");
        }
        if (options->scenario == QStringLiteral("explicit-rescan")) {
            const auto previous = facade;
            auto rescan_candidate = owner.PrepareCandidate(configuration);
            const auto prepared = owner.PreparedFacadeSnapshot(rescan_candidate);
            if (!rescan_candidate || !prepared || prepared == previous ||
                !owner.Activate(rescan_candidate)) {
                throw std::runtime_error("could not activate rescanned dictionary runtime");
            }
            facade = owner.CurrentSnapshot();
            if (!facade || facade != prepared || facade == previous) {
                throw std::runtime_error("rescanned dictionary runtime was not published");
            }
        }
        auto& service = facade->GetDictionaryService();
        const auto identities = service.GetCatalog();
        QJsonArray catalog;
        qint64 order = 0;
        for (const auto& identity : identities) {
            catalog.append(CatalogEntry(identity, order));
            ++order;
        }

        const QString phase_name = PhaseName(options->scenario);
        QJsonArray phases;
        QString outcome = QStringLiteral("completed");
        if (options->scenario == QStringLiteral("cancellation") ||
            options->scenario == QStringLiteral("cancellation-recovery")) {
            const auto dictionary_id =
                LifecycleDictionaryId(service, identities);
            if (!dictionary_id.has_value()) {
                throw std::runtime_error(
                    "no full-text lifecycle dictionary was discovered");
            }
            if (cancellation) {
                static_cast<void>(
                    WaitForState(service, *dictionary_id,
                                 goldendict::core::dictionary::
                                     FullTextIndexLifecycleState::kWorking));
            }
            phases.append(Phase(phase_name, QStringLiteral("started"), 0,
                                *dictionary_id));
            if (cancellation) {
                if (!goldendict::core::application::
                        CancelFullTextIndexLifecycleWork(service,
                                                         *dictionary_id)) {
                    throw std::runtime_error(
                        "could not cancel active full-text indexing");
                }
                if (!owner.Shutdown()) {
                    throw std::runtime_error(
                        "could not stop full-text indexing safely");
                }
                if (!goldendict::core::application::
                        IsFullTextIndexExecutorStopped(service)) {
                    throw std::runtime_error(
                        "could not stop full-text indexing safely");
                }
                const auto cancelled = goldendict::core::application::
                    FullTextIndexLifecycleSnapshot(service, *dictionary_id);
                if (!cancelled.has_value() ||
                    cancelled->state() !=
                        goldendict::core::dictionary::
                            FullTextIndexLifecycleState::kCancelled) {
                    throw std::runtime_error(
                        "full-text indexing did not reach cancelled state: " +
                        (cancelled.has_value()
                             ? std::to_string(
                                   static_cast<int>(cancelled->state()))
                             : std::string("missing")));
                }
                phases.append(Phase(phase_name, QStringLiteral("cancelled"), 1,
                                    *dictionary_id));
                outcome = QStringLiteral("cancelled");
            } else {
                static_cast<void>(
                    WaitForState(service, *dictionary_id,
                                 goldendict::core::dictionary::
                                     FullTextIndexLifecycleState::kCurrent));
                phases.append(Phase(phase_name, QStringLiteral("completed"), 1,
                                    *dictionary_id));
            }
        } else {
            phases.append(Phase(phase_name, QStringLiteral("started"), 0));
            phases.append(Phase(phase_name, QStringLiteral("completed"), 1));
        }

        goldendict::core::LookupQuery query;
        query.text = "__goldendict_acceptance_probe__";
        query.result_limit = 1U;
        query.timeout = std::chrono::seconds(5);
        QJsonArray errors;
        for (const auto& error : service.Lookup(query).errors) {
            errors.append(ErrorEntry(error));
        }
        const QJsonObject raw{
            {QStringLiteral("catalog"), catalog},
            {QStringLiteral("conditions_sha256"), options->conditions_sha256},
            {QStringLiteral("elapsed_milliseconds"), timer.elapsed()},
            {QStringLiteral("errors"), errors},
            {QStringLiteral("outcome"), outcome},
            {QStringLiteral("phases"), phases},
            {QStringLiteral("scenario"), options->scenario},
            {QStringLiteral("schema"), QString::fromLatin1(kRawSchema)},
        };
        if (!WriteAtomically(options->output, raw)) {
            std::cerr << "could not publish raw observation\n";
            return 3;
        }
    } catch (const std::exception& error) {
        std::cerr << "dictionary observation failed: " << error.what() << '\n';
        return 4;
    }
    return 0;
}
