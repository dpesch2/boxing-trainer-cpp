module boxing_trainer.model;

import std;
import boxing_trainer.common;

namespace boxing_trainer::model {

namespace {

constexpr std::string_view state_file_rel = ".boxing-trainer/state-2.json";
constexpr std::string_view favorites_file_rel = ".boxing-trainer/favorites";
constexpr std::size_t search_triplet_size = 3;
std::mutex favorites_mutex;

[[nodiscard]] std::vector<std::string> split_search_triplets(std::string_view query) {
    const auto normalized = collapse_whitespace_lower(query);
    if (normalized.size() < search_triplet_size) {
        return {};
    }

    std::vector<std::string> out;
    out.reserve(normalized.size() - search_triplet_size + 1);
    for (std::size_t i = 0; i <= normalized.size() - search_triplet_size; ++i) {
        out.push_back(normalized.substr(i, search_triplet_size));
    }
    return out;
}

[[nodiscard]] TripletSet triplet_set_from_text(std::string_view text) {
    TripletSet out;
    for (auto&& triplet : split_search_triplets(text)) {
        out.insert(std::move(triplet));
    }
    return out;
}

[[nodiscard]] TripletSet triplets_for_combination(const combo::Combination& combination) {
    return triplet_set_from_text(combination.description + " " + combination.comment);
}

[[nodiscard]] std::vector<TripletSet> build_combination_search_triplets(
    const std::vector<combo::Combination>& data) {
    std::vector<TripletSet> out;
    out.reserve(data.size());
    for (const auto& combination : data) {
        out.push_back(triplets_for_combination(combination));
    }
    return out;
}

struct SearchResult {
    CombinationItem item;
    int score = 0;
};

[[nodiscard]] std::vector<CombinationItem> ranked_search_results(std::vector<SearchResult> results) {
    std::ranges::stable_sort(results, [](const auto& left, const auto& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        const auto left_len = left.item.combination.get().description.size();
        const auto right_len = right.item.combination.get().description.size();
        return left_len < right_len;
    });

    std::vector<CombinationItem> out;
    out.reserve(results.size());
    for (const auto& result : results) {
        out.push_back(result.item);
    }
    return out;
}

[[nodiscard]] int enum_or_default(int value, int min, int max, int fallback) {
    return value >= min && value <= max ? value : fallback;
}

[[nodiscard]] int json_number_or(std::string_view json, std::string_view key, int fallback) {
    const auto pattern = "\"" + std::string{key} + R"("\s*:\s*(-?\d+))";
    const std::regex re{pattern};
    std::cmatch match;
    if (!std::regex_search(json.data(), json.data() + json.size(), match, re)) {
        return fallback;
    }
    return std::stoi(match[1].str());
}

[[nodiscard]] std::uint64_t json_uint64_or(std::string_view json, std::string_view key, std::uint64_t fallback) {
    const auto pattern = "\"" + std::string{key} + R"("\s*:\s*(\d+))";
    const std::regex re{pattern};
    std::cmatch match;
    if (!std::regex_search(json.data(), json.data() + json.size(), match, re)) {
        return fallback;
    }
    return static_cast<std::uint64_t>(std::stoull(match[1].str()));
}

void require_probably_json_object(std::string_view json) {
    const auto cleaned = trim(json);
    if (!cleaned.starts_with('{') || !cleaned.ends_with('}')) {
        throw std::runtime_error("unmarshal state file: invalid JSON object");
    }
}

[[nodiscard]] std::filesystem::path home_dir() {
#ifdef _WIN32
    if (const char* user_profile = std::getenv("USERPROFILE"); user_profile != nullptr && *user_profile != '\0') {
        return user_profile;
    }
#endif
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return home;
    }
    throw std::runtime_error("determine home directory");
}

[[nodiscard]] std::chrono::milliseconds now_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

} // namespace

std::string CombinationItem::label() const {
    const auto& description = combination.get().description;
    return is_favorite ? description + " ★" : description;
}

std::string to_string(FilterSelection selection) {
    switch (selection) {
    case FilterSelection::yes:
        return "Yes";
    case FilterSelection::no:
        return "No";
    case FilterSelection::all:
    default:
        return "All";
    }
}

FilterSelection selection_from_string(std::string_view value) {
    if (value == "Yes" || value == "yes") {
        return FilterSelection::yes;
    }
    if (value == "No" || value == "no") {
        return FilterSelection::no;
    }
    return FilterSelection::all;
}

std::string to_string(SortOrder order) {
    switch (order) {
    case SortOrder::random:
        return "Random";
    case SortOrder::in_order:
    default:
        return "InOrder";
    }
}

