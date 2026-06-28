export module boxing_trainer.common;

import std;

export namespace boxing_trainer {

[[nodiscard]] std::string trim(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return {begin, end};
}

[[nodiscard]] std::string lower_ascii(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const auto ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

[[nodiscard]] bool starts_with(std::string_view value, std::string_view prefix) {
    return value.starts_with(prefix);
}

[[nodiscard]] bool contains(std::string_view value, std::string_view needle) {
    return value.find(needle) != std::string_view::npos;
}

[[nodiscard]] std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto pos = value.find(delimiter, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(value.substr(start));
            break;
        }
        parts.emplace_back(value.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

[[nodiscard]] std::string collapse_whitespace_lower(std::string_view text) {
    std::istringstream input(std::string{text});
    std::string word;
    std::string out;
    while (input >> word) {
        if (!out.empty()) {
            out.push_back(' ');
        }
        out.append(lower_ascii(word));
    }
    return out;
}

} // namespace boxing_trainer
