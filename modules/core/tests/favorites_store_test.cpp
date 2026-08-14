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
    void RejectsOversizedAndInvalidUtf8LegacyFavorites();
    void ImportsAndExportsCompatibilityXml();
    void MovesItemsAcrossArbitraryFolders();
    void ReordersUsingPreMoveInsertionBoundaries();
    void RejectsInvalidMovesWithoutWriting();
    void PersistenceFailureDoesNotReplaceDestination();
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
    QVERIFY(!std::filesystem::exists(current.string() + ".tmp"));
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

void FavoritesStoreTest::RejectsOversizedAndInvalidUtf8LegacyFavorites() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto current = Path(directory, "favorites-v1");
    const auto legacy = Path(directory, "favorites");
    const auto reject = [&](std::string_view contents) {
        Write(legacy, contents);
        QVERIFY_EXCEPTION_THROWN(
            LoadOrMigrateFavorites(current.string(), legacy.string()),
            std::runtime_error);
        QVERIFY(!std::filesystem::exists(current));
        QVERIFY(!std::filesystem::exists(current.string() + ".tmp"));
    };

    reject(std::string("<root><headword>invalid\xFF</headword></root>", 42U));
    reject(std::string(1024U * 1024U + 1U, 'x'));
}

void FavoritesStoreTest::ImportsAndExportsCompatibilityXml() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "favorites.xml");
    const Favorites expected = {
        {FavoriteItemKind::kFolder,
         "A & \"B\"",
         true,
         {{FavoriteItemKind::kHeadword, "<café>", false, {}}}},
        {FavoriteItemKind::kHeadword, "rock'n", false, {}}};

    ExportFavoritesXml(path.string(), expected);

    QCOMPARE(ImportFavoritesXml(path.string()), expected);
    QCOMPARE(Read(path),
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?><root>"
             "<folder name=\"A &amp; &quot;B&quot;\" expanded=\"1\">"
             "<headword>&lt;café&gt;</headword></folder>"
             "<headword>rock&apos;n</headword></root>\n");

    Write(path, "sentinel");
    const Favorites invalid = {{FavoriteItemKind::kHeadword, {}, false, {}}};
    QVERIFY_EXCEPTION_THROWN(ExportFavoritesXml(path.string(), invalid),
                             std::runtime_error);
    QCOMPARE(Read(path), "sentinel");
}

void FavoritesStoreTest::MovesItemsAcrossArbitraryFolders() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "favorites-v1");
    const Favorites original = {
        {FavoriteItemKind::kFolder,
         "Source",
         true,
         {{FavoriteItemKind::kHeadword, "first", false, {}},
          {FavoriteItemKind::kFolder,
           "Subtree",
           true,
           {{FavoriteItemKind::kHeadword, "nested", false, {}}}}}},
        {FavoriteItemKind::kFolder,
         "Destination",
         false,
         {{FavoriteItemKind::kHeadword, "existing", false, {}}}},
        {FavoriteItemKind::kHeadword, "root", false, {}}};
    SaveFavorites(path.string(), original);

    auto moved = MoveFavorite(path.string(), original, {0U, 0U}, {1U}, 0U);
    QCOMPARE(moved.status, FavoriteMoveStatus::kMoved);
    QCOMPARE(moved.moved_path, (FavoritePath{1U, 0U}));
    QCOMPARE(moved.favorites[1].children[0].text, "first");
    QCOMPARE(moved.favorites[1].children[1].text, "existing");
    QCOMPARE(moved.favorites[0].expanded, true);
    QCOMPARE(moved.favorites[1].expanded, false);
    QCOMPARE(LoadFavorites(path.string()), moved.favorites);

    moved = MoveFavorite(path.string(), moved.favorites, {0U, 0U}, {}, 3U);
    QCOMPARE(moved.status, FavoriteMoveStatus::kMoved);
    QCOMPARE(moved.moved_path, (FavoritePath{3U}));
    QCOMPARE(moved.favorites[3].text, "Subtree");
    QCOMPARE(moved.favorites[3].children[0].text, "nested");
    QCOMPARE(moved.favorites[3].expanded, true);
    QCOMPARE(LoadFavorites(path.string()), moved.favorites);
}