SortOrder order_from_string(std::string_view value) {
    return value == "Random" ? SortOrder::random : SortOrder::in_order;
}

bool matches_filter(FilterSelection selection, bool is_yes) {
    switch (selection) {
    case FilterSelection::yes:
        return is_yes;
    case FilterSelection::no:
        return !is_yes;
    case FilterSelection::all:
    default:
        return true;
    }
}

std::filesystem::path config_path(std::filesystem::path relative) {
    return home_dir() / std::move(relative);
}

void ensure_config_dir(const std::filesystem::path& path, std::string_view label) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        throw std::runtime_error("create " + std::string{label} + " directory: " + ec.message());
    }
}

std::set<std::string> load_favorites() {
    std::scoped_lock lock{favorites_mutex};

    std::set<std::string> favorites;
    std::ifstream input(config_path(favorites_file_rel));
    if (!input) {
        return favorites;
    }

    std::string line;
    while (std::getline(input, line)) {
        auto cleaned = trim(line);
        if (!cleaned.empty()) {
            favorites.insert(std::move(cleaned));
        }
    }
    if (input.bad()) {
        throw std::runtime_error("read favorites file");
    }
    return favorites;
}

void save_favorites(const std::set<std::string>& favorites) {
    std::scoped_lock lock{favorites_mutex};

    const auto path = config_path(favorites_file_rel);
    ensure_config_dir(path, "favorites");

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("write favorites file");
    }

    bool first = true;
    for (const auto& favorite : favorites) {
        if (!first) {
            output << '\n';
        }
        first = false;
        output << favorite;
    }
}

AppState load_state() {
    const auto path = config_path(state_file_rel);
    std::ifstream input(path);
    if (!input) {
        return {};
    }

    const std::string json{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (input.bad()) {
        throw std::runtime_error("read state file");
    }

    require_probably_json_object(json);
    const auto selection_value = [&json](std::string_view key) {
        const auto raw = json_number_or(json, key, 0);
        return static_cast<FilterSelection>(enum_or_default(raw, 0, 2, 0));
    };

    return AppState{
        .current = json_number_or(json, "current", 0),
        .number = json_number_or(json, "number", 0),
        .sort_order = static_cast<SortOrder>(enum_or_default(json_number_or(json, "order", 0), 0, 1, 0)),
        .seed = json_uint64_or(json, "seed", 0),
        .selection = MainViewSelection{
            .long_range = selection_value("selection_long"),
            .defense = selection_value("selection_defense"),
            .feint = selection_value("selection_faint"),
            .body = selection_value("selection_body"),
            .counter = selection_value("selection_counter"),
            .favorites = selection_value("selection_fav"),
        },
    };
}

void save_state(const AppState& state) {
    const auto path = config_path(state_file_rel);
    ensure_config_dir(path, "state");

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("write state file");
    }

    output
        << "{\n"
        << "  \"current\": " << state.current << ",\n"
        << "  \"number\": " << state.number << ",\n"
        << "  \"order\": " << std::to_underlying(state.sort_order) << ",\n"
        << "  \"seed\": " << state.seed << ",\n"
        << "  \"selection_long\": " << std::to_underlying(state.selection.long_range) << ",\n"
        << "  \"selection_defense\": " << std::to_underlying(state.selection.defense) << ",\n"
        << "  \"selection_faint\": " << std::to_underlying(state.selection.feint) << ",\n"
        << "  \"selection_body\": " << std::to_underlying(state.selection.body) << ",\n"
        << "  \"selection_counter\": " << std::to_underlying(state.selection.counter) << ",\n"
        << "  \"selection_fav\": " << std::to_underlying(state.selection.favorites) << "\n"
        << "}";
}

