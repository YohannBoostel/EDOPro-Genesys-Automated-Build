#include "genesys_rules.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace ygo::genesys {
namespace {
std::string Trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n\xef\xbb\xbf");
    if(first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}
std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cell;
    bool quoted = false;
    for(size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if(c == '"') {
            if(quoted && i + 1 < line.size() && line[i + 1] == '"') {
                cell.push_back('"');
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if(c == ',' && !quoted) {
            out.push_back(Trim(cell));
            cell.clear();
        } else {
            cell.push_back(c);
        }
    }
    out.push_back(Trim(cell));
    return out;
}
std::string NormalizeName(std::string name) {
    name = Trim(std::move(name));
    std::string out;
    out.reserve(name.size());
    bool last_space = false;
    for(unsigned char c : name) {
        if(c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
        if(c == '\t' || c == '\r' || c == '\n' || c == ' ') {
            if(!last_space && !out.empty()) out.push_back(' ');
            last_space = true;
        } else {
            out.push_back(static_cast<char>(c));
            last_space = false;
        }
    }
    return out;
}
}

bool Rules::LoadCsv(const std::filesystem::path& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if(!in) {
        if(error) *error = "Could not open Genesys points CSV: " + path.string();
        return false;
    }
    std::unordered_map<uint32_t, int> loaded_ids;
    std::unordered_map<std::string, int> loaded_names;
    std::string line;
    bool header_seen = false;
    bool by_name = false;
    size_t line_number = 0;
    while(std::getline(in, line)) {
        ++line_number;
        line = Trim(line);
        if(line.empty() || line[0] == '#') continue;
        const auto cols = SplitCsvLine(line);
        if(!header_seen) {
            header_seen = true;
            if(cols.size() >= 3 && cols[0] == "card_id" && cols[2] == "points") {
                by_name = false;
            } else if(cols.size() >= 2 && cols[0] == "card_name" && cols[1] == "points") {
                by_name = true;
            } else {
                if(error) *error = "CSV header must be card_id,card_name,points OR card_name,points";
                return false;
            }
            continue;
        }
        try {
            if(by_name) {
                if(cols.size() < 2 || cols[0].empty()) throw std::runtime_error("missing name");
                const int pts = std::stoi(cols[1]);
                if(pts < 0) throw std::runtime_error("negative points");
                loaded_names[NormalizeName(cols[0])] = pts;
            } else {
                if(cols.size() < 3) throw std::runtime_error("missing columns");
                const auto id = static_cast<uint32_t>(std::stoul(cols[0]));
                const int pts = std::stoi(cols[2]);
                if(id == 0 || pts < 0) throw std::runtime_error("invalid value");
                loaded_ids[id] = pts;
            }
        } catch(...) {
            if(error) *error = "Invalid card name/ID/points at CSV line " + std::to_string(line_number);
            return false;
        }
    }
    if(!header_seen) {
        if(error) *error = "CSV is empty or has no header";
        return false;
    }
    points_ = std::move(loaded_ids);
    name_points_ = std::move(loaded_names);
    return true;
}

int Rules::PointsFor(uint32_t id, uint32_t alias, const std::string& name) const {
    if(auto it = points_.find(id); it != points_.end()) return it->second;
    if(alias) {
        if(auto it = points_.find(alias); it != points_.end()) return it->second;
    }
    if(!name.empty()) {
        if(auto it = name_points_.find(NormalizeName(name)); it != name_points_.end()) return it->second;
    }
    return 0;
}

bool Rules::HasPointEntry(uint32_t id, uint32_t alias, const std::string& name) const {
    if(points_.find(id) != points_.end() || (alias && points_.find(alias) != points_.end())) return true;
    return !name.empty() && name_points_.find(NormalizeName(name)) != name_points_.end();
}

ValidationResult Rules::Validate(const DeckLists& deck, int cap) const {
    ValidationResult result;
    result.point_cap = std::max(0, cap);
    std::unordered_map<uint32_t, unsigned> copies;
    std::unordered_set<uint32_t> unknown, forbidden, over_three;
    auto visit = [&](const std::vector<CardEntry>& cards) {
        for(const auto& card : cards) {
            result.total_points += PointsFor(card.id, card.alias, card.name);
            const uint32_t count_key = card.alias ? card.alias : card.id;
            if(++copies[count_key] > 3) over_three.insert(count_key);
            if(card.type & (GENESYS_TYPE_LINK | GENESYS_TYPE_PENDULUM)) forbidden.insert(card.id);
            if(!HasPointEntry(card.id, card.alias, card.name)) unknown.insert(card.id);
        }
    };
    visit(deck.main);
    visit(deck.extra);
    visit(deck.side);
    result.over_cap = result.total_points > result.point_cap;
    result.unknown_point_ids.assign(unknown.begin(), unknown.end());
    result.forbidden_type_ids.assign(forbidden.begin(), forbidden.end());
    result.over_three_copy_ids.assign(over_three.begin(), over_three.end());
    std::sort(result.unknown_point_ids.begin(), result.unknown_point_ids.end());
    std::sort(result.forbidden_type_ids.begin(), result.forbidden_type_ids.end());
    std::sort(result.over_three_copy_ids.begin(), result.over_three_copy_ids.end());
    return result;
}

} // namespace ygo::genesys
