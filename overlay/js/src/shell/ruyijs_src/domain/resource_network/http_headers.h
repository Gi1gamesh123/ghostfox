#pragma once
#include "domain/resource_network/header_rules.h"
#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::resource_network {
// The ordered field list is the only authoritative representation.
class HttpHeaders {
public:
    using Pairs = std::vector<std::pair<std::string, std::string>>;
    void append(const std::string& name, const std::string& value) {
        fields_.emplace_back(lower_header_name(name), value);
    }
    void set(const std::string& name, const std::string& value) {
        const auto key = lower_header_name(name);
        Pairs candidate;
        candidate.reserve(fields_.size() + 1);
        bool replaced = false;
        for (const auto& field : fields_) {
            if (field.first != key) candidate.push_back(field);
            else if (!replaced) { candidate.emplace_back(key, value); replaced = true; }
        }
        if (!replaced) candidate.emplace_back(key, value);
        fields_.swap(candidate);
    }
    void erase(const std::string& name) {
        const auto key = lower_header_name(name);
        fields_.erase(std::remove_if(fields_.begin(), fields_.end(),
            [&](const auto& field) { return field.first == key; }), fields_.end());
    }
    std::string get(const std::string& name) const {
        const auto key = lower_header_name(name);
        for (const auto& field : fields_) if (field.first == key) return field.second;
        return {};
    }
    std::string combined_get(const std::string& name) const {
        const auto key = lower_header_name(name);
        std::string result;
        bool found = false;
        for (const auto& field : fields_) {
            if (field.first != key) continue;
            if (found) result += ", ";
            result += field.second;
            found = true;
        }
        return result;
    }
    std::vector<std::string> get_all(const std::string& name) const {
        const auto key = lower_header_name(name);
        std::vector<std::string> result;
        for (const auto& field : fields_) if (field.first == key) result.push_back(field.second);
        return result;
    }
    bool has(const std::string& name) const {
        const auto key = lower_header_name(name);
        for (const auto& field : fields_) if (field.first == key) return true;
        return false;
    }
    // Compatibility projection; it is never retained as a parallel owner.
    std::map<std::string, std::vector<std::string>> all() const {
        std::map<std::string, std::vector<std::string>> result;
        for (const auto& field : fields_) result[field.first].push_back(field.second);
        return result;
    }
    size_t size() const noexcept {
        size_t count = 0;
        for (size_t i = 0; i < fields_.size(); ++i) {
            bool duplicate = false;
            for (size_t j = 0; j < i; ++j) duplicate |= fields_[j].first == fields_[i].first;
            if (!duplicate) ++count;
        }
        return count;
    }
    size_t value_count() const noexcept { return fields_.size(); }
    size_t bytes() const noexcept {
        size_t result = 0;
        for (const auto& field : fields_) result += field.first.size() + field.second.size();
        return result;
    }
    bool empty() const noexcept { return fields_.empty(); }
    const Pairs& raw_pairs() const noexcept { return fields_; }
    Pairs to_pairs() const { return fields_; }
    Pairs sorted_combined() const {
        auto sorted = fields_;
        std::stable_sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        Pairs result;
        for (auto& field : sorted) {
            if (!result.empty() && result.back().first == field.first && field.first != "set-cookie")
                result.back().second += ", " + field.second;
            else result.push_back(std::move(field));
        }
        return result;
    }
    void swap(HttpHeaders& other) noexcept { fields_.swap(other.fields_); }
private:
    Pairs fields_;
};
}
