// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../src/application/desktop_facade_activation_owner.h"
#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

namespace {

constexpr char kCatalogSchema[] = "goldendict-real-management-catalog-v1";
constexpr char kRawSchema[] = "goldendict-real-management-raw-observation-v1";
constexpr qint64 kMaximumCatalogBytes = 1024 * 1024;

struct Options {
    QString dictionary_root;
    QString index_root;
    QString configuration;
    QString conditions_sha256;
    QString catalog;
    QString scenario;
    QString output;
};

std::optional<Options> ParseOptions(const QStringList& arguments) {
    if (arguments.size() != 15)
        return std::nullopt;
    Options options;
    QSet<QString> seen;
    for (int index = 1; index < arguments.size(); index += 2) {
        const QString& name = arguments[index];
        const QString& value = arguments[index + 1];
        if (value.isEmpty() || seen.contains(name))
            return std::nullopt;
        seen.insert(name);
        if (name == QStringLiteral("--dictionary-root"))
            options.dictionary_root = value;
        else if (name == QStringLiteral("--index-root"))
            options.index_root = value;
        else if (name == QStringLiteral("--configuration"))
            options.configuration = value;
        else if (name == QStringLiteral("--conditions-sha256"))
            options.conditions_sha256 = value;
        else if (name == QStringLiteral("--catalog"))
            options.catalog = value;
        else if (name == QStringLiteral("--scenario"))
            options.scenario = value;
        else if (name == QStringLiteral("--output"))
            options.output = value;
        else
            return std::nullopt;
    }
    if (seen.size() != 7 ||
        (options.scenario != QStringLiteral("clean-discovery") &&
         options.scenario != QStringLiteral("warm-restart"))) {
        return std::nullopt;
    }
    return options;
}

std::string Utf8(const QString& value) {
    const QByteArray encoded = value.toUtf8();
    return {encoded.constData(), static_cast<std::size_t>(encoded.size())};
}

QString Text(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QByteArray ReadBounded(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 1 ||
        file.size() > kMaximumCatalogBytes) {
        throw std::runtime_error("cannot read bounded management catalog");
    }
    const QByteArray content = file.readAll();
    if (content.size() != file.size())
        throw std::runtime_error("cannot read complete management catalog");
    return content;
}

QJsonObject ReadCatalog(const QByteArray& content) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error("management catalog is not valid JSON");
    const QJsonObject catalog = document.object();
    if (catalog.value(QStringLiteral("schema")).toString() !=
        QString::fromLatin1(kCatalogSchema)) {
        throw std::runtime_error("management catalog schema is invalid");
    }
    return catalog;
}

QString CanonicalFile(const QString& root, const QString& relative) {
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) ||
        relative.split(QLatin1Char('/')).contains(QStringLiteral(".."))) {
        throw std::runtime_error("management catalog component path is unsafe");
    }
    const QString canonical_root = QDir(root).canonicalPath();
    const QString path =
        QFileInfo(QDir(canonical_root).filePath(relative)).canonicalFilePath();
    if (canonical_root.isEmpty() || path.isEmpty() ||
        !QFileInfo(path).isFile() ||
        !path.startsWith(canonical_root + QLatin1Char('/'),
                         Qt::CaseInsensitive)) {
        throw std::runtime_error(
            "management catalog component is outside corpus");
    }
    return QDir::cleanPath(path);
}

struct SelectedDictionary {
    QString catalog_id;
    QString primary;
    goldendict::core::DictionaryIdentity identity;
};

