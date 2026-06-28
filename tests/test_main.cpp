import std;
import boxing_trainer.combo;
import boxing_trainer.model;
import boxing_trainer.video_url;

#ifdef _WIN32
extern "C" int _putenv_s(const char*, const char*);
#else
extern "C" int setenv(const char*, const char*, int);
#endif

using namespace boxing_trainer;

namespace {

struct TestFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct TestCase {
    std::string name;
    std::function<void()> run;
};

template <typename T, typename U>
void require_equal(const T& got, const U& want, std::string_view context) {
    if (!(got == want)) {
        std::ostringstream msg;
        msg << context << " mismatch";
        throw TestFailure(msg.str());
    }
}

void require_true(bool value, std::string_view context) {
    if (!value) {
        throw TestFailure(std::string{context});
    }
}

void require_false(bool value, std::string_view context) {
    if (value) {
        throw TestFailure(std::string{context});
    }
}

void require_contains(std::string_view text, std::string_view needle, std::string_view context) {
    if (text.find(needle) == std::string_view::npos) {
        std::ostringstream msg;
        msg << context << " expected " << text << " to contain " << needle;
        throw TestFailure(msg.str());
    }
}

std::filesystem::path unique_temp_home(std::string_view test_name) {
    auto path = std::filesystem::temp_directory_path()
        / ("boxing-trainer-cpp-test-" + std::string{test_name} + "-"
           + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
#ifdef _WIN32
    _putenv_s("USERPROFILE", path.string().c_str());
    _putenv_s("HOME", path.string().c_str());
#else
    setenv("HOME", path.string().c_str(), 1);
    setenv("USERPROFILE", path.string().c_str(), 1);
#endif
    return path;
}

std::vector<std::string> combination_names(const std::vector<model::CombinationItem>& combinations) {
    std::vector<std::string> out;
    out.reserve(combinations.size());
    for (const auto& item : combinations) {
        out.push_back(item.combination.get().description);
    }
    return out;
}

combo::Combination make_combo(
    std::string description,
    combo::CombinationFeatures features = {},
    std::string comment = {},
    std::map<std::string, std::string> values = {}) {
    return combo::Combination{
        .description = std::move(description),
        .features = features,
        .url = {},
        .position = {},
        .comment = std::move(comment),
        .values = std::move(values),
    };
}

template <std::ranges::input_range Range>
std::set<std::string> as_set(Range&& items) {
    std::set<std::string> out;
    for (const auto& item : items) {
        out.emplace(std::string{item});
    }
    return out;
}

std::set<std::string> as_set(std::initializer_list<std::string_view> items) {
    std::set<std::string> out;
    for (const auto item : items) {
        out.emplace(item);
    }
    return out;
}

void test_parse_combination() {
    const std::map<std::string, std::string> values{{"url", "https://example.com"}};
    const auto got = combo::parse_combination("1-1-2-step_back-2; 1m20s;", 0, values);

    require_equal(got.description, std::string{"1-1-2-step_back-2"}, "description");
    require_true(got.features.is_long, "long feature");
    require_true(got.features.has_defense, "defense feature");
    require_false(got.features.is_feint, "faint feature");
    require_false(got.features.targets_body, "body feature");
    require_false(got.features.is_counter, "counter feature");
    require_equal(got.url, std::string{"https://example.com"}, "url");
    require_equal(got.position, std::string{"1m20s"}, "position");
    require_equal(got.comment, std::string{}, "comment");
    require_equal(got.values.at("lineNo"), std::string{"0"}, "lineNo value");
}

void test_parse_errors_and_variables() {
    std::set<std::string> vars;
    std::map<std::string, std::string> values;

    require_true(combo::parse_comment(""), "blank comment");
    require_true(combo::parse_comment("# comment"), "hash comment");
    require_false(combo::parse_comment("not comment"), "non-comment");

    require_true(combo::parse_var_defs("var foo", vars), "var definition");
    require_true(vars.contains("foo"), "var set contains foo");
    require_false(combo::parse_var_defs("variable foo", vars), "invalid var definition");

    require_true(combo::parse_var_assignments("foo=123", vars, values), "var assignment");
    require_equal(values.at("foo"), std::string{"123"}, "var value");
    require_false(combo::parse_var_assignments("bar=123", vars, values), "unknown assignment");

    try {
        (void)combo::parse_combination("1-2;", 7, values);
        throw TestFailure("expected field count error");
    } catch (const std::runtime_error& err) {
        require_contains(err.what(), "expect 3 elements", "field count error");
    }

    try {
        (void)combo::parse_combination("1-2; 1m20s;", 7, values);
        throw TestFailure("expected missing URL error");
    } catch (const std::runtime_error& err) {
        require_contains(err.what(), "missing at line 7 `url` value", "missing URL error");
    }
}

void test_feature_extraction_and_youtube_timestamps() {
    require_true(combo::extract_long("f1 jab"), "f1 long");
    require_true(combo::extract_long("2-cross"), "2 long");
    require_true(combo::extract_long("long_3 hook"), "long_3 long");
    require_true(combo::extract_long("setup attack"), "setup long");
    require_false(combo::extract_long("setup close"), "setup close short");
    require_false(combo::extract_long("unknown"), "unknown short");

    require_true(combo::extract_defense("slip and counter"), "slip defense");
    require_true(combo::extract_defense("step back"), "step defense");
    require_false(combo::extract_defense("movement only"), "movement defense");

    require_true(combo::extract_faint("f1-2-3"), "f1 faint");
    require_true(combo::extract_faint("f2-3-2"), "f2 faint");
    require_false(combo::extract_faint("no f"), "no faint");
    require_true(combo::extract_body("1b-2-3"), "1b body");
    require_true(combo::extract_body("6b-5-3"), "6b body");
    require_false(combo::extract_body("face punch"), "no body");
    require_true(combo::extract_counter("COUNTER 1-2-3"), "counter");
    require_false(combo::extract_counter("1-2-3"), "not counter");

    require_equal(
        combo::create_url_with_location("https://youtube.com/watch?v=abc", "1m20s", 7),
        std::string{"https://youtube.com/watch?v=abc&t=1m20s"},
        "youtube minute timestamp");
    require_equal(
        combo::create_url_with_location("https://youtube.com/watch?v=abc", "59s", 7),
        std::string{"https://youtube.com/watch?v=abc&t=59s"},
        "youtube second timestamp");
    require_equal(
        combo::create_url_with_location("https://example.com/video", "90s", 7),
        std::string{"https://example.com/video"},
        "non-youtube unchanged");

    try {
        (void)combo::create_url_with_location("https://youtube.com/watch?v=abc", "90s", 7);
        throw TestFailure("expected invalid timestamp error");
    } catch (const std::runtime_error& err) {
        require_contains(err.what(), "invalid timestamp at line 7", "timestamp error");
    }
}

void test_load_embedded_data_copy() {
    const auto data = combo::load_data();
    require_true(data.size() > 100, "loaded data size");
    require_false(data.front().description.empty(), "first loaded description");
    require_true(data.front().values.contains("author"), "first loaded values include author");
}

void test_enums_and_match() {
    require_equal(model::to_string(model::FilterSelection::all), std::string{"All"}, "All string");
    require_equal(model::to_string(model::FilterSelection::yes), std::string{"Yes"}, "Yes string");
    require_equal(model::to_string(model::FilterSelection::no), std::string{"No"}, "No string");
    require_equal(model::selection_from_string("Yes"), model::FilterSelection::yes, "selection Yes");
    require_equal(model::selection_from_string("No"), model::FilterSelection::no, "selection No");
    require_equal(model::selection_from_string("invalid"), model::FilterSelection::all, "selection invalid");
    require_equal(model::to_string(model::SortOrder::in_order), std::string{"InOrder"}, "order InOrder");
    require_equal(model::to_string(model::SortOrder::random), std::string{"Random"}, "order Random");
    require_equal(model::order_from_string("Random"), model::SortOrder::random, "order parse random");
    require_equal(model::order_from_string("invalid"), model::SortOrder::in_order, "order parse invalid");

    require_true(model::matches_filter(model::FilterSelection::all, true), "all true");
    require_true(model::matches_filter(model::FilterSelection::all, false), "all false");
    require_true(model::matches_filter(model::FilterSelection::yes, true), "yes true");
    require_false(model::matches_filter(model::FilterSelection::yes, false), "yes false");
    require_false(model::matches_filter(model::FilterSelection::no, true), "no true");
    require_true(model::matches_filter(model::FilterSelection::no, false), "no false");
}

void test_main_model_empty_and_header() {
    unique_temp_home("main-empty");
    model::MainModel empty;

    require_equal(empty.combination_name(), std::string{"None"}, "empty name");
    require_equal(empty.comment(), std::string{}, "empty comment");
    require_true(empty.values() == nullptr, "empty values");
    try {
        (void)empty.current_combination();
        throw TestFailure("expected current combination error");
    } catch (const std::runtime_error& err) {
        require_contains(err.what(), "no current combination", "current combination error");
    }

    model::MainModel m({make_combo("combo-1", {}, "work the jab")});
    m.add_favorite("combo-1");
    require_contains(m.header_label(), "★", "favorite header");
    require_contains(m.header_label(), "work the jab", "header comment");
}

void test_filters_and_favorite_flag() {
    unique_temp_home("filters-favorites");
    model::MainModel m({
        make_combo("fav-combo", {.is_long = true, .is_feint = true, .is_counter = true}),
        make_combo("other-combo", {.is_long = true, .is_feint = true, .is_counter = true}),
    });
    m.add_favorite("fav-combo");

    auto selection = m.selection();
    selection.long_range = model::FilterSelection::yes;
    selection.defense = model::FilterSelection::no;
    selection.body = model::FilterSelection::no;
    selection.counter = model::FilterSelection::yes;
    selection.favorites = model::FilterSelection::yes;
    m.set_selection(selection);

    require_equal(combination_names(m.combinations()), std::vector<std::string>{"fav-combo"}, "favorite filtered names");
    require_true(m.combinations().front().is_favorite, "favorite flag carried");
}

void test_search_behavior() {
    unique_temp_home("search");
    model::MainModel m({
        make_combo("jab cross"),
        make_combo("slip roll", {}, "work the angle step"),
        make_combo("lead hook"),
    });

    m.set_search_query("cross");
    require_equal(combination_names(m.combinations()), std::vector<std::string>{"jab cross"}, "search description");
    m.set_search_query("angle");
    require_equal(combination_names(m.combinations()), std::vector<std::string>{"slip roll"}, "search comment");

    model::MainModel ranking({
        make_combo("abcxxxx"),
        make_combo("abcdef"),
        make_combo("nomatch"),
    });
    ranking.set_search_query("abcdef");
    require_equal(
        combination_names(ranking.combinations()),
        std::vector<std::string>{"abcdef", "abcxxxx"},
        "search ranking score");

    model::MainModel tie({
        make_combo("abcde-one"),
        make_combo("abcde"),
        make_combo("abcde-two"),
    });
    tie.set_search_query("abcde");
    require_equal(
        combination_names(tie.combinations()),
        std::vector<std::string>{"abcde", "abcde-one", "abcde-two"},
        "search tie ranking");

    model::MainModel no_match({make_combo("abcde"), make_combo("fghij")});
    no_match.set(1);
    no_match.set_search_query("zzzzz");
    require_true(no_match.combinations().empty(), "search no match empty");
    require_equal(no_match.current(), 0, "search no match clamps current");
}

void test_search_composes_with_filters_and_does_not_persist() {
    unique_temp_home("search-filter");
    model::MainModel m({
        make_combo("abcde-long", {.is_long = true}),
        make_combo("abcde-short", {.is_long = false}),
        make_combo("other-long", {.is_long = true}),
    });

    auto selection = m.selection();
    selection.long_range = model::FilterSelection::yes;
    m.set_selection(selection);
    require_equal(
        combination_names(m.combinations()),
        std::vector<std::string>{"abcde-long", "other-long"},
        "filter before search");

    m.set_search_query("abcde");
    require_equal(combination_names(m.combinations()), std::vector<std::string>{"abcde-long"}, "search with filter");
    m.set_search_query("");
    require_equal(
        combination_names(m.combinations()),
        std::vector<std::string>{"abcde-long", "other-long"},
        "clear search preserves filter");

    unique_temp_home("search-not-persisted");
    model::MainModel search_state({make_combo("abcdef")});
    search_state.set_search_query("abcdef");
    require_equal(model::load_state(), model::AppState{}, "search not persisted");
}

void test_navigation_and_state() {
    unique_temp_home("navigation");
    model::MainModel m({make_combo("c1"), make_combo("c2"), make_combo("c3")});

    m.next();
    require_equal(m.current(), 1, "next current 1");
    require_equal(m.number(), 2, "next number 2");
    m.next();
    require_equal(m.current(), 2, "next current 2");
    m.next();
    require_equal(m.current(), 0, "next wraps");
    m.previous();
    require_equal(m.current(), 2, "previous wraps");
    m.set(0);
    require_equal(m.current(), 0, "set current");
    require_equal(m.number(), 6, "set increments number");
    m.reset();
    require_equal(m.current(), 0, "reset current");
    require_equal(m.number(), 1, "reset number");

    auto selection = m.selection();
    selection.long_range = model::FilterSelection::yes;
    m.set_selection(selection);
    const auto state = model::load_state();
    require_equal(state.selection.long_range, model::FilterSelection::yes, "selection persisted");
}

void test_favorite_persistence_and_toggle() {
    const auto home = unique_temp_home("favorites");
    model::save_favorites({"zeta", "alpha", "hook"});
    const auto favorites_path = home / ".boxing-trainer" / "favorites";
    std::ifstream input(favorites_path);
    const std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    require_equal(content, std::string{"alpha\nhook\nzeta"}, "favorites sorted content");

    {
        std::ofstream output(favorites_path, std::ios::trunc);
        output << "  jab-cross  \n\nhook\njab-cross\n";
    }
    require_equal(model::load_favorites(), std::set<std::string>({"hook", "jab-cross"}), "favorites load trims dedupes");

    unique_temp_home("favorite-toggle");
    model::MainModel m({make_combo("jab-cross")});
    m.toggle_favorite();
    require_true(m.is_favorite("jab-cross"), "favorite after toggle");
    require_contains(m.combinations().front().label(), "★", "favorite label");
    require_true(model::load_favorites().contains("jab-cross"), "favorite persisted");
    m.toggle_favorite();
    require_false(m.is_favorite("jab-cross"), "favorite removed");
    require_false(m.combinations().front().label().find("★") != std::string::npos, "favorite marker removed");

    model::MainModel empty;
    empty.toggle_favorite();
}

void test_state_persistence() {
    unique_temp_home("state");
    const model::AppState want{
        .current = 2,
        .number = 9,
        .sort_order = model::SortOrder::random,
        .seed = 12345,
        .selection = model::MainViewSelection{
            .long_range = model::FilterSelection::yes,
            .defense = model::FilterSelection::no,
            .feint = model::FilterSelection::all,
            .body = model::FilterSelection::yes,
            .counter = model::FilterSelection::no,
            .favorites = model::FilterSelection::yes,
        },
    };
    model::save_state(want);
    require_equal(model::load_state(), want, "state roundtrip");

    const auto home = unique_temp_home("state-invalid");
    const auto path = home / ".boxing-trainer" / "state-2.json";
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream output(path);
        output << "{";
    }
    try {
        (void)model::load_state();
        throw TestFailure("expected invalid JSON state error");
    } catch (const std::runtime_error& err) {
        require_contains(err.what(), "unmarshal state file", "invalid state error");
    }
}

void test_load_restores_state_and_clamps() {
    unique_temp_home("load-state");
    model::save_state(model::AppState{
        .current = 5,
        .number = 12,
        .sort_order = model::SortOrder::in_order,
        .selection = model::MainViewSelection{.long_range = model::FilterSelection::yes},
    });

    model::MainModel m({
        make_combo("long", {.is_long = true}),
        make_combo("short", {.is_long = false}),
    });
    m.load();

    require_equal(m.current(), 0, "load clamps current");
    require_equal(m.number(), 12, "load number");
    require_equal(m.selection().long_range, model::FilterSelection::yes, "load selection");
    require_equal(combination_names(m.combinations()), std::vector<std::string>{"long"}, "load filters");
}

void test_random_order_deterministic() {
    unique_temp_home("random-order");
    model::MainModel m1({make_combo("c1"), make_combo("c2"), make_combo("c3"), make_combo("c4")});
    model::MainModel m2({make_combo("c1"), make_combo("c2"), make_combo("c3"), make_combo("c4")});
    m1.reset_in_random_order(42);
    m2.reset_in_random_order(42);
    require_equal(m1.order(), m2.order(), "same seed order");

    m1.reset_in_order();
    require_equal(m1.order(), std::vector<int>{0, 1, 2, 3}, "reset in order");
    require_equal(m1.sort_order(), model::SortOrder::in_order, "reset sort order");
    require_equal(m1.seed(), std::uint64_t{0}, "reset seed");
}

void test_defense_model() {
    using enum model::FilterSelection;

    const std::vector<std::pair<model::DefenseViewSelection, std::set<std::string>>> cases{
        {{all, all}, as_set({"1", "2", "3", "4", "5", "6", "8", "1b", "2b", "3b", "4b", "5b", "6b"})},
        {{yes, all}, as_set({"1", "2", "3", "1b", "2b", "8"})},
        {{no, all}, as_set({"3", "4", "5", "6", "3b", "4b", "5b", "6b"})},
        {{all, yes}, as_set({"1b", "2b", "3b", "4b", "5b", "6b"})},
        {{all, no}, as_set({"1", "2", "3", "4", "5", "6", "8"})},
        {{yes, yes}, as_set({"1b", "2b"})},
        {{no, yes}, as_set({"3b", "4b", "5b", "6b"})},
        {{yes, no}, as_set({"1", "2", "3", "8"})},
        {{no, no}, as_set({"4", "5", "6"})},
    };
    for (const auto& [selection, want] : cases) {
        require_equal(as_set(model::defense_candidates(selection)), want, "defense candidates");
    }

    model::DefenseModel m;
    require_equal(m.header_label(), std::string{"1. "}, "defense header");
    require_equal(std::string{m.combination_name()}, std::string{"1"}, "defense current");
    m.set_selection({no, no});
    for (int i = 0; i < 20; ++i) {
        m.next_attack();
        require_true(as_set({"4", "5", "6"}).contains(std::string{m.combination_name()}), "next attack selection");
    }
    require_equal(m.header_label(), std::string{"21. "}, "defense counter");
    require_false(m.pause(), "pause initially false");
    m.toggle_pause();
    require_true(m.pause(), "pause toggled true");
}

void test_video_url() {
    const std::vector<std::pair<std::string, bool>> cases{
        {"https://youtube.com/watch?v=abc", true},
        {"https://www.youtube.com/watch?v=abc", true},
        {"https://youtu.be/abc", true},
        {"https://vimeo.com/123", true},
        {"https://archive.org/details/boxing", true},
        {"http://youtube.com/watch?v=abc", true},
        {"ftp://youtube.com/watch?v=abc", false},
        {"/watch?v=abc", false},
        {"", false},
        {"https://notyoutube.example/watch", false},
        {"https://youtube.com.evil.example/watch", false},
        {"://youtube.com", false},
    };

    for (const auto& [url, want] : cases) {
        require_equal(view::video_url(url).has_value(), want, "video url " + url);
    }
}

void test_misc_ui_helpers() {
    require_equal(model::yes_no_to_bool(true), std::string{"yes"}, "yes bool");
    require_equal(model::yes_no_to_bool(false), std::string{"no"}, "no bool");
    for (int i = 0; i < 20; ++i) {
        const auto delay = model::next_delay();
        require_true(delay >= std::chrono::seconds{3}, "delay lower bound");
        require_true(delay < std::chrono::seconds{6}, "delay upper bound");
    }
}

} // namespace