std::span<const std::string_view> defense_candidates(DefenseViewSelection selection) noexcept {
    static constexpr std::array<std::string_view, 7> attacks_head{"1", "2", "3", "4", "5", "6", "8"};
    static constexpr std::array<std::string_view, 6> attacks_body{"1b", "2b", "3b", "4b", "5b", "6b"};
    static constexpr std::array<std::string_view, 13> attacks_all{
        "1", "2", "3", "4", "5", "6", "8", "1b", "2b", "3b", "4b", "5b", "6b"};
    static constexpr std::array<std::string_view, 6> attacks_long{"1", "2", "3", "1b", "2b", "8"};
    static constexpr std::array<std::string_view, 8> attacks_short{"3", "4", "5", "6", "3b", "4b", "5b", "6b"};
    static constexpr std::array<std::string_view, 2> body_long{"1b", "2b"};
    static constexpr std::array<std::string_view, 4> body_short{"3b", "4b", "5b", "6b"};
    static constexpr std::array<std::string_view, 4> head_long{"1", "2", "3", "8"};
    static constexpr std::array<std::string_view, 3> head_short{"4", "5", "6"};

    switch (selection.body) {
    case FilterSelection::yes:
        switch (selection.long_range) {
        case FilterSelection::yes:
            return body_long;
        case FilterSelection::no:
            return body_short;
        case FilterSelection::all:
        default:
            return attacks_body;
        }
    case FilterSelection::no:
        switch (selection.long_range) {
        case FilterSelection::yes:
            return head_long;
        case FilterSelection::no:
            return head_short;
        case FilterSelection::all:
        default:
            return attacks_head;
        }
    case FilterSelection::all:
    default:
        switch (selection.long_range) {
        case FilterSelection::yes:
            return attacks_long;
        case FilterSelection::no:
            return attacks_short;
        case FilterSelection::all:
        default:
            return attacks_all;
        }
    }
}

MainModel::MainModel(std::vector<combo::Combination> source)
    : source_{std::move(source)}
    , order_(source_.size())
    , favorites_{load_favorites()}
    , search_triplets_{build_combination_search_triplets(source_)} {
    std::iota(order_.begin(), order_.end(), 0);
    update_filter();
}

void MainModel::set_search_query(std::string query) {
    if (search_query_ == query) {
        return;
    }
    search_query_ = std::move(query);
    current_ = 0;
    update_filter();
}

void MainModel::set_selection(MainViewSelection selection) {
    selection_ = selection;
    update_filter();
    save();
}

std::string MainModel::header_label() const {
    std::string favorite;
    if (!filtered_.empty() && is_favorite(combination_name())) {
        favorite = "★";
    }

    std::ostringstream out;
    out << number_ << ". (" << filtered_.size() << ") " << favorite << " " << comment();
    return out.str();
}

std::string MainModel::combination_name() const {
    if (filtered_.empty()) {
        return "None";
    }
    return filtered_.at(static_cast<std::size_t>(current_)).combination.get().description;
}

std::string MainModel::title() const {
    if (filtered_.empty()) {
        return {};
    }

    const auto& values = filtered_.at(static_cast<std::size_t>(current_)).combination.get().values;
    const auto author = values.find("author");
    const auto title_value = values.find("title");
    return (author == values.end() ? std::string{} : author->second)
        + ": "
        + (title_value == values.end() ? std::string{} : title_value->second);
}

const CombinationItem& MainModel::current_combination() const {
    if (filtered_.empty()) {
        throw std::runtime_error("no current combination");
    }
    return filtered_.at(static_cast<std::size_t>(current_));
}

std::string MainModel::comment() const {
    if (filtered_.empty()) {
        return {};
    }
    return filtered_.at(static_cast<std::size_t>(current_)).combination.get().comment;
}

const std::map<std::string, std::string>* MainModel::values() const {
    if (filtered_.empty()) {
        return nullptr;
    }
    return &filtered_.at(static_cast<std::size_t>(current_)).combination.get().values;
}

bool MainModel::is_favorite(std::string_view name) const {
    return favorites_.contains(std::string{name});
}

void MainModel::add_favorite(std::string name) {
    favorites_.insert(std::move(name));
    save_favorites(favorites_);
}

void MainModel::remove_favorite(std::string_view name) {
    favorites_.erase(std::string{name});
    save_favorites(favorites_);
}

void MainModel::toggle_favorite() {
    if (filtered_.empty()) {
        return;
    }

    auto& current_combo = filtered_.at(static_cast<std::size_t>(current_));
    const auto& name = current_combo.combination.get().description;
    if (!is_favorite(name)) {
        add_favorite(name);
        current_combo.is_favorite = true;
    } else {
        remove_favorite(name);
        current_combo.is_favorite = false;
    }
}

void MainModel::next() {
    if (filtered_.empty()) {
        return;
    }
    ++number_;
    if (number_ < 0) {
        number_ = 1;
    }
    current_ = (current_ + 1) % static_cast<int>(filtered_.size());
    save();
}

void MainModel::previous() {
    if (filtered_.empty()) {
        return;
    }
    ++number_;
    if (number_ < 0) {
        number_ = 1;
    }
    current_ = current_ == 0 ? static_cast<int>(filtered_.size()) - 1 : current_ - 1;
    save();
}

void MainModel::set(int index) {
    if (filtered_.empty() || index < 0 || index >= static_cast<int>(filtered_.size())) {
        return;
    }
    ++number_;
    current_ = index;
    save();
}