std::vector<SelectedDictionary> SelectDictionaries(
    const QJsonObject& catalog, const QString& root,
    const goldendict::core::DictionaryService& service) {
    const auto identities = service.GetCatalog();
    std::vector<SelectedDictionary> selected;
    for (const QJsonValue& value :
         catalog.value(QStringLiteral("dictionaries")).toArray()) {
        const QJsonObject item = value.toObject();
        const QString primary = CanonicalFile(
            root, item.value(QStringLiteral("primary_component")).toString());
        const auto found = std::find_if(
            identities.begin(), identities.end(),
            [&primary](const auto& identity) {
                return QFileInfo(Text(identity.source))
                           .canonicalFilePath()
                           .compare(primary, Qt::CaseInsensitive) == 0;
            });
        if (found == identities.end())
            throw std::runtime_error(
                "cataloged management dictionary was not discovered");
        selected.push_back(
            {item.value(QStringLiteral("id")).toString(), primary, *found});
    }
    return selected;
}

const SelectedDictionary& ByCatalogId(
    const std::vector<SelectedDictionary>& selected, const QString& id) {
    const auto found = std::find_if(
        selected.begin(), selected.end(),
        [&id](const auto& dictionary) { return dictionary.catalog_id == id; });
    if (found == selected.end())
        throw std::runtime_error(
            "management workflow references an unknown dictionary");
    return *found;
}

std::vector<std::string> ResolveIds(
    const QJsonArray& aliases,
    const std::vector<SelectedDictionary>& selected) {
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(aliases.size()));
    for (const QJsonValue& alias : aliases)
        result.push_back(ByCatalogId(selected, alias.toString()).identity.id);
    return result;
}

void ApplyWorkflow(const QJsonObject& catalog,
                   const std::vector<SelectedDictionary>& selected,
                   goldendict::core::CoreConfiguration* configuration) {
    configuration->dictionary_groups.clear();
    const QJsonArray groups = catalog.value(QStringLiteral("workflow"))
                                  .toObject()
                                  .value(QStringLiteral("groups"))
                                  .toArray();
    for (const QJsonValue& value : groups) {
        const QJsonObject group = value.toObject();
        configuration->dictionary_groups.push_back(
            {static_cast<std::uint32_t>(
                 group.value(QStringLiteral("id")).toInteger()),
             Utf8(group.value(QStringLiteral("name")).toString()),
             {},
             ResolveIds(group.value(QStringLiteral("dictionary_ids")).toArray(),
                        selected),
             ResolveIds(
                 group.value(QStringLiteral("muted_dictionary_ids")).toArray(),
                 selected),
             ResolveIds(
                 group.value(QStringLiteral("popup_muted_dictionary_ids"))
                     .toArray(),
                 selected)});
    }
}

QString AliasForRuntimeId(const std::vector<SelectedDictionary>& selected,
                          const std::string& id) {
    const auto found = std::find_if(
        selected.begin(), selected.end(),
        [&id](const auto& dictionary) { return dictionary.identity.id == id; });
    if (found == selected.end())
        throw std::runtime_error(
            "persisted management group contains an unknown ID");
    return found->catalog_id;
}

QJsonArray AliasArray(const std::vector<std::string>& ids,
                      const std::vector<SelectedDictionary>& selected) {
    QJsonArray result;
    for (const auto& id : ids)
        result.append(AliasForRuntimeId(selected, id));
    return result;
}

QJsonArray Groups(const goldendict::core::CoreConfiguration& configuration,
                  const std::vector<SelectedDictionary>& selected) {
    QJsonArray groups;
    for (const auto& group : configuration.dictionary_groups) {
        groups.append(QJsonObject{
            {QStringLiteral("dictionary_ids"),
             AliasArray(group.dictionary_ids, selected)},
            {QStringLiteral("id"), static_cast<qint64>(group.id)},
            {QStringLiteral("muted_dictionary_ids"),
             AliasArray(group.muted_dictionary_ids, selected)},
            {QStringLiteral("name"), Text(group.name)},
            {QStringLiteral("popup_muted_dictionary_ids"),
             AliasArray(group.popup_muted_dictionary_ids, selected)},
        });
    }
    return groups;
}

