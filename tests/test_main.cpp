#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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
using Catch::Matchers::ContainsSubstring;

namespace {

void require_contains(std::string_view text, std::string_view needle) {
    REQUIRE_THAT(std::string{text}, ContainsSubstring(std::string{needle}));
}

template <typename Action>
void require_runtime_error_contains(Action action, std::string_view needle) {
    bool thrown = false;
    try {
        action();
    } catch (const std::runtime_error& err) {
        thrown = true;
        require_contains(err.what(), needle);
    }
    REQUIRE(thrown);
}

std::filesystem::path unique_temp_home(std::string_view test_name) {
    auto path = std::filesystem::temp_directory_path()
        / std::format(
            "boxing-trainer-cpp-test-{}-{}",
            test_name,
            std::chrono::steady_clock::now().time_since_epoch().count());
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
        out.emplace(item);
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

} // namespace

TEST_CASE("parser handles comments, variables, and field errors", "[combo]") {
    std::set<std::string> vars;
    std::map<std::string, std::string> values;

    CHECK(combo::parse_comment("   "));
    CHECK(combo::parse_comment(""));
    CHECK(combo::parse_comment("# comment"));
    CHECK_FALSE(combo::parse_comment("not comment"));

    CHECK_FALSE(combo::parse_var_defs("var ", vars));
    CHECK(combo::parse_var_defs("var foo", vars));
    CHECK(vars.contains("foo"));
    CHECK_FALSE(combo::parse_var_defs("variable foo", vars));

    CHECK(combo::parse_var_assignments("foo=123", vars, values));
    CHECK(values.at("foo") == "123");
    CHECK_FALSE(combo::parse_var_assignments("missing=1", vars, values));
    CHECK_FALSE(combo::parse_var_assignments("bar=123", vars, values));

    auto missing_fields = combo::parse_combination("just-a-description", 10, values);
    REQUIRE_FALSE(missing_fields.has_value());
    require_contains(missing_fields.error(), "expect 3 elements");

    auto short_row = combo::parse_combination("1-2;", 7, values);
    REQUIRE_FALSE(short_row.has_value());
    require_contains(short_row.error(), "expect 3 elements");

    auto missing_url = combo::parse_combination("1-2; 1m20s;", 7, values);
    REQUIRE_FALSE(missing_url.has_value());
    require_contains(missing_url.error(), "missing at line 7 `url` value");
}

TEST_CASE("parser creates combinations with derived features", "[combo]") {
    const std::map<std::string, std::string> values{{"url", "https://example.com"}};
    const auto got = combo::parse_combination("1-1-2-step_back-2; 1m20s;", 0, values);

    REQUIRE(got.has_value());
    const auto& result = got.value();
    CHECK(result.description == "1-1-2-step_back-2");
    CHECK(result.features.is_long);
    CHECK(result.features.has_defense);
    CHECK_FALSE(result.features.is_feint);
    CHECK_FALSE(result.features.targets_body);
    CHECK_FALSE(result.features.is_counter);
    CHECK(result.url == "https://example.com");
    CHECK(result.position == "1m20s");
    CHECK(result.comment.empty());
    CHECK(result.values.at("lineNo") == "0");
}

TEST_CASE("feature extraction and timestamp handling match the data format", "[combo]") {
    CHECK(combo::extract_long("f1 jab"));
    CHECK(combo::extract_long("2-cross"));
    CHECK(combo::extract_long("long_3 hook"));
    CHECK(combo::extract_long("setup attack"));
    CHECK_FALSE(combo::extract_long("setup close"));
    CHECK_FALSE(combo::extract_long("unknown"));

    CHECK(combo::extract_defense("slip and counter"));
    CHECK(combo::extract_defense("step back"));
    CHECK_FALSE(combo::extract_defense("movement only"));

    CHECK(combo::extract_faint("f1-2-3"));
    CHECK(combo::extract_faint("f2-3-2"));
    CHECK_FALSE(combo::extract_faint("no f"));

    CHECK(combo::extract_body("1b-2-3"));
    CHECK(combo::extract_body("6b-5-3"));
    CHECK_FALSE(combo::extract_body("face punch"));

    CHECK(combo::extract_counter("COUNTER 1-2-3"));
    CHECK_FALSE(combo::extract_counter("1-2-3"));

    const auto minute_url = combo::create_url_with_location("https://youtube.com/watch?v=abc", "1m20s", 7);
    REQUIRE(minute_url.has_value());
    CHECK(minute_url.value() == "https://youtube.com/watch?v=abc&t=1m20s");

    const auto second_url = combo::create_url_with_location("https://youtube.com/watch?v=abc", "59s", 7);
    REQUIRE(second_url.has_value());
    CHECK(second_url.value() == "https://youtube.com/watch?v=abc&t=59s");

    const auto non_youtube_url = combo::create_url_with_location("https://example.com/video", "90s", 7);
    REQUIRE(non_youtube_url.has_value());
    CHECK(non_youtube_url.value() == "https://example.com/video");

    const auto invalid_timestamp = combo::create_url_with_location("https://youtube.com/watch?v=abc", "90s", 7);
    REQUIRE_FALSE(invalid_timestamp.has_value());
    require_contains(invalid_timestamp.error(), "invalid timestamp at line 7");
}

TEST_CASE("embedded data loads", "[combo]") {
    const auto data_result = combo::load_data();
    REQUIRE(data_result.has_value());

    const auto& data = data_result.value();
    CHECK(data.size() > 100);
    CHECK_FALSE(data.front().description.empty());
    CHECK(data.front().values.contains("author"));
}

TEST_CASE("model enum conversion and filters are stable", "[model]") {
    CHECK(model::to_string(model::FilterSelection::all) == "All");
    CHECK(model::to_string(model::FilterSelection::yes) == "Yes");
    CHECK(model::to_string(model::FilterSelection::no) == "No");
    CHECK(model::selection_from_string("Yes") == model::FilterSelection::yes);
    CHECK(model::selection_from_string("No") == model::FilterSelection::no);
    CHECK(model::selection_from_string("invalid") == model::FilterSelection::all);
    CHECK(model::to_string(model::SortOrder::in_order) == "InOrder");
    CHECK(model::to_string(model::SortOrder::random) == "Random");
    CHECK(model::order_from_string("Random") == model::SortOrder::random);
    CHECK(model::order_from_string("invalid") == model::SortOrder::in_order);

    CHECK(model::matches_filter(model::FilterSelection::all, true));
    CHECK(model::matches_filter(model::FilterSelection::all, false));
    CHECK(model::matches_filter(model::FilterSelection::yes, true));
    CHECK_FALSE(model::matches_filter(model::FilterSelection::yes, false));
    CHECK_FALSE(model::matches_filter(model::FilterSelection::no, true));
    CHECK(model::matches_filter(model::FilterSelection::no, false));
}

TEST_CASE("main model handles empty state and header text", "[model]") {
    unique_temp_home("main-empty");
    model::MainModel empty;

    CHECK(empty.combination_name() == "None");
    CHECK(empty.comment().empty());
    CHECK(empty.values() == nullptr);
    require_runtime_error_contains(
        [&empty] { (void)empty.current_combination(); },
        "no current combination");

    model::MainModel m({make_combo("combo-1", {}, "work the jab")});
    m.add_favorite("combo-1");
    require_contains(m.header_label(), "★");
    require_contains(m.header_label(), "work the jab");
}

TEST_CASE("main model filters include favorite flags", "[model]") {
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

    CHECK(combination_names(m.combinations()) == std::vector<std::string>{"fav-combo"});
    REQUIRE_FALSE(m.combinations().empty());
    CHECK(m.combinations().front().is_favorite);
}

TEST_CASE("search matches descriptions, comments, and ranking", "[model]") {
    unique_temp_home("search");
    model::MainModel m({
        make_combo("jab cross"),
        make_combo("slip roll", {}, "work the angle step"),
        make_combo("lead hook"),
    });

    m.set_search_query("cross");
    CHECK(combination_names(m.combinations()) == std::vector<std::string>{"jab cross"});
    m.set_search_query("angle");
    CHECK(combination_names(m.combinations()) == std::vector<std::string>{"slip roll"});

    model::MainModel ranking({
        make_combo("abcxxxx"),
        make_combo("abcdef"),
        make_combo("nomatch"),
    });
    ranking.set_search_query("abcdef");
    CHECK(combination_names(ranking.combinations()) == std::vector<std::string>{"abcdef", "abcxxxx"});

    model::MainModel tie({
        make_combo("abcde-one"),
        make_combo("abcde"),
        make_combo("abcde-two"),
    });
    tie.set_search_query("abcde");
    CHECK(combination_names(tie.combinations()) == std::vector<std::string>{"abcde", "abcde-one", "abcde-two"});

    model::MainModel no_match({make_combo("abcde"), make_combo("fghij")});
    no_match.set(1);
    no_match.set_search_query("zzzzz");
    CHECK(no_match.combinations().empty());
    CHECK(no_match.current() == 0);
}

TEST_CASE("search composes with filters and is not persisted", "[model]") {
    unique_temp_home("search-filter");
    model::MainModel m({
        make_combo("abcde-long", {.is_long = true}),
        make_combo("abcde-short", {.is_long = false}),
        make_combo("other-long", {.is_long = true}),
    });

    auto selection = m.selection();
    selection.long_range = model::FilterSelection::yes;
    m.set_selection(selection);
    CHECK(combination_names(m.combinations()) == std::vector<std::string>{"abcde-long", "other-long"});

    m.set_search_query("abcde");
    CHECK(combination_names(m.combinations()) == std::vector<std::string>{"abcde-long"});
    m.set_search_query("");
    CHECK(combination_names(m.combinations()) == std::vector<std::string>{"abcde-long", "other-long"});

    unique_temp_home("search-not-persisted");
    model::MainModel search_state({make_combo("abcdef")});
    search_state.set_search_query("abcdef");
    CHECK(model::load_state() == model::AppState{});
}

TEST_CASE("navigation and persisted state work together", "[model]") {
    unique_temp_home("navigation");
    model::MainModel m({make_combo("c1"), make_combo("c2"), make_combo("c3")});

    m.next();
    CHECK(m.current() == 1);
    CHECK(m.number() == 2);
    m.next();
    CHECK(m.current() == 2);
    m.next();
    CHECK(m.current() == 0);
    m.previous();
    CHECK(m.current() == 2);
    m.set(0);
    CHECK(m.current() == 0);
    CHECK(m.number() == 6);
    m.reset();
    CHECK(m.current() == 0);
    CHECK(m.number() == 1);

    auto selection = m.selection();
    selection.long_range = model::FilterSelection::yes;
    m.set_selection(selection);
    const auto state = model::load_state();
    CHECK(state.selection.long_range == model::FilterSelection::yes);
}

TEST_CASE("favorites persist sorted values and toggle on the model", "[model]") {
    const auto home = unique_temp_home("favorites");
    model::save_favorites({"zeta", "alpha", "hook"});
    const auto favorites_path = home / ".boxing-trainer" / "favorites";
    std::ifstream input(favorites_path);
    const std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    CHECK(content == "alpha\nhook\nzeta");

    {
        std::ofstream output(favorites_path, std::ios::trunc);
        constexpr std::string_view duplicate_content =
            "  jab-cross  \n"
            "\n"
            "hook\n"
            "jab-cross\n";
        output.write(duplicate_content.data(), static_cast<std::streamsize>(duplicate_content.size()));
    }
    CHECK(model::load_favorites() == std::set<std::string>({"hook", "jab-cross"}));

    unique_temp_home("favorite-toggle");
    model::MainModel m({make_combo("jab-cross")});
    m.toggle_favorite();
    CHECK(m.is_favorite("jab-cross"));
    require_contains(m.combinations().front().label(), "★");
    CHECK(model::load_favorites().contains("jab-cross"));
    m.toggle_favorite();
    CHECK_FALSE(m.is_favorite("jab-cross"));
    CHECK(m.combinations().front().label().find("★") == std::string::npos);

    model::MainModel empty;
    empty.toggle_favorite();
}

TEST_CASE("state roundtrips and rejects invalid JSON", "[model]") {
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
    CHECK(model::load_state() == want);

    const auto home = unique_temp_home("state-invalid");
    const auto path = home / ".boxing-trainer" / "state-2.json";
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream output(path);
        constexpr std::string_view content = "{";
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    require_runtime_error_contains(
        [] { (void)model::load_state(); },
        "unmarshal state file");
}

TEST_CASE("loading state restores filters and clamps current index", "[model]") {
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

    CHECK(m.current() == 0);
    CHECK(m.number() == 12);
    CHECK(m.selection().long_range == model::FilterSelection::yes);
    CHECK(combination_names(m.combinations()) == std::vector<std::string>{"long"});
}

TEST_CASE("random order is deterministic for a fixed seed", "[model]") {
    unique_temp_home("random-order");
    model::MainModel m1({make_combo("c1"), make_combo("c2"), make_combo("c3"), make_combo("c4")});
    model::MainModel m2({make_combo("c1"), make_combo("c2"), make_combo("c3"), make_combo("c4")});
    m1.reset_in_random_order(42);
    m2.reset_in_random_order(42);
    CHECK(m1.order() == m2.order());

    m1.reset_in_order();
    CHECK(m1.order() == std::vector<int>{0, 1, 2, 3});
    CHECK(m1.sort_order() == model::SortOrder::in_order);
    CHECK(m1.seed() == std::uint64_t{0});
}

TEST_CASE("defense model chooses candidates from the selected range", "[model]") {
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
        CHECK(as_set(model::defense_candidates(selection)) == want);
    }

    model::DefenseModel m;
    CHECK(m.header_label() == "1. ");
    CHECK(m.combination_name() == "1");
    m.set_selection({no, no});
    for (int i = 0; i < 20; ++i) {
        m.next_attack();
        CHECK(as_set({"4", "5", "6"}).contains(std::string{m.combination_name()}));
    }
    CHECK(m.header_label() == "21. ");
    CHECK_FALSE(m.pause());
    m.toggle_pause();
    CHECK(m.pause());
}

TEST_CASE("video URL allowlist accepts supported video hosts only", "[video]") {
    static constexpr std::array<std::pair<std::string_view, bool>, 12> cases{{
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
    }};

    for (const auto [url, want] : cases) {
        CAPTURE(url);
        CHECK(view::video_url(url).has_value() == want);
    }
}

TEST_CASE("miscellaneous UI helpers keep simple contracts", "[model]") {
    CHECK(model::yes_no_to_bool(true) == "yes");
    CHECK(model::yes_no_to_bool(false) == "no");
    for (int i = 0; i < 20; ++i) {
        const auto delay = model::next_delay();
        CHECK(delay >= std::chrono::seconds{3});
        CHECK(delay < std::chrono::seconds{6});
    }
}
