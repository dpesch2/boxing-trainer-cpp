export module boxing_trainer.combo;

import std;

export namespace boxing_trainer::combo {

struct CombinationFeatures {
    bool is_long = false;
    bool has_defense = false;
    bool is_feint = false;
    bool targets_body = false;
    bool is_counter = false;

    friend bool operator==(const CombinationFeatures&, const CombinationFeatures&) = default;
};

struct Combination {
    std::string description;
    CombinationFeatures features;
    std::string url;
    std::string position;
    std::string comment;
    std::map<std::string, std::string> values;

    friend bool operator==(const Combination&, const Combination&) = default;
};

[[nodiscard]] bool parse_comment(std::string_view line);
[[nodiscard]] bool parse_var_defs(std::string_view line, std::set<std::string>& vars);
[[nodiscard]] bool parse_var_assignments(
    std::string_view line,
    const std::set<std::string>& vars,
    std::map<std::string, std::string>& values);

[[nodiscard]] std::expected<Combination, std::string> parse_combination(
    std::string_view line,
    std::int64_t line_no,
    const std::map<std::string, std::string>& values);

[[nodiscard]] bool extract_long(std::string_view description);
[[nodiscard]] bool extract_defense(std::string_view description);
[[nodiscard]] bool extract_faint(std::string_view description);
[[nodiscard]] bool extract_body(std::string_view description);
[[nodiscard]] bool extract_counter(std::string_view description);

[[nodiscard]] std::vector<std::string_view> split_description(std::string_view description);
[[nodiscard]] std::expected<std::string, std::string> create_url_with_location(
    std::string_view url,
    std::string_view timestamp,
    std::int64_t line_no);
[[nodiscard]] std::string append_time_param(std::string_view url, std::string_view timestamp);

[[nodiscard]] std::expected<std::vector<Combination>, std::string> load_data(
    const std::filesystem::path& path = std::filesystem::path{BOXING_TRAINER_RESOURCE_DIR} / "combinations.txt");

} // namespace boxing_trainer::combo