QJsonObject Dictionary(const SelectedDictionary& selected) {
    const auto& identity = selected.identity;
    return {
        {QStringLiteral("article_count"),
         static_cast<qint64>(identity.article_count)},
        {QStringLiteral("catalog_id"), selected.catalog_id},
        {QStringLiteral("components"), QJsonArray{selected.primary}},
        {QStringLiteral("description"), Text(identity.description)},
        {QStringLiteral("headword_count"),
         static_cast<qint64>(identity.headword_count)},
        {QStringLiteral("name"), Text(identity.name)},
        {QStringLiteral("source_language"), Text(identity.source_language)},
        {QStringLiteral("target_language"), Text(identity.target_language)},
    };
}

QJsonArray Browse(const QJsonObject& catalog,
                  const std::vector<SelectedDictionary>& selected,
                  const goldendict::core::DictionaryService& service,
                  std::vector<std::pair<std::string, std::string>>* cursors) {
    QJsonArray result;
    const QJsonArray requests = catalog.value(QStringLiteral("workflow"))
                                    .toObject()
                                    .value(QStringLiteral("browse"))
                                    .toArray();
    for (const QJsonValue& value : requests) {
        const QJsonObject request = value.toObject();
        const QString alias =
            request.value(QStringLiteral("dictionary_id")).toString();
        const auto& dictionary = ByCatalogId(selected, alias);
        goldendict::core::HeadwordEnumerationQuery query;
        query.dictionary_id = dictionary.identity.id;
        query.page_size = static_cast<std::size_t>(
            request.value(QStringLiteral("page_size")).toInteger());
        query.timeout = std::chrono::seconds(30);
        const auto first = service.EnumerateHeadwords(query);
        if (first.error.has_value() || first.headwords.empty())
            throw std::runtime_error(
                "management headword browse first page failed");
        QJsonArray headwords;
        for (const auto& headword : first.headwords)
            headwords.append(Text(headword));
        if (!first.complete) {
            if (first.next_cursor.empty())
                throw std::runtime_error(
                    "management browse omitted its cursor");
            cursors->push_back({dictionary.identity.id, first.next_cursor});
            query.cursor = first.next_cursor;
            const auto second = service.EnumerateHeadwords(query);
            if (second.error.has_value() || second.headwords.empty())
                throw std::runtime_error(
                    "management headword browse second page failed");
            for (const auto& headword : second.headwords)
                headwords.append(Text(headword));
        }
        result.append(QJsonObject{
            {QStringLiteral("dictionary_id"), alias},
            {QStringLiteral("headwords"), headwords},
        });
    }
    return result;
}

void VerifyStaleCursors(
    const std::vector<std::pair<std::string, std::string>>& cursors,
    const goldendict::core::DictionaryService& rescanned) {
    for (const auto& [dictionary_id, cursor] : cursors) {
        goldendict::core::HeadwordEnumerationQuery query;
        query.dictionary_id = dictionary_id;
        query.cursor = cursor;
        query.page_size = 1U;
        const auto page = rescanned.EnumerateHeadwords(query);
        if (!page.error.has_value() ||
            page.error->code !=
                goldendict::core::HeadwordEnumerationErrorCode::kStaleCursor) {
            throw std::runtime_error(
                "source rescan did not invalidate browse cursor");
        }
    }
}

