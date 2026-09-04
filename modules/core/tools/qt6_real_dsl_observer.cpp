// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

namespace {

constexpr char kCatalogSchema[] = "goldendict-dsl-query-resource-catalog-v1";
constexpr char kRawSchema[] = "goldendict-real-dsl-raw-observation-v1";
constexpr qint64 kMaximumCatalogBytes = 1024 * 1024;

struct Options {
    QString dictionary_root;
    QString index_root;
    QString conditions_sha256;
    QString catalog;
    QString scenario;
    QString output;
};

std::optional<Options> ParseOptions(const QStringList& arguments) {
    if (arguments.size() != 13)
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
    if (seen.size() != 6 ||
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
        throw std::runtime_error("cannot read bounded DSL catalog");
    }
    const QByteArray content = file.readAll();
    if (content.size() != file.size())
        throw std::runtime_error("cannot read complete DSL catalog");
    return content;
}

QJsonObject ReadCatalog(const QByteArray& content) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error("DSL catalog is not valid JSON");
    const QJsonObject catalog = document.object();
    if (catalog.value(QStringLiteral("schema")).toString() !=
        QString::fromLatin1(kCatalogSchema)) {
        throw std::runtime_error("DSL catalog schema is invalid");
    }
    return catalog;
}

QString CanonicalFile(const QString& root, const QString& relative) {
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) ||
        relative.split(QLatin1Char('/')).contains(QStringLiteral(".."))) {
        throw std::runtime_error("DSL component path is unsafe");
    }
    const QString canonical_root = QDir(root).canonicalPath();
    const QString canonical =
        QFileInfo(QDir(canonical_root).filePath(relative)).canonicalFilePath();
    if (canonical_root.isEmpty() || canonical.isEmpty() ||
        !canonical.startsWith(canonical_root + QLatin1Char('/'),
                              Qt::CaseInsensitive)) {
        throw std::runtime_error("DSL component escapes dictionary root");
    }
    return QDir::cleanPath(canonical);
}

const goldendict::core::DictionaryIdentity& FindDictionary(
    const std::vector<goldendict::core::DictionaryIdentity>& identities,
    const QString& primary) {
    const auto found = std::find_if(
        identities.begin(), identities.end(), [&primary](const auto& identity) {
            return QFileInfo(Text(identity.source))
                       .canonicalFilePath()
                       .compare(primary, Qt::CaseInsensitive) == 0;
        });
    if (found == identities.end())
        throw std::runtime_error("cataloged DSL dictionary was not discovered");
    return *found;
}

QString ResourceMediaType(const goldendict::core::DictionaryEntry& entry,
                          const std::string& resource_id) {
    const auto found =
        std::find_if(entry.resources.begin(), entry.resources.end(),
                     [&resource_id](const auto& item) {
                         return item.resource_id == resource_id;
                     });
    if (found != entry.resources.end() && !found->media_type.empty())
        return Text(found->media_type);
    return QMimeDatabase()
        .mimeTypeForFile(Text(resource_id), QMimeDatabase::MatchExtension)
        .name();
}

QJsonObject ObserveDictionary(
    const QJsonObject& catalog_entry,
    const goldendict::core::DictionaryIdentity& identity,
    const goldendict::core::DictionaryService& service, const QString& primary,
    const QString& archive) {
    const QJsonObject probe =
        catalog_entry.value(QStringLiteral("probe")).toObject();
    const QJsonObject catalog_query =
        probe.value(QStringLiteral("query")).toObject();
    const QJsonObject catalog_resource =
        probe.value(QStringLiteral("resource")).toObject();
    const QString probe_id = probe.value(QStringLiteral("id")).toString();
    const QString query_text =
        catalog_query.value(QStringLiteral("text")).toString();
    const QString resource_id =
        catalog_resource.value(QStringLiteral("id")).toString();
    if (probe_id.isEmpty() || query_text.isEmpty() || resource_id.isEmpty() ||
        catalog_query.value(QStringLiteral("match_mode")).toString() !=
            QStringLiteral("exact")) {
        throw std::runtime_error("DSL catalog probe is invalid");
    }

    goldendict::core::LookupQuery query;
    query.text = Utf8(query_text);
    query.dictionary_ids = {identity.id};
    query.dictionary_filter_active = true;
    query.match_mode = goldendict::core::MatchMode::kExact;
    query.result_limit = 4U;
    query.timeout = std::chrono::seconds(90);
    const auto response = service.Lookup(query);
    if (!response.errors.empty() || response.partial ||
        response.entries.size() != 1U) {
        throw std::runtime_error("DSL exact lookup did not return one entry");
    }
    const auto& entry = response.entries.front();
    if (entry.dictionary.id != identity.id ||
        entry.match.mode != goldendict::core::MatchMode::kExact ||
        !entry.article.sanitized_html.has_value()) {
        throw std::runtime_error("DSL exact lookup metadata is incomplete");
    }

    goldendict::core::ResourceReference reference;
    reference.dictionary_id = identity.id;
    reference.resource_id = Utf8(resource_id);
    const auto bytes = service.GetResource(reference);
    QByteArray resource_bytes;
    resource_bytes.resize(static_cast<int>(bytes.size()));
    std::transform(
        bytes.begin(), bytes.end(), resource_bytes.begin(),
        [](std::byte value) {
            return static_cast<char>(std::to_integer<unsigned char>(value));
        });

    const bool available = !bytes.empty();
    QJsonObject raw_probe;
    raw_probe.insert(QStringLiteral("article_markup"),
                     Text(*entry.article.sanitized_html));
    raw_probe.insert(QStringLiteral("headword"), query_text);
    raw_probe.insert(QStringLiteral("id"), probe_id);
    raw_probe.insert(QStringLiteral("query"), query_text);
    raw_probe.insert(QStringLiteral("resource_available"), available);
    raw_probe.insert(
        QStringLiteral("resource_data_base64"),
        available ? QString::fromLatin1(resource_bytes.toBase64()) : QString());
    raw_probe.insert(QStringLiteral("resource_id"), resource_id);
    raw_probe.insert(QStringLiteral("resource_media_type"),
                     ResourceMediaType(entry, reference.resource_id));

    QJsonArray components;
    components.append(primary);
    if (available)
        components.append(archive);
    QJsonObject result;
    result.insert(QStringLiteral("archive_owned"), available);
    result.insert(QStringLiteral("components"), components);
    result.insert(QStringLiteral("id"), Text(identity.id));
    result.insert(QStringLiteral("name"), Text(identity.name));
    result.insert(QStringLiteral("probe"), raw_probe);
    return result;
}