void MainModel::reset() noexcept {
    number_ = 1;
    current_ = 0;
}

void MainModel::reset_in_random_order(std::optional<std::uint64_t> seed) {
    reset();
    sort_order_ = SortOrder::random;
    seed_ = seed.value_or(static_cast<std::uint64_t>(now_millis().count()));
    shuffle_order(seed_);
    update_filter();
    save();
}

void MainModel::reset_in_order() {
    reset();
    std::iota(order_.begin(), order_.end(), 0);
    sort_order_ = SortOrder::in_order;
    seed_ = 0;
    update_filter();
    save();
}

void MainModel::load() {
    const auto state = load_state();
    current_ = state.current;
    number_ = state.number;
    sort_order_ = state.sort_order;
    seed_ = state.seed;
    selection_ = state.selection;

    if (sort_order_ == SortOrder::random) {
        shuffle_order(seed_);
    } else {
        std::iota(order_.begin(), order_.end(), 0);
    }
    update_filter();
}

std::vector<CombinationItem> MainModel::filter_combinations() const {
    const auto search_active = has_search_query();
    const auto query_triplets = split_search_triplets(search_query_);
    std::vector<CombinationItem> out;
    out.reserve(source_.size());
    std::vector<SearchResult> matches;
    matches.reserve(source_.size());

    for (const auto source_index : order_) {
        const auto& combination = source_.at(static_cast<std::size_t>(source_index));
        if (!filter_one(combination)) {
            continue;
        }

        const CombinationItem combo_model{.combination = std::cref(combination), .is_favorite = is_favorite(combination.description)};
        if (!search_active) {
            out.push_back(combo_model);
            continue;
        }

        const auto score = search_score(source_index, query_triplets);
        if (score > 0) {
            matches.push_back(SearchResult{.item = combo_model, .score = score});
        }
    }

    if (search_active) {
        return ranked_search_results(std::move(matches));
    }
    return out;
}

bool MainModel::filter_one(const combo::Combination& combination) const {
    const auto& features = combination.features;
    const auto favorite = favorites_.contains(combination.description);
    return matches_filter(selection_.long_range, features.is_long)
        && matches_filter(selection_.defense, features.has_defense)
        && matches_filter(selection_.feint, features.is_feint)
        && matches_filter(selection_.body, features.targets_body)
        && matches_filter(selection_.counter, features.is_counter)
        && matches_filter(selection_.favorites, favorite);
}

void MainModel::update_filter() {
    filtered_ = filter_combinations();
    ensure_current_in_bounds();
}

void MainModel::ensure_current_in_bounds() noexcept {
    if (filtered_.empty() || current_ < 0 || current_ >= static_cast<int>(filtered_.size())) {
        current_ = 0;
    }
}

void MainModel::save() const {
    save_state(AppState{
        .current = current_,
        .number = number_,
        .sort_order = sort_order_,
        .seed = seed_,
        .selection = selection_,
    });
}

void MainModel::shuffle_order(std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::ranges::shuffle(order_, rng);
}

bool MainModel::has_search_query() const {
    return !trim(search_query_).empty();
}

int MainModel::search_score(int source_index, const std::vector<std::string>& query_triplets) const {
    if (query_triplets.empty()) {
        return 0;
    }

    const auto target_triplets = triplets_for_source_index(source_index);
    if (target_triplets.empty()) {
        return 0;
    }

    int score = 0;
    for (const auto& triplet : query_triplets) {
        if (target_triplets.contains(triplet)) {
            ++score;
        }
    }
    return score;
}

TripletSet MainModel::triplets_for_source_index(int source_index) const {
    if (source_index >= 0 && source_index < static_cast<int>(search_triplets_.size())) {
        return search_triplets_.at(static_cast<std::size_t>(source_index));
    }
    if (source_index < 0 || source_index >= static_cast<int>(source_.size())) {
        return {};
    }
    return triplets_for_combination(source_.at(static_cast<std::size_t>(source_index)));
}

std::string DefenseModel::header_label() const {
    return std::to_string(number_) + ". ";
}

void DefenseModel::next_attack() {
    const auto candidates = defense_candidates(selection_);
    if (candidates.empty()) {
        return;
    }
    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
    current_ = std::string{candidates[dist(rng_)]};
    ++number_;
}

Model::Model(std::vector<combo::Combination> all_data)
    : main_{std::move(all_data)} {}

std::string yes_no_to_bool(bool value) {
    return value ? "yes" : "no";
}

std::chrono::milliseconds next_delay() {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 2999);
    return std::chrono::milliseconds{3000 + dist(rng)};
}

} // namespace boxing_trainer::model
