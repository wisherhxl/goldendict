// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <algorithm>
#include <string>
#include <vector>

#include "../src/application/legacy_suggestion_ranking.h"

namespace goldendict::core::application {
namespace {

std::vector<std::string> RankHeadwords(std::string_view query,
                                       std::vector<std::string> headwords) {
    struct RankedHeadword {
        std::string headword;
        LegacySuggestionRank rank;
    };

    const auto query_forms = foundation::FoldForLegacyPrefixRanking(query);
    std::vector<RankedHeadword> ranked;
    ranked.reserve(headwords.size());
    for (auto& headword : headwords) {
        const auto rank = RankLegacySuggestion(headword, query_forms);
        ranked.push_back({std::move(headword), rank});
    }
    std::stable_sort(
        ranked.begin(), ranked.end(),
        [](const RankedHeadword& left, const RankedHeadword& right) {
            return LegacySuggestionLess(left.rank, left.headword, right.rank,
                                        right.headword);
        });

    headwords.clear();
    for (auto& candidate : ranked) {
        headwords.push_back(std::move(candidate.headword));
    }
    return headwords;
}

}  // namespace

class LegacySuggestionRankingTest : public QObject {
    Q_OBJECT

   private slots:
    void UsesFrozenDiacriticFoldingAcrossRankingCategories();
    void DoesNotApplyCaseMappingsAddedAfterUnicode52();
};

void LegacySuggestionRankingTest::
    UsesFrozenDiacriticFoldingAcrossRankingCategories() {
    const std::string rome_with_stroke = "r\xc3\xb8me";
    const auto ranked = RankHeadwords(
        "rome", {"in romex", rome_with_stroke + "x", "in " + rome_with_stroke,
                 "romex", rome_with_stroke, "in rome", "rome"});

    QCOMPARE(ranked,
             (std::vector<std::string>{"rome", rome_with_stroke, "in rome",
                                       "in " + rome_with_stroke, "romex",
                                       rome_with_stroke + "x", "in romex"}));
}

void LegacySuggestionRankingTest::
    DoesNotApplyCaseMappingsAddedAfterUnicode52() {
    const std::string mtavruli_an("\xe1\xb2\x90", 3);
    const std::string mkhedruli_an("\xe1\x83\x90", 3);
    const auto query = foundation::FoldForLegacyPrefixRanking(mtavruli_an);

    QCOMPARE(RankLegacySuggestion(mtavruli_an, query).category,
             LegacySuggestionCategory::kExact);
    QCOMPARE(RankLegacySuggestion("in " + mtavruli_an, query).category,
             LegacySuggestionCategory::kExactInside);
    QCOMPARE(RankLegacySuggestion(mtavruli_an + "x", query).category,
             LegacySuggestionCategory::kPrefix);
    QCOMPARE(RankLegacySuggestion(mkhedruli_an, query).category,
             LegacySuggestionCategory::kWorst);
    QCOMPARE(RankLegacySuggestion("in " + mkhedruli_an, query).category,
             LegacySuggestionCategory::kWorst);
    QCOMPARE(RankLegacySuggestion(mkhedruli_an + "x", query).category,
             LegacySuggestionCategory::kWorst);
}

}  // namespace goldendict::core::application

using goldendict::core::application::LegacySuggestionRankingTest;

QTEST_APPLESS_MAIN(LegacySuggestionRankingTest)

#include "legacy_suggestion_ranking_test.moc"
