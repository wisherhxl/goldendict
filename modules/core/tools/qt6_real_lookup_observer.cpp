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
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

namespace {

constexpr char kCatalogSchema[] = "goldendict-real-lookup-catalog-v1";
constexpr char kRawSchema[] = "goldendict-real-lookup-raw-observation-v1";
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
        throw std::runtime_error("cannot read bounded lookup catalog");
    }
    const QByteArray content = file.readAll();
    if (content.size() != file.size())
        throw std::runtime_error("cannot read complete lookup catalog");
    return content;
}

QJsonObject ReadCatalog(const QByteArray& content) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error("lookup catalog is not valid JSON");
    const QJsonObject catalog = document.object();
    if (catalog.value(QStringLiteral("schema")).toString() !=
        QString::fromLatin1(kCatalogSchema)) {
        throw std::runtime_error("lookup catalog schema is invalid");
    }
    return catalog;
}

QString CanonicalFile(const QString& root, const QString& relative) {
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) ||
        relative.split(QLatin1Char('/')).contains(QStringLiteral(".."))) {
        throw std::runtime_error("lookup catalog component path is unsafe");
    }
    const QString canonical_root = QDir(root).canonicalPath();
    const QString path =
        QFileInfo(QDir(canonical_root).filePath(relative)).canonicalFilePath();
    if (canonical_root.isEmpty() || path.isEmpty() ||
        !QFileInfo(path).isFile() ||
        !path.startsWith(canonical_root + QLatin1Char('/'),
                         Qt::CaseInsensitive)) {
        throw std::runtime_error("lookup catalog component is outside corpus");
    }
    return QDir::cleanPath(path);
}

const goldendict::core::DictionaryIdentity& FindDictionary(
    const std::vector<goldendict::core::DictionaryIdentity>& identities,
    const QString& primary) {
    const auto found = std::find_if(
        identities.begin(), identities.end(), [&primary](const auto& value) {
            return QFileInfo(Text(value.source))
                       .canonicalFilePath()
                       .compare(primary, Qt::CaseInsensitive) == 0;
        });
    if (found == identities.end())
        throw std::runtime_error("cataloged dictionary was not discovered");
    return *found;
}

QJsonArray Errors(const std::vector<goldendict::core::LookupError>& errors) {
    QJsonArray result;
    for (const auto& error : errors) {
        result.append(QJsonObject{
            {QStringLiteral("dictionary_id"), Text(error.dictionary_id)},
            {QStringLiteral("message"), Text(error.message)},
        });
    }
    return result;
}

QJsonObject ObserveLookup(
    const QJsonObject& probe,
    const goldendict::core::DictionaryIdentity& identity,
    const goldendict::core::DictionaryService& service) {
    goldendict::core::LookupQuery query;
    query.text = Utf8(probe.value(QStringLiteral("query")).toString());
    query.dictionary_ids = {identity.id};
    query.dictionary_filter_active = true;
    query.match_mode = goldendict::core::MatchMode::kExact;
    query.result_limit = static_cast<std::size_t>(
        probe.value(QStringLiteral("result_limit")).toInteger());
    query.timeout = std::chrono::seconds(30);
    const auto response = service.Lookup(query);
    QJsonArray entries;
    for (const auto& entry : response.entries) {
        QJsonArray resources;
        for (const auto& resource : entry.resources) {
            resources.append(QJsonObject{
                {QStringLiteral("id"), Text(resource.resource_id)},
                {QStringLiteral("media_type"), Text(resource.media_type)},
            });
        }
        entries.append(QJsonObject{
            {QStringLiteral("article_markup"),
             entry.article.sanitized_html.has_value()
                 ? Text(*entry.article.sanitized_html)
                 : QString()},
            {QStringLiteral("headword"), Text(entry.headword)},
            {QStringLiteral("plain_text"), Text(entry.article.plain_text)},
            {QStringLiteral("resources"), resources},
        });
    }
    return QJsonObject{
        {QStringLiteral("entries"), entries},
        {QStringLiteral("errors"), Errors(response.errors)},
        {QStringLiteral("id"), probe.value(QStringLiteral("id"))},
        {QStringLiteral("operation"), QStringLiteral("lookup")},
        {QStringLiteral("suggestions"), QJsonArray()},
    };
}