bool ObserveOrphanArchiveOwnership(
    const QJsonObject& catalog,
    const std::vector<goldendict::core::DictionaryIdentity>& identities,
    const goldendict::core::DictionaryService& service,
    const QString& dictionary_root) {
    const QJsonArray orphans =
        catalog.value(QStringLiteral("orphan_archives")).toArray();
    if (orphans.size() != 1)
        throw std::runtime_error("DSL catalog must contain one orphan archive");
    const QJsonObject orphan = orphans.at(0).toObject();
    const QString orphan_path = CanonicalFile(
        dictionary_root, orphan.value(QStringLiteral("component")).toString());
    const QJsonObject resource =
        orphan.value(QStringLiteral("resource")).toObject();
    const QString resource_id = resource.value(QStringLiteral("id")).toString();
    const QString expected_hash =
        resource.value(QStringLiteral("sha256")).toString();
    const qint64 expected_size =
        resource.value(QStringLiteral("size")).toInteger(-1);
    if (resource_id.isEmpty() || expected_hash.size() != 64 ||
        expected_size < 1) {
        throw std::runtime_error("DSL orphan resource contract is invalid");
    }
    for (const auto& identity : identities) {
        if (QFileInfo(Text(identity.source))
                .canonicalFilePath()
                .compare(orphan_path, Qt::CaseInsensitive) == 0) {
            return true;
        }
        goldendict::core::ResourceReference reference;
        reference.dictionary_id = identity.id;
        reference.resource_id = Utf8(resource_id);
        const auto bytes = service.GetResource(reference);
        if (bytes.empty())
            continue;
        QByteArray content;
        content.resize(static_cast<int>(bytes.size()));
        std::transform(
            bytes.begin(), bytes.end(), content.begin(), [](std::byte value) {
                return static_cast<char>(std::to_integer<unsigned char>(value));
            });
        const QString actual_hash = QString::fromLatin1(
            QCryptographicHash::hash(content, QCryptographicHash::Sha256)
                .toHex());
        if (content.size() != expected_size || actual_hash != expected_hash) {
            throw std::runtime_error(
                "DSL orphan resource id collides with owned data");
        }
        return true;
    }
    return false;
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
        std::cerr << "usage: qt6_real_dsl_observer --dictionary-root PATH "
                     "--index-root PATH --conditions-sha256 HASH "
                     "--catalog PATH --scenario SCENARIO --output PATH\n";
        return 2;
    }
    try {
        const QByteArray catalog_content = ReadBounded(options->catalog);
        const QJsonObject catalog = ReadCatalog(catalog_content);
        const QString catalog_hash =
            QString::fromLatin1(QCryptographicHash::hash(
                                    catalog_content, QCryptographicHash::Sha256)
                                    .toHex());
        const QJsonArray catalog_dictionaries =
            catalog.value(QStringLiteral("dictionaries")).toArray();
        if (catalog_dictionaries.size() != 5)
            throw std::runtime_error(
                "DSL catalog must contain five dictionaries");

        goldendict::core::CoreConfiguration configuration;
        configuration.dictionary_paths = {Utf8(options->dictionary_root)};
        configuration.index_directory = Utf8(options->index_root);
        auto service = goldendict::core::CreateDictionaryService(configuration);
        const auto identities = service->GetCatalog();
        QJsonArray dictionaries;
        for (const QJsonValue& value : catalog_dictionaries) {
            const QJsonObject entry = value.toObject();
            const QString primary = CanonicalFile(
                options->dictionary_root,
                entry.value(QStringLiteral("primary_component")).toString());
            const QString archive =
                CanonicalFile(options->dictionary_root,
                              entry.value(QStringLiteral("archive"))
                                  .toObject()
                                  .value(QStringLiteral("component"))
                                  .toString());
            const auto& identity = FindDictionary(identities, primary);
            dictionaries.append(
                ObserveDictionary(entry, identity, *service, primary, archive));
        }

        const bool orphan_archive_owned = ObserveOrphanArchiveOwnership(
            catalog, identities, *service, options->dictionary_root);
        const QJsonObject raw{
            {QStringLiteral("catalog_sha256"), catalog_hash},
            {QStringLiteral("conditions_sha256"), options->conditions_sha256},
            {QStringLiteral("dictionaries"), dictionaries},
            {QStringLiteral("errors"), QJsonArray()},
            {QStringLiteral("orphan_archive_owned"), orphan_archive_owned},
            {QStringLiteral("scenario"), options->scenario},
            {QStringLiteral("schema"), QString::fromLatin1(kRawSchema)},
        };
        if (!WriteAtomically(options->output, raw)) {
            std::cerr << "could not publish raw DSL observation\n";
            return 3;
        }
    } catch (const std::exception& error) {
        std::cerr << "DSL observation failed: " << error.what() << '\n';
        return 4;
    }
    return 0;
}
