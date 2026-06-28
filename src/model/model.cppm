export module boxing_trainer.model;

import std;
export import boxing_trainer.combo;

export namespace boxing_trainer::model {

enum class FilterSelection : int {
    all = 0,
    yes = 1,
    no = 2,
};

enum class SortOrder : int {
    in_order = 0,
    random = 1,
};

enum class ActiveView : int {
    main = 0,
    info = 1,
    defense = 2,
};

struct MainViewSelection {
    FilterSelection long_range = FilterSelection::all;
    FilterSelection defense = FilterSelection::all;
    FilterSelection feint = FilterSelection::all;
    FilterSelection body = FilterSelection::all;
    FilterSelection counter = FilterSelection::all;
    FilterSelection favorites = FilterSelection::all;

    friend bool operator==(const MainViewSelection&, const MainViewSelection&) = default;
};

struct DefenseViewSelection {
    FilterSelection long_range = FilterSelection::all;
    FilterSelection body = FilterSelection::all;

    friend bool operator==(const DefenseViewSelection&, const DefenseViewSelection&) = default;
};

struct AppState {
    int current = 0;
    int number = 0;
    SortOrder sort_order = SortOrder::in_order;
    std::uint64_t seed = 0;
    MainViewSelection selection;

    friend bool operator==(const AppState&, const AppState&) = default;
};

struct CombinationItem {
    std::reference_wrapper<const combo::Combination> combination;
    bool is_favorite = false;

    [[nodiscard]] std::string label() const;
};

using TripletSet = std::set<std::string>;

[[nodiscard]] std::string to_string(FilterSelection selection);
[[nodiscard]] FilterSelection selection_from_string(std::string_view value);
[[nodiscard]] std::string to_string(SortOrder order);
[[nodiscard]] SortOrder order_from_string(std::string_view value);
[[nodiscard]] bool matches_filter(FilterSelection selection, bool value);

[[nodiscard]] std::filesystem::path config_path(std::filesystem::path relative);
void ensure_config_dir(const std::filesystem::path& path, std::string_view label);

[[nodiscard]] std::set<std::string> load_favorites();
void save_favorites(const std::set<std::string>& favorites);

[[nodiscard]] AppState load_state();
void save_state(const AppState& state);

[[nodiscard]] std::span<const std::string_view> defense_candidates(DefenseViewSelection selection) noexcept;

class MainModel {
public:
    explicit MainModel(std::vector<combo::Combination> source = {});

    [[nodiscard]] int current() const noexcept { return current_; }
    [[nodiscard]] int number() const noexcept { return number_; }
    [[nodiscard]] const std::vector<CombinationItem>& combinations() const noexcept { return filtered_; }
    [[nodiscard]] std::string_view search_query() const noexcept { return search_query_; }
    [[nodiscard]] MainViewSelection selection() const noexcept { return selection_; }
    [[nodiscard]] SortOrder sort_order() const noexcept { return sort_order_; }
    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
    [[nodiscard]] const std::vector<int>& order() const noexcept { return order_; }

    void set_search_query(std::string query);
    void set_selection(MainViewSelection selection);

    [[nodiscard]] std::string header_label() const;
    [[nodiscard]] std::string combination_name() const;
    [[nodiscard]] std::string title() const;
    [[nodiscard]] const CombinationItem& current_combination() const;
    [[nodiscard]] std::string comment() const;
    [[nodiscard]] const std::map<std::string, std::string>* values() const;

    [[nodiscard]] bool is_favorite(std::string_view name) const;
    void add_favorite(std::string name);
    void remove_favorite(std::string_view name);
    void toggle_favorite();

    void next();
    void previous();
    void set(int index);
    void reset() noexcept;
    void reset_in_random_order(std::optional<std::uint64_t> seed = std::nullopt);
    void reset_in_order();
    void load();

private:
    [[nodiscard]] std::vector<CombinationItem> filter_combinations() const;
    [[nodiscard]] bool filter_one(const combo::Combination& combination) const;
    void update_filter();
    void ensure_current_in_bounds() noexcept;
    void save() const;
    void shuffle_order(std::uint64_t seed);

    [[nodiscard]] bool has_search_query() const;
    [[nodiscard]] int search_score(int source_index, const std::vector<std::string>& query_triplets) const;
    [[nodiscard]] TripletSet triplets_for_source_index(int source_index) const;

    int number_ = 1;
    int current_ = 0;
    MainViewSelection selection_;
    std::vector<CombinationItem> filtered_;
    std::vector<combo::Combination> source_;
    std::vector<int> order_;
    std::set<std::string> favorites_;
    std::string search_query_;
    std::vector<TripletSet> search_triplets_;
    SortOrder sort_order_ = SortOrder::in_order;
    std::uint64_t seed_ = 0;
};

class DefenseModel {
public:
    [[nodiscard]] std::string header_label() const;
    [[nodiscard]] std::string_view combination_name() const noexcept { return current_; }
    void next_attack();
    void set_selection(DefenseViewSelection selection) noexcept { selection_ = selection; }
    [[nodiscard]] DefenseViewSelection selection() const noexcept { return selection_; }
    [[nodiscard]] bool pause() const noexcept { return pause_; }
    void toggle_pause() noexcept { pause_ = !pause_; }

private:
    int number_ = 1;
    std::string current_ = "1";
    DefenseViewSelection selection_;
    bool pause_ = false;
    std::mt19937 rng_{std::random_device{}()};
};

class Model {
public:
    explicit Model(std::vector<combo::Combination> all_data);

    [[nodiscard]] ActiveView active_view() const noexcept { return active_view_; }
    void set_active_view(ActiveView view) noexcept { active_view_ = view; }
    [[nodiscard]] MainModel& main() noexcept { return main_; }
    [[nodiscard]] const MainModel& main() const noexcept { return main_; }
    [[nodiscard]] DefenseModel& defense() noexcept { return defense_; }
    [[nodiscard]] const DefenseModel& defense() const noexcept { return defense_; }

private:
    ActiveView active_view_ = ActiveView::main;
    MainModel main_;
    DefenseModel defense_;
};

[[nodiscard]] std::string yes_no_to_bool(bool value);
[[nodiscard]] std::chrono::milliseconds next_delay();

} // namespace boxing_trainer::model