bool WriteAtomically(const QString& path, const QJsonObject& value) {
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly))
        return false;
    const QByteArray content =
        QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
    return output.write(content) == content.size() && output.commit();
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    const auto options = ParseOptions(application.arguments());
    if (!options.has_value()) {
        std::cerr << "usage: qt6_real_management_observer --dictionary-root "
                     "PATH --index-root PATH --configuration PATH "
                     "--conditions-sha256 HASH --catalog PATH --scenario "
                     "SCENARIO --output PATH\n";
        return 2;
    }
    try {
        const QByteArray catalog_content = ReadBounded(options->catalog);
        const QJsonObject catalog = ReadCatalog(catalog_content);
        const QString catalog_hash =
            QString::fromLatin1(QCryptographicHash::hash(
                                    catalog_content, QCryptographicHash::Sha256)
                                    .toHex());
        goldendict::core::CoreConfiguration configuration;
        if (options->scenario == QStringLiteral("clean-discovery")) {
            if (QFileInfo::exists(options->configuration))
                throw std::runtime_error(
                    "clean management profile already exists");
            configuration.dictionary_paths = {Utf8(options->dictionary_root)};
            configuration.index_directory = Utf8(options->index_root);
            configuration.preferences.full_text_search_enabled = false;
            auto discovery =
                goldendict::core::CreateDictionaryService(configuration);
            const auto selected = SelectDictionaries(
                catalog, options->dictionary_root, *discovery);
            ApplyWorkflow(catalog, selected, &configuration);
            QDir().mkpath(QFileInfo(options->configuration).absolutePath());
            goldendict::core::SaveConfiguration(Utf8(options->configuration),
                                                configuration);
        } else {
            if (!QFileInfo::exists(options->configuration))
                throw std::runtime_error("warm management profile is missing");
        }

        configuration =
            goldendict::core::LoadConfiguration(Utf8(options->configuration));
        goldendict::core::application::DesktopFacadeActivationOwner owner;
        auto initial_candidate = owner.PrepareCandidate(configuration);
        const auto initial_prepared =
            owner.PreparedFacadeSnapshot(initial_candidate);
        if (!initial_candidate || !initial_prepared ||
            !owner.Activate(initial_candidate)) {
            throw std::runtime_error(
                "could not activate initial management runtime");
        }
        auto facade = owner.CurrentSnapshot();
        if (!facade || facade != initial_prepared) {
            throw std::runtime_error(
                "initial management runtime was not published");
        }
        auto& service = facade->GetDictionaryService();
        const auto selected =
            SelectDictionaries(catalog, options->dictionary_root, service);
        QJsonArray dictionaries;
        for (const auto& dictionary : selected)
            dictionaries.append(Dictionary(dictionary));
        std::vector<std::pair<std::string, std::string>> cursors;
        const QJsonArray browse = Browse(catalog, selected, service, &cursors);

        const auto previous = facade;
        auto rescan_candidate = owner.PrepareCandidate(configuration);
        const auto rescan_prepared =
            owner.PreparedFacadeSnapshot(rescan_candidate);
        if (!rescan_candidate || !rescan_prepared ||
            rescan_prepared == previous || !owner.Activate(rescan_candidate)) {
            throw std::runtime_error(
                "could not activate rescanned management runtime");
        }
        facade = owner.CurrentSnapshot();
        if (!facade || facade != rescan_prepared || facade == previous) {
            throw std::runtime_error(
                "rescanned management runtime was not published");
        }
        auto& rescanned = facade->GetDictionaryService();
        const auto rescanned_selected =
            SelectDictionaries(catalog, options->dictionary_root, rescanned);
        VerifyStaleCursors(cursors, rescanned);
        QJsonArray rescanned_ids;
        for (const auto& dictionary : rescanned_selected)
            rescanned_ids.append(dictionary.catalog_id);

        const QJsonObject raw{
            {QStringLiteral("browse"), browse},
            {QStringLiteral("catalog_sha256"), catalog_hash},
            {QStringLiteral("conditions_sha256"), options->conditions_sha256},
            {QStringLiteral("dictionaries"), dictionaries},
            {QStringLiteral("groups"), Groups(configuration, selected)},
            {QStringLiteral("rescan_dictionary_ids"), rescanned_ids},
            {QStringLiteral("scenario"), options->scenario},
            {QStringLiteral("schema"), QString::fromLatin1(kRawSchema)},
        };
        if (!WriteAtomically(options->output, raw)) {
            std::cerr << "could not publish raw management observation\n";
            return 3;
        }
    } catch (const std::exception& error) {
        std::cerr << "management observation failed: " << error.what() << '\n';
        return 4;
    }
    return 0;
}
