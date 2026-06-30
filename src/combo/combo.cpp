module boxing_trainer.combo;

import std;
import boxing_trainer.common;

namespace boxing_trainer::combo {

namespace {

constexpr std::string_view comment_prefix = "#";
constexpr std::size_t field_count = 3;
constexpr char delimiter = ';';

[[nodiscard]] std::optional<int> parse_int(std::string_view value) noexcept {
    int out = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return out;
}

[[nodiscard]] bool valid_youtube_timestamp(std::string_view value) noexcept {
    if (value.ends_with('s')) {
        const auto seconds_text = value.substr(0, value.size() - 1);
        if (seconds_text.size() >= 1 && seconds_text.size() <= 2) {
            const auto seconds = parse_int(seconds_text);
            if (seconds && *seconds >= 0 && *seconds < 60) {
                return true;
            }
        }
    }

    const auto minute_delimiter = value.find('m');
    if (minute_delimiter == std::string_view::npos || !value.ends_with('s')) {
        return false;
    }

    const auto minutes_text = value.substr(0, minute_delimiter);
    const auto seconds_text = value.substr(minute_delimiter + 1, value.size() - minute_delimiter - 2);
    if (minutes_text.size() < 1 || minutes_text.size() > 2 || seconds_text.size() < 1 || seconds_text.size() > 2) {
        return false;
    }

    const auto minutes = parse_int(minutes_text);
    const auto seconds = parse_int(seconds_text);
    if (!minutes || *minutes < 0 || !seconds || *seconds < 0) {
        return false;
    }

    if (*seconds < 60) {
        return true;
    }
    return false;
}

[[nodiscard]] char lower_ascii_char(char ch) noexcept {
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<char>(ch - 'A' + 'a');
    }
    return ch;
}

[[nodiscard]] bool contains_ascii_case_insensitive(std::string_view value, std::string_view needle) noexcept {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > value.size()) {
        return false;
    }

