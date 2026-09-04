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
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

namespace {

constexpr char kCatalogSchema[] = "goldendict-mdict-query-resource-catalog-v1";
constexpr char kRawSchema[] = "goldendict-real-mdict-raw-observation-v1";
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
        throw std::runtime_error("cannot read bounded MDict catalog");
    }
    const QByteArray content = file.readAll();
    if (content.size() != file.size()) {
        throw std::runtime_error("cannot read complete MDict catalog");
    }
    return content;
}

QJsonObject ReadCatalog(const QByteArray& content) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        throw std::runtime_error("MDict catalog is not valid JSON");
    }
    const QJsonObject catalog = document.object();
    if (catalog.value(QStringLiteral("schema")).toString() !=
        QString::fromLatin1(kCatalogSchema)) {
        throw std::runtime_error("MDict catalog schema is invalid");
    }
    return catalog;
}

QString CanonicalPath(const QString& path) {
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty() || !info.isFile()) {
        throw std::runtime_error("MDict component does not exist");
    }
    return QDir::cleanPath(canonical);
}

QJsonArray BoundComponents(const QString& root, const QJsonObject& catalog) {
    const QJsonObject dictionary =
        catalog.value(QStringLiteral("dictionary")).toObject();
    const QJsonArray relative_components =
        dictionary.value(QStringLiteral("ordered_components")).toArray();
    if (relative_components.size() != 4) {
        throw std::runtime_error("MDict catalog must contain four components");
    }
    const QString canonical_root = QDir(root).canonicalPath();
    if (canonical_root.isEmpty()) {
        throw std::runtime_error("dictionary root does not exist");
    }
    QJsonArray components;
    for (const QJsonValue& value : relative_components) {
        const QString relative = value.toString();
        if (relative.isEmpty() || QDir::isAbsolutePath(relative) ||
            relative.split(QLatin1Char('/')).contains(QStringLiteral(".."))) {
            throw std::runtime_error("MDict component path is unsafe");
        }
        const QString component =
            CanonicalPath(QDir(canonical_root).filePath(relative));
        const QString prefix = canonical_root + QLatin1Char('/');
        if (!component.startsWith(prefix, Qt::CaseInsensitive)) {
            throw std::runtime_error("MDict component escapes dictionary root");
        }
        components.append(component);
    }
    return components;
}

const goldendict::core::DictionaryIdentity& FindDictionary(
    const std::vector<goldendict::core::DictionaryIdentity>& identities,
    const QJsonArray& components) {
    const QString primary = CanonicalPath(components.at(0).toString());
    const auto found = std::find_if(
        identities.begin(), identities.end(), [&primary](const auto& identity) {
            return CanonicalPath(Text(identity.source))
                       .compare(primary, Qt::CaseInsensitive) == 0;
        });
    if (found == identities.end()) {
        throw std::runtime_error(
            "cataloged MDict dictionary was not discovered");
    }
    return *found;
}

QString ResourceMediaType(const goldendict::core::DictionaryEntry& entry,
                          const std::string& resource_id) {
    const auto found =
        std::find_if(entry.resources.begin(), entry.resources.end(),
                     [&resource_id](const auto& item) {
                         return item.resource_id == resource_id;
                     });
    if (found != entry.resources.end() && !found->media_type.empty()) {
        return Text(found->media_type);
    }
    return QMimeDatabase()
        .mimeTypeForFile(Text(resource_id), QMimeDatabase::MatchExtension)
        .name();
}

