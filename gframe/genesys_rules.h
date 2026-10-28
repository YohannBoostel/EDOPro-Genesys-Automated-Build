#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ygo::genesys {

// Deliberately independent of EDOPro's CardDataC so this rule engine can be
// unit-tested and then adapted at the DeckBuilder/DeckManager boundary.
struct CardEntry {
    uint32_t id = 0;
    uint32_t alias = 0;
    uint32_t type = 0;
    std::string name;
};

struct DeckLists {
    std::vector<CardEntry> main;
    std::vector<CardEntry> extra;
    std::vector<CardEntry> side;
};

struct ValidationResult {
    int total_points = 0;
    int point_cap = 100;
    bool over_cap = false;
    std::vector<uint32_t> unknown_point_ids;
    std::vector<uint32_t> forbidden_type_ids;
    std::vector<uint32_t> over_three_copy_ids;
};

class Rules {
public:
    static constexpr uint32_t TYPE_PENDULUM = 0x1000000;
    static constexpr uint32_t TYPE_LINK = 0x4000000;

    bool LoadCsv(const std::filesystem::path& path, std::string* error = nullptr);
    int PointsFor(uint32_t id, uint32_t alias = 0, const std::string& name = {}) const;
    bool HasPointEntry(uint32_t id, uint32_t alias = 0, const std::string& name = {}) const;
    ValidationResult Validate(const DeckLists& deck, int cap = 100) const;
    size_t PointEntryCount() const noexcept { return points_.size() + name_points_.size(); }

private:
    std::unordered_map<uint32_t, int> points_;
    std::unordered_map<std::string, int> name_points_;
};

} // namespace ygo::genesys