int main() {
    const std::vector<TestCase> tests{
        {"parse_combination", test_parse_combination},
        {"parse_errors_and_variables", test_parse_errors_and_variables},
        {"feature_extraction_and_youtube_timestamps", test_feature_extraction_and_youtube_timestamps},
        {"load_embedded_data_copy", test_load_embedded_data_copy},
        {"enums_and_match", test_enums_and_match},
        {"main_model_empty_and_header", test_main_model_empty_and_header},
        {"filters_and_favorite_flag", test_filters_and_favorite_flag},
        {"search_behavior", test_search_behavior},
        {"search_composes_with_filters_and_does_not_persist", test_search_composes_with_filters_and_does_not_persist},
        {"navigation_and_state", test_navigation_and_state},
        {"favorite_persistence_and_toggle", test_favorite_persistence_and_toggle},
        {"state_persistence", test_state_persistence},
        {"load_restores_state_and_clamps", test_load_restores_state_and_clamps},
        {"random_order_deterministic", test_random_order_deterministic},
        {"defense_model", test_defense_model},
        {"video_url", test_video_url},
        {"misc_ui_helpers", test_misc_ui_helpers},
    };

    int failed = 0;
    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& err) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << err.what() << '\n';
        }
    }

    if (failed != 0) {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }
    std::cout << tests.size() << " test(s) passed\n";
    return 0;
}
