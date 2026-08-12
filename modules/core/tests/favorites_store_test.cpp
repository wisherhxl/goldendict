// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "goldendict/core/favorites_store.h"

namespace goldendict::core {
namespace {

class FavoritesStoreTest : public QObject {
    Q_OBJECT

   private slots:
    void RoundTripsCurrentFavorites();
    void MigratesLegacyTreeWithoutChangingIt();
    void CurrentFavoritesTakePrecedence();
    void RejectsMalformedLegacyWithoutPartialMigration();
    void RejectsLegacyEntityDeclarations();
};

std::filesystem::path Path(const QTemporaryDir& directory, const char* name) {
    return std::filesystem::path(directory.path().toStdString()) / name;
}

void Write(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    output.close();
    QVERIFY(output.good());
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

Favorites Fixture() {
    return {{FavoriteItemKind::kFolder,
             "Languages",
             true,
             {{FavoriteItemKind::kHeadword, "café", false, {}},
              {FavoriteItemKind::kFolder,
               "日本語",
               false,
               {{FavoriteItemKind::kHeadword, "辞書", false, {}}}}}},
            {FavoriteItemKind::kHeadword, "root word", false, {}}};
}

void FavoritesStoreTest::RoundTripsCurrentFavorites() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "favorites-v1");
    const Favorites expected = Fixture();

    SaveFavorites(path.string(), expected);

    QCOMPARE(LoadFavorites(path.string()), expected);
}

void FavoritesStoreTest::MigratesLegacyTreeWithoutChangingIt() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "favorites-v1");
    const auto legacy = Path(directory, "favorites");
    const std::string xml =
        "<?xml version=\"1.0\"?><root><folder name=\"Languages\" "
        "expanded=\"1\"><headword>café</headword><folder name=\"日本語\" "
        "expanded=\"0\"><headword>辞書</headword></folder></folder>"
        "<headword>root word</headword></root>";
    Write(legacy, xml);

    const Favorites migrated =
        LoadOrMigrateFavorites(current.string(), legacy.string());

    QCOMPARE(migrated, Fixture());
    QCOMPARE(Read(legacy), xml);
    QCOMPARE(LoadFavorites(current.string()), migrated);
}

void FavoritesStoreTest::CurrentFavoritesTakePrecedence() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "favorites-v1");
    const auto legacy = Path(directory, "favorites");
    const Favorites expected = Fixture();
    SaveFavorites(current.string(), expected);
    Write(legacy, "malformed");

    QCOMPARE(LoadOrMigrateFavorites(current.string(), legacy.string()),
             expected);
}

void FavoritesStoreTest::RejectsMalformedLegacyWithoutPartialMigration() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "favorites-v1");
    const auto legacy = Path(directory, "favorites");
    Write(legacy, "<root><folder name=\"open\"></root>");

    QVERIFY_EXCEPTION_THROWN(
        LoadOrMigrateFavorites(current.string(), legacy.string()),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(current));
    QVERIFY(std::filesystem::exists(legacy));
}

void FavoritesStoreTest::RejectsLegacyEntityDeclarations() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "favorites-v1");
    const auto legacy = Path(directory, "favorites");
    Write(legacy,
          "<!DOCTYPE root [<!ENTITY word \"secret\">]><root>"
          "<headword>&word;</headword></root>");

    QVERIFY_EXCEPTION_THROWN(
        LoadOrMigrateFavorites(current.string(), legacy.string()),
        std::runtime_error);
    QVERIFY(!std::filesystem::exists(current));
}

}  // namespace
}  // namespace goldendict::core

using goldendict::core::FavoritesStoreTest;

QTEST_APPLESS_MAIN(FavoritesStoreTest)

#include "favorites_store_test.moc"
