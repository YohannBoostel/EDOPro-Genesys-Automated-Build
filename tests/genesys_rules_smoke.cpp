#include "../gframe/genesys_rules.h"
#include <cassert>
#include <fstream>
#include <iostream>

int main() {
    using namespace ygo::genesys;
    const char* csv = "genesys_points_test.csv";
    {
        std::ofstream out(csv);
        out << "card_id,card_name,points\n"
            << "123,Test Card,60\n"
            << "456,Alias Card,45\n";
    }
    Rules rules;
    std::string error;
    assert(rules.LoadCsv(csv, &error));
    assert(rules.PointEntryCount() == 2);

    DeckLists deck;
    deck.main.push_back({123, 0, 0, "Test Card"});
    deck.extra.push_back({456, 0, Rules::TYPE_LINK, "Forbidden Link"});
    deck.side.push_back({123, 0, 0, "Test Card"});
    deck.side.push_back({123, 0, 0, "Test Card"});
    deck.side.push_back({123, 0, 0, "Test Card"});
    auto result = rules.Validate(deck, 100);
    assert(result.total_points == 285); // Main + Extra + Side
    assert(result.over_cap);
    assert(result.forbidden_type_ids.size() == 1);
    assert(result.over_three_copy_ids.size() == 1);
    assert(result.unknown_point_ids.empty());

    std::remove(csv);

    const char* named_csv = "genesys_points_by_name_test.csv";
    {
        std::ofstream out(named_csv);
        out << "card_name,points\n"
            << "\"Ash Blossom & Joyous Spring\",18\n"
            << "\"Number 38: Hope Harbinger Dragon Titanic Galaxy\",20\n";
    }
    Rules named_rules;
    assert(named_rules.LoadCsv(named_csv, &error));
    assert(named_rules.PointEntryCount() == 2);
    DeckLists named_deck;
    named_deck.main.push_back({999001, 0, 0, "Ash Blossom & Joyous Spring"});
    named_deck.extra.push_back({999002, 0, 0, "Number 38: Hope Harbinger Dragon Titanic Galaxy"});
    named_deck.side.push_back({999003, 0, 0, "A zero-point card not listed"});
    auto named_result = named_rules.Validate(named_deck, 100);
    assert(named_result.total_points == 38);
    assert(!named_result.over_cap);
    std::remove(named_csv);
    std::cout << "Genesys rule module tests passed.\n";
}