QJsonObject ObserveSuggestions(
    const QJsonObject& probe,
    const goldendict::core::DictionaryIdentity& identity,
    const goldendict::core::DictionaryService& service) {
    goldendict::core::SuggestionQuery query;
    query.text = Utf8(probe.value(QStringLiteral("query")).toString());
    query.dictionary_ids = {identity.id};
    query.dictionary_filter_active = true;
    query.result_limit = static_cast<std::size_t>(
        probe.value(QStringLiteral("result_limit")).toInteger());
    query.timeout = std::chrono::seconds(30);
    const auto response = service.Suggest(query);
    QJsonArray suggestions;
    for (const auto& suggestion : response.suggestions)
        suggestions.append(Text(suggestion.headword));
    return QJsonObject{
        {QStringLiteral("entries"), QJsonArray()},
        {QStringLiteral("errors"), Errors(response.errors)},
        {QStringLiteral("id"), probe.value(QStringLiteral("id"))},
        {QStringLiteral("operation"), QStringLiteral("suggest")},
        {QStringLiteral("suggestions"), suggestions},
    };
}

QJsonObject ObserveDictionary(
    const QJsonObject& item,
    const goldendict::core::DictionaryIdentity& identity,
    const goldendict::core::DictionaryService& service,
    const QString& primary) {
    QJsonArray probes;
    for (const QJsonValue& value :
         item.value(QStringLiteral("probes")).toArray()) {
        const QJsonObject probe = value.toObject();
        const QString operation =
            probe.value(QStringLiteral("operation")).toString();
        if (operation == QStringLiteral("lookup"))
            probes.append(ObserveLookup(probe, identity, service));
        else if (operation == QStringLiteral("suggest"))
            probes.append(ObserveSuggestions(probe, identity, service));
        else
            throw std::runtime_error("lookup catalog operation is invalid");
    }
    return QJsonObject{
        {QStringLiteral("catalog_id"),
         item.value(QStringLiteral("id")).toString()},
        {QStringLiteral("components"), QJsonArray{primary}},
        {QStringLiteral("id"), Text(identity.id)},
        {QStringLiteral("name"), Text(identity.name)},
        {QStringLiteral("probes"), probes},
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
        std::cerr << "usage: qt6_real_lookup_observer --dictionary-root PATH "
                     "--index-root PATH --conditions-sha256 HASH "
                     "--catalog PATH --scenario SCENARIO --output PATH\n";
        return 2;
    }
    try {
        const QByteArray catalog_content = ReadBounded(options->catalog);
        const QJsonObject catalog = ReadCatalog(catalog_content);
        const QString catalog_hash = QString::fromLatin1(
            QCryptographicHash::hash(catalog_content,
                                     QCryptographicHash::Sha256)
                .toHex());
        goldendict::core::CoreConfiguration configuration;
        configuration.dictionary_paths = {Utf8(options->dictionary_root)};
        configuration.index_directory = Utf8(options->index_root);
        auto service = goldendict::core::CreateDictionaryService(configuration);
        const auto identities = service->GetCatalog();
        QJsonArray dictionaries;
        for (const QJsonValue& value :
             catalog.value(QStringLiteral("dictionaries")).toArray()) {
            const QJsonObject item = value.toObject();
            const QString primary = CanonicalFile(
                options->dictionary_root,
                item.value(QStringLiteral("primary_component")).toString());
            dictionaries.append(ObserveDictionary(
                item, FindDictionary(identities, primary), *service, primary));
        }
        const QJsonObject raw{
            {QStringLiteral("catalog_sha256"), catalog_hash},
            {QStringLiteral("conditions_sha256"),
             options->conditions_sha256},
            {QStringLiteral("dictionaries"), dictionaries},
            {QStringLiteral("errors"), QJsonArray()},
            {QStringLiteral("scenario"), options->scenario},
            {QStringLiteral("schema"), QString::fromLatin1(kRawSchema)},
        };
        if (!WriteAtomically(options->output, raw)) {
            std::cerr << "could not publish raw lookup observation\n";
            return 3;
        }
    } catch (const std::exception& error) {
        std::cerr << "lookup observation failed: " << error.what() << '\n';
        return 4;
    }
    return 0;
}