QJsonObject ObserveProbe(const QJsonObject& probe,
                         const goldendict::core::DictionaryIdentity& identity,
                         const goldendict::core::DictionaryService& service) {
    const QString id = probe.value(QStringLiteral("id")).toString();
    const QJsonObject catalog_query =
        probe.value(QStringLiteral("query")).toObject();
    const QString query_text =
        catalog_query.value(QStringLiteral("text")).toString();
    const QJsonObject catalog_resource =
        probe.value(QStringLiteral("resource")).toObject();
    const QString resource_id =
        catalog_resource.value(QStringLiteral("id")).toString();
    if (id.isEmpty() || query_text.isEmpty() || resource_id.isEmpty() ||
        catalog_query.value(QStringLiteral("match_mode")).toString() !=
            QStringLiteral("exact")) {
        throw std::runtime_error("MDict catalog probe is invalid");
    }

    goldendict::core::LookupQuery query;
    query.text = Utf8(query_text);
    query.dictionary_ids = {identity.id};
    query.dictionary_filter_active = true;
    query.match_mode = goldendict::core::MatchMode::kExact;
    query.result_limit = 4U;
    query.timeout = std::chrono::seconds(30);
    const auto response = service.Lookup(query);
    if (!response.errors.empty() || response.partial ||
        response.entries.size() != 1U) {
        throw std::runtime_error("MDict exact lookup did not return one entry");
    }
    const auto& entry = response.entries.front();
    if (entry.dictionary.id != identity.id ||
        entry.match.mode != goldendict::core::MatchMode::kExact ||
        !entry.article.sanitized_html.has_value()) {
        throw std::runtime_error("MDict exact lookup metadata is incomplete");
    }
    goldendict::core::ResourceReference resource_reference;
    resource_reference.dictionary_id = identity.id;
    resource_reference.resource_id = Utf8(resource_id);
    const auto resource = service.GetResource(resource_reference);
    if (resource.empty()) {
        throw std::runtime_error("MDict resource lookup returned no bytes");
    }
    QByteArray resource_bytes;
    resource_bytes.resize(static_cast<int>(resource.size()));
    std::transform(
        resource.begin(), resource.end(), resource_bytes.begin(),
        [](std::byte value) {
            return static_cast<char>(std::to_integer<unsigned char>(value));
        });
    return {
        {QStringLiteral("article_markup"), Text(*entry.article.sanitized_html)},
        {QStringLiteral("headword"), query_text},
        {QStringLiteral("id"), id},
        {QStringLiteral("query"), query_text},
        {QStringLiteral("resource_data_base64"),
         QString::fromLatin1(resource_bytes.toBase64())},
        {QStringLiteral("resource_id"), resource_id},
        {QStringLiteral("resource_media_type"),
         ResourceMediaType(entry, resource_reference.resource_id)},
    };
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
        std::cerr << "usage: qt6_real_mdict_observer --dictionary-root PATH "
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
        const QJsonArray components =
            BoundComponents(options->dictionary_root, catalog);

        goldendict::core::CoreConfiguration configuration;
        configuration.dictionary_paths = {Utf8(options->dictionary_root)};
        configuration.index_directory = Utf8(options->index_root);
        auto service = goldendict::core::CreateDictionaryService(configuration);
        const auto identities = service->GetCatalog();
        const auto& identity = FindDictionary(identities, components);

        QJsonArray probes;
        const QJsonArray catalog_probes =
            catalog.value(QStringLiteral("probes")).toArray();
        if (catalog_probes.size() != 3) {
            throw std::runtime_error("MDict catalog must contain three probes");
        }
        for (const QJsonValue& value : catalog_probes) {
            probes.append(ObserveProbe(value.toObject(), identity, *service));
        }
        QJsonObject dictionary;
        dictionary.insert(QStringLiteral("components"), components);
        dictionary.insert(QStringLiteral("id"), Text(identity.id));
        dictionary.insert(QStringLiteral("name"), Text(identity.name));
        const QJsonObject raw{
            {QStringLiteral("catalog_sha256"), catalog_hash},
            {QStringLiteral("conditions_sha256"), options->conditions_sha256},
            {QStringLiteral("dictionary"), dictionary},
            {QStringLiteral("errors"), QJsonArray()},
            {QStringLiteral("probes"), probes},
            {QStringLiteral("scenario"), options->scenario},
            {QStringLiteral("schema"), QString::fromLatin1(kRawSchema)},
        };
        if (!WriteAtomically(options->output, raw)) {
            std::cerr << "could not publish raw MDict observation\n";
            return 3;
        }
    } catch (const std::exception& error) {
        std::cerr << "MDict observation failed: " << error.what() << '\n';
        return 4;
    }
    return 0;
}