    for (std::size_t start = 0; start <= value.size() - needle.size(); ++start) {
        bool matched = true;
        for (std::size_t offset = 0; offset < needle.size(); ++offset) {
            if (lower_ascii_char(value[start + offset]) != lower_ascii_char(needle[offset])) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool extract_long_from_parts(std::span<const std::string_view> parts) {
    if (parts.empty()) {
        return false;
    }

    const auto first = parts.front();
    static constexpr std::array<std::string_view, 6> long_prefixes{
        "1", "2", "f1", "long_3", "counter_1", "counter_2"};
    for (const auto prefix : long_prefixes) {
        if (first == prefix) {
            return true;
        }
    }

    if (first == "setup") {
        return std::ranges::none_of(parts, [](std::string_view part) { return part == "close"; });
    }

    return false;
}

[[nodiscard]] bool extract_defense_from_parts(std::span<const std::string_view> parts) {
    static constexpr std::array<std::string_view, 9> keywords{
        "slip", "step", "lean", "roll", "escape", "pivot", "duck", "parry", "block"};

    for (const auto part : parts) {
        for (const auto keyword : keywords) {
            if (contains_ascii_case_insensitive(part, keyword)) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool extract_faint_from_parts(std::span<const std::string_view> parts) {
    return std::ranges::any_of(parts, [](std::string_view part) {
        return starts_with(part, "f1") || starts_with(part, "f2");
    });
}

[[nodiscard]] bool extract_body_from_parts(std::span<const std::string_view> parts) {
    static constexpr std::array<std::string_view, 6> keywords{"1b", "2b", "3b", "4b", "5b", "6b"};
    for (const auto part : parts) {
        for (const auto keyword : keywords) {
            if (part == keyword || contains(part, keyword)) {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] CombinationFeatures extract_features(std::string_view description) {
    const auto parts = split_description(description);
    return CombinationFeatures{
        .is_long = extract_long_from_parts(parts),
        .has_defense = extract_defense_from_parts(parts),
        .is_feint = extract_faint_from_parts(parts),
        .targets_body = extract_body_from_parts(parts),
        .is_counter = contains_ascii_case_insensitive(description, "counter"),
    };
}

} // namespace

bool parse_comment(std::string_view line) {
    const auto cleaned = trim(line);
    return cleaned.empty() || starts_with(cleaned, comment_prefix);
}

bool parse_var_defs(std::string_view line, std::set<std::string>& vars) {
    auto cleaned = trim(line);
    constexpr std::string_view prefix = "var ";
    if (!starts_with(cleaned, prefix)) {
        return false;
    }

    auto name = trim(std::string_view{cleaned}.substr(prefix.size()));
    if (name.empty()) {
        return false;
    }

    vars.emplace(name);
    return true;
}

bool parse_var_assignments(
    std::string_view line,
    const std::set<std::string>& vars,
    std::map<std::string, std::string>& values) {
    auto cleaned = trim(line);
    for (const auto& var : vars) {
        if (cleaned.starts_with(var) && cleaned.substr(var.size()).starts_with('=')) {
            values[var] = std::string{trim(cleaned.substr(var.size() + 1))};
            return true;
        }
    }
    return false;
}

std::expected<Combination, std::string> parse_combination(
    std::string_view line,
    std::int64_t line_no,
    const std::map<std::string, std::string>& values) {
    const auto parts = split_view(line, delimiter);
    if (parts.size() != field_count) {
        return std::unexpected(std::format(
            R"(expect {} elements delimited by ";" in "{}")",
            field_count,
            line));
    }

    const auto description = trim(parts[0]);
    const auto url_it = values.find("url");
    if (url_it == values.end() || trim(url_it->second).empty()) {
        return std::unexpected(std::format(
            R"(missing at line {} `url` value for combination: "{}")",
            line_no,
            line));
    }

    const auto location = trim(parts[1]);
    const auto url = create_url_with_location(url_it->second, location, line_no);
    if (!url) {
        return std::unexpected(url.error());
    }

    auto value_copy = values;
    value_copy["lineNo"] = std::format("{}", line_no);

    return Combination{
        .description = std::string{description},
        .features = extract_features(description),
        .url = std::move(url.value()),
        .position = std::string{location},
        .comment = std::string{trim(parts[2])},
        .values = std::move(value_copy),
    };
}

bool extract_long(std::string_view description) {
    const auto parts = split_description(description);
    return extract_long_from_parts(parts);
}

bool extract_defense(std::string_view description) {
    const auto parts = split_description(description);
    return extract_defense_from_parts(parts);
}

bool extract_faint(std::string_view description) {
    const auto parts = split_description(description);
    return extract_faint_from_parts(parts);
}

bool extract_body(std::string_view description) {
    const auto parts = split_description(description);
    return extract_body_from_parts(parts);
}

bool extract_counter(std::string_view description) {
    return contains_ascii_case_insensitive(description, "counter");
}

std::vector<std::string_view> split_description(std::string_view description) {
    static constexpr std::array delimiters{'-', '+', ' '};
    std::vector<std::string_view> cleaned;
    std::size_t start = 0;
    while (start < description.size()) {
        const auto end = description.find_first_of(delimiters.data(), start, delimiters.size());
        const auto value = trim(description.substr(start, end == std::string_view::npos ? end : end - start));
        if (!value.empty()) {
            cleaned.emplace_back(value);
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return cleaned;
}

std::expected<std::string, std::string> create_url_with_location(
    std::string_view url,
    std::string_view timestamp,
    std::int64_t line_no) {
    if (!contains(url, "youtube")) {
        return std::string{url};
    }

    const auto cleaned = trim(timestamp);
    if (valid_youtube_timestamp(cleaned)) {
        return append_time_param(url, cleaned);
    }

    return std::unexpected(std::format(
        R"(invalid timestamp at line {} timestamp: {})",
        line_no,
        timestamp));
}

std::string append_time_param(std::string_view url, std::string_view timestamp) {
    return std::format("{}{}t={}", url, contains(url, "?") ? "&" : "?", timestamp);
}

std::expected<std::vector<Combination>, std::string> load_data(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return std::unexpected(std::format("open combinations data: {}", path.string()));
    }

    std::vector<Combination> data;
    data.reserve(255);
    std::set<std::string> vars;
    std::map<std::string, std::string> values;

    std::string line;
    std::int64_t line_no = -1;
    while (std::getline(input, line)) {
        ++line_no;
        const auto cleaned = trim(line);
        if (parse_comment(cleaned)) {
            continue;
        }
        if (parse_var_defs(cleaned, vars)) {
            continue;
        }
        if (parse_var_assignments(cleaned, vars, values)) {
            continue;
        }
        auto result = parse_combination(cleaned, line_no, values);
        if (!result) {
            return std::unexpected(result.error());
        }
        data.push_back(std::move(result.value()));
    }
    return data;
}

} // namespace boxing_trainer::combo