void FavoritesStoreTest::ReordersUsingPreMoveInsertionBoundaries() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "favorites-v1");
    const Favorites original = {{FavoriteItemKind::kHeadword, "a", false, {}},
                                {FavoriteItemKind::kHeadword, "b", false, {}},
                                {FavoriteItemKind::kHeadword, "c", false, {}}};
    SaveFavorites(path.string(), original);

    auto moved = MoveFavorite(path.string(), original, {0U}, {}, 3U);
    QCOMPARE(moved.status, FavoriteMoveStatus::kMoved);
    QCOMPARE(moved.moved_path, (FavoritePath{2U}));
    QCOMPARE(moved.favorites[0].text, "b");
    QCOMPARE(moved.favorites[1].text, "c");
    QCOMPARE(moved.favorites[2].text, "a");

    moved = MoveFavorite(path.string(), moved.favorites, {2U}, {}, 0U);
    QCOMPARE(moved.status, FavoriteMoveStatus::kMoved);
    QCOMPARE(moved.favorites, original);

    const auto before = Read(path);
    const auto no_op = MoveFavorite(path.string(), original, {1U}, {}, 2U);
    QCOMPARE(no_op.status, FavoriteMoveStatus::kNoOp);
    QCOMPARE(no_op.moved_path, (FavoritePath{1U}));
    QCOMPARE(Read(path), before);
}

void FavoritesStoreTest::RejectsInvalidMovesWithoutWriting() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = Path(directory, "favorites-v1");
    const Favorites original = {
        {FavoriteItemKind::kFolder,
         "Folder",
         true,
         {{FavoriteItemKind::kFolder, "Child", false, {}},
          {FavoriteItemKind::kHeadword, "duplicate", false, {}}}},
        {FavoriteItemKind::kFolder,
         "Target",
         false,
         {{FavoriteItemKind::kHeadword, "duplicate", false, {}}}},
        {FavoriteItemKind::kHeadword, "word", false, {}}};
    SaveFavorites(path.string(), original);
    const auto before = Read(path);
    const auto reject = [&](const FavoritePath& source,
                            const FavoritePath& destination, std::size_t index,
                            FavoriteMoveStatus expected) {
        const auto result =
            MoveFavorite(path.string(), original, source, destination, index);
        QCOMPARE(result.status, expected);
        QCOMPARE(result.favorites, original);
        QCOMPARE(Read(path), before);
    };

    reject({}, {}, 0U, FavoriteMoveStatus::kInvalidSource);
    reject({9U}, {}, 0U, FavoriteMoveStatus::kInvalidSource);
    reject({2U}, {9U}, 0U, FavoriteMoveStatus::kInvalidDestination);
    reject({0U}, {2U}, 0U, FavoriteMoveStatus::kInvalidDestination);
    reject({2U}, {}, 4U, FavoriteMoveStatus::kInvalidDestination);
    reject({0U}, {0U}, 0U, FavoriteMoveStatus::kCycle);
    reject({0U}, {0U, 0U}, 0U, FavoriteMoveStatus::kCycle);
    reject({0U, 1U}, {1U}, 1U, FavoriteMoveStatus::kDuplicate);
}

void FavoritesStoreTest::PersistenceFailureDoesNotReplaceDestination() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const Favorites original = {
        {FavoriteItemKind::kHeadword, "first", false, {}},
        {FavoriteItemKind::kHeadword, "second", false, {}}};

    QVERIFY_EXCEPTION_THROWN(
        MoveFavorite(directory.path().toStdString(), original, {0U}, {}, 2U),
        std::runtime_error);
    QVERIFY(std::filesystem::is_directory(directory.path().toStdString()));
}

}  // namespace
}  // namespace goldendict::core

using goldendict::core::FavoritesStoreTest;

QTEST_APPLESS_MAIN(FavoritesStoreTest)

#include "favorites_store_test.moc"
