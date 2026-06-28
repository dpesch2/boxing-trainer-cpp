module boxing_trainer.video_url;

import std;
import boxing_trainer.common;

namespace boxing_trainer::view {

namespace {

struct ParsedUrl {
    std::string scheme;
    std::string host;
};

[[nodiscard]] std::optional<ParsedUrl> parse_url(std::string_view raw) {
    const auto value = trim(raw);
    const auto scheme_pos = value.find("://");
    if (scheme_pos == std::string::npos || scheme_pos == 0) {
        return std::nullopt;
    }

    const auto scheme = lower_ascii(std::string_view{value}.substr(0, scheme_pos));
    auto rest = std::string_view{value}.substr(scheme_pos + 3);
    if (rest.empty()) {
        return std::nullopt;
    }

    if (const auto slash = rest.find('/'); slash != std::string_view::npos) {
        rest = rest.substr(0, slash);
    }
    if (const auto at = rest.rfind('@'); at != std::string_view::npos) {
        rest = rest.substr(at + 1);
    }
    if (const auto colon = rest.find(':'); colon != std::string_view::npos) {
        rest = rest.substr(0, colon);
    }
    if (rest.empty()) {
        return std::nullopt;
    }

    return ParsedUrl{.scheme = scheme, .host = lower_ascii(rest)};
}

} // namespace

std::optional<std::string> video_url(std::string_view raw) {
    const auto parsed = parse_url(raw);
    if (!parsed) {
        return std::nullopt;
    }
    if (parsed->scheme != "http" && parsed->scheme != "https") {
        return std::nullopt;
    }
    if (!is_allowed_video_host(parsed->host)) {
        return std::nullopt;
    }
    return trim(raw);
}

bool is_allowed_video_host(std::string_view host) {
    const auto normalized = lower_ascii(host);
    static constexpr std::array allowed{
        "youtube.com", "youtu.be", "vimeo.com", "archive.org"};

    for (const auto domain : allowed) {
        if (normalized == domain || normalized.ends_with("." + std::string{domain})) {
            return true;
        }
    }
    return false;
}

} // namespace boxing_trainer::view
