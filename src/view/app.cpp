module;

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Round_Button.H>
#include <FL/Fl_Scroll.H>
#include <FL/fl_ask.H>

module boxing_trainer.app;

import std;
import boxing_trainer.combo;
import boxing_trainer.model;
import boxing_trainer.text;
import boxing_trainer.video_url;

namespace boxing_trainer::view {

namespace {

constexpr int window_width = 1200;
constexpr int window_height = 720;
constexpr int margin = 10;

void set_widget_label(Fl_Widget& widget, std::string_view text) {
    widget.copy_label(std::string{text}.c_str());
}

void open_url(std::string_view url) {
#ifdef _WIN32
    const auto command = "start \"\" " + text::shell_quote(url);
#elif defined(__APPLE__)
    const auto command = "open " + text::shell_quote(url);
#else
    const auto command = "xdg-open " + text::shell_quote(url);
#endif
    (void)std::system(command.c_str());
}

} // namespace

class TrainerWindow final : public Fl_Double_Window {
public:
    explicit TrainerWindow(std::vector<combo::Combination> data)
        : Fl_Double_Window(window_width, window_height, "Boxing Trainer")
        , model_{std::move(data)} {
        resizable(this);
        rebuild();
        schedule_timer();
    }

    ~TrainerWindow() override {
        Fl::remove_timeout(timer_callback, this);
    }

    int handle(int event) override {
        if (event != FL_KEYDOWN) {
            return Fl_Double_Window::handle(event);
        }

        const auto key = Fl::event_key();
        const auto text = Fl::event_text() != nullptr ? std::string{Fl::event_text()} : std::string{};
        const auto active = model_.active_view();

        try {
            if (active == model::ActiveView::main && key == FL_Left) {
                model_.main().previous();
                rebuild();
                return 1;
            }
            if (active == model::ActiveView::main && key == FL_Right) {
                model_.main().next();
                rebuild();
                return 1;
            }
            if (active == model::ActiveView::main && key == ' ') {
                model_.main().toggle_favorite();
                rebuild();
                return 1;
            }

            const auto ch = text.empty() ? '\0' : static_cast<char>(std::toupper(static_cast<unsigned char>(text.front())));
            if (active == model::ActiveView::main && ch == 'F') {
                model_.main().toggle_favorite();
                rebuild();
                return 1;
            }
            if (active == model::ActiveView::main && ch == 'S') {
                toggle_search();
                return 1;
            }
            if (active == model::ActiveView::main && ch == 'H') {
                show_current_combination_video();
                return 1;
            }
            if (ch == 'D') {
                model_.set_active_view(model::ActiveView::defense);
                rebuild();
                return 1;
            }
            if (ch == 'I') {
                model_.set_active_view(model::ActiveView::info);
                rebuild();
                return 1;
            }
            if (ch == 'C') {
                model_.set_active_view(model::ActiveView::main);
                rebuild();
                return 1;
            }
        } catch (const std::exception& err) {
            show_error(err);
            return 1;
        }

        return Fl_Double_Window::handle(event);
    }

private:
    using Callback = std::function<void()>;

    struct SearchFocusState {
        int position = 0;
        int mark = 0;
    };

    void rebuild() {
        rebuilding_ = true;
        focus_after_rebuild_ = nullptr;
        callbacks_.clear();
        clear();
        begin();

        switch (model_.active_view()) {
        case model::ActiveView::info:
            build_info_view();
            break;
        case model::ActiveView::defense:
            build_defense_view();
            break;
        case model::ActiveView::main:
        default:
            build_main_view();
            break;
        }

        end();
        if (focus_after_rebuild_ != nullptr) {
            focus_after_rebuild_->take_focus();
        }
        redraw();
        search_focus_after_rebuild_.reset();
        focus_after_rebuild_ = nullptr;
        rebuilding_ = false;
    }

    void build_main_view() {
        add_main_header();

        const auto button_count = 9;
        const auto gap = 4;
        const auto button_y = 180;
        const auto button_h = 34;
        const auto button_w = (w() - (2 * margin) - ((button_count - 1) * gap)) / button_count;
        int x = margin;

        add_button(x, button_y, button_w, button_h, "Next [->]", [this] {
            model_.main().next();
            rebuild();
        });
        x += button_w + gap;
        add_button(x, button_y, button_w, button_h, "Previous [<-]", [this] {
            model_.main().previous();
            rebuild();
        });
        x += button_w + gap;
        add_button(x, button_y, button_w, button_h, "Shuffle", [this] {
            model_.main().reset_in_random_order();
            rebuild();
        });
        x += button_w + gap;
        add_button(x, button_y, button_w, button_h, "In Order", [this] {
            model_.main().reset_in_order();
            rebuild();
        });
        x += button_w + gap;
        add_button(x, button_y, button_w, button_h, "Show [H]", [this] { show_current_combination_video(); });
        x += button_w + gap;
        add_button(x, button_y, button_w, button_h, "Info [I]", [this] {
            model_.set_active_view(model::ActiveView::info);
            rebuild();
        });
        x += button_w + gap;
        add_button(x, button_y, button_w, button_h, "Favorite [F]", [this] {
            model_.main().toggle_favorite();
            rebuild();
        });
        x += button_w + gap;
        add_button(x, button_y, button_w, button_h, "Defense [D]", [this] {
            model_.set_active_view(model::ActiveView::defense);
            rebuild();
        });
        x += button_w + gap;
        add_button(x, button_y, button_w, button_h, "Search [S]", [this] { toggle_search(); });

        int y = button_y + button_h + 8;
        if (search_visible_) {
            auto* input = new Fl_Input(margin, y, w() - (2 * margin), 30);
            input->copy_label("Search");
            input->value(std::string{model_.main().search_query()}.c_str());
            input->when(FL_WHEN_CHANGED);
            bind(*input, [this, input] {
                if (!rebuilding_) {
                    search_focus_after_rebuild_ = SearchFocusState{
                        .position = input->position(),
                        .mark = input->mark(),
                    };
                    model_.main().set_search_query(input->value());
                    rebuild();
                }
            });
            restore_search_focus(*input);
            y += 38;
        }

        const auto selection = model_.main().selection();
        const auto filter_w = (w() - (2 * margin) - (2 * gap)) / 3;
        add_selection_group(margin, y, filter_w, 54, "Long", selection.long_range, [this](auto value) {
            auto selection = model_.main().selection();
            selection.long_range = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_selection_group(margin + filter_w + gap, y, filter_w, 54, "Defense", selection.defense, [this](auto value) {
            auto selection = model_.main().selection();
            selection.defense = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_selection_group(margin + ((filter_w + gap) * 2), y, filter_w, 54, "Faint", selection.feint, [this](auto value) {
            auto selection = model_.main().selection();
            selection.feint = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        y += 58;
        add_selection_group(margin, y, filter_w, 54, "Body", selection.body, [this](auto value) {
            auto selection = model_.main().selection();
            selection.body = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_selection_group(margin + filter_w + gap, y, filter_w, 54, "Counter", selection.counter, [this](auto value) {
            auto selection = model_.main().selection();
            selection.counter = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_selection_group(margin + ((filter_w + gap) * 2), y, filter_w, 54, "Favorites", selection.favorites, [this](auto value) {
            auto selection = model_.main().selection();
            selection.favorites = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        y += 64;

        auto* list = new Fl_Hold_Browser(margin, y, w() - (2 * margin), h() - y - margin);
        int index = 1;
        for (const auto& item : model_.main().combinations()) {
            list->add(item.label().c_str());
            if (index - 1 == model_.main().current()) {
                list->select(index);
            }
            ++index;
        }
        bind(*list, [this, list] {
            if (!rebuilding_ && list->value() > 0) {
                model_.main().set(list->value() - 1);
                rebuild();
            }
        });
    }

    void build_info_view() {
        add_main_header();
        add_button(margin, 180, w() - (2 * margin), 34, "Close [C]", [this] {
            model_.set_active_view(model::ActiveView::main);
            rebuild();
        });

        auto* scroll = new Fl_Scroll(margin, 224, w() - (2 * margin), h() - 234);
        scroll->box(FL_DOWN_FRAME);
        scroll->begin();

        int y = 10;
        try {
            const auto& current = model_.main().current_combination().combination.get();
            for (const auto& [key, value] : current.values) {
                add_scroll_text(*scroll, y, key + ": " + value);
                y += 28;
            }

            add_scroll_text(*scroll, y, "position: " + current.position);
            y += 28;

            const auto& features = current.features;
            const std::string distance = features.is_long ? "long" : "short";
            add_scroll_text(
                *scroll,
                y,
                "distance: " + distance
                    + ", defense: " + model::yes_no_to_bool(features.has_defense)
                    + ", body: " + model::yes_no_to_bool(features.targets_body)
                    + ", faint: " + model::yes_no_to_bool(features.is_feint));
            y += 34;

            if (const auto url = video_url(current.url)) {
                auto* open = new Fl_Button(10, y, 140, 30, "Open video");
                bind(*open, [url] { open_url(*url); });
                scroll->add(open);
            }
        } catch (const std::exception&) {
        }

        scroll->end();
    }

    void build_defense_view() {
        add_defense_header();
        add_button(margin, 180, (w() - (2 * margin) - 4) / 2, 34, model_.defense().pause() ? "Start" : "Pause", [this] {
            model_.defense().toggle_pause();
            rebuild();
        });
        add_button(margin + ((w() - (2 * margin) - 4) / 2) + 4, 180, (w() - (2 * margin) - 4) / 2, 34, "Close [C]", [this] {
            model_.set_active_view(model::ActiveView::main);
            rebuild();
        });

        const auto selection = model_.defense().selection();
        const auto filter_w = (w() - (2 * margin) - 4) / 2;
        add_selection_group(margin, 230, filter_w, 54, "Long", selection.long_range, [this](auto value) {
            auto selection = model_.defense().selection();
            selection.long_range = value;
            selection.body = model::FilterSelection::all;
            model_.defense().set_selection(selection);
            rebuild();
        });
        add_selection_group(margin + filter_w + 4, 230, filter_w, 54, "Body", selection.body, [this](auto value) {
            auto selection = model_.defense().selection();
            selection.body = value;
            selection.long_range = model::FilterSelection::all;
            model_.defense().set_selection(selection);
            rebuild();
        });
    }

    void add_main_header() {
        try {
            (void)model_.main().current_combination();
            add_text(margin, 10, w() - (2 * margin), 46, model_.main().header_label(), 34);
            add_text(margin, 56, w() - (2 * margin), 82, model_.main().combination_name(), 56, true);
            add_text(margin, 146, w() - (2 * margin), 28, model_.main().title(), 16);
        } catch (const std::exception&) {
            add_text(margin, 10, w() - (2 * margin), 46, "No combination selected", 34);
            add_text(margin, 56, w() - (2 * margin), 82, "", 56, true);
            add_text(margin, 146, w() - (2 * margin), 28, "", 16);
        }
    }

    void add_defense_header() {
        add_text(margin, 10, w() - (2 * margin), 46, model_.defense().header_label(), 34);
        add_text(margin, 56, w() - (2 * margin), 100, model_.defense().combination_name(), 64, true);
    }

    void add_text(int x, int y, int width, int height, std::string_view text, int size, bool bold = false) {
        auto* box = new Fl_Box(x, y, width, height);
        set_widget_label(*box, text);
        box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        box->labelsize(size);
        if (bold) {
            box->labelfont(FL_BOLD);
        }
    }

    void add_scroll_text(Fl_Scroll& scroll, int y, std::string_view text) {
        auto* box = new Fl_Box(10, y, scroll.w() - 30, 24);
        set_widget_label(*box, text);
        box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
        box->labelsize(15);
        scroll.add(box);
    }

    void add_button(int x, int y, int width, int height, std::string_view text, Callback callback) {
        auto* button = new Fl_Button(x, y, width, height);
        set_widget_label(*button, text);
        bind(*button, std::move(callback));
    }

    void add_selection_group(
        int x,
        int y,
        int width,
        int height,
        std::string_view label,
        model::FilterSelection selected,
        std::function<void(model::FilterSelection)> on_change) {
        auto* group = new Fl_Group(x, y, width, height);
        group->box(FL_THIN_UP_FRAME);
        group->begin();

        auto* title = new Fl_Box(x + 8, y + 6, 80, 20);
        set_widget_label(*title, std::string{label} + ":");
        title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        const auto button_w = 68;
        const auto start_x = x + 86;
        add_radio(start_x, y + 6, button_w, 22, "All", selected == model::FilterSelection::all, [on_change] {
            on_change(model::FilterSelection::all);
        });
        add_radio(start_x + button_w, y + 6, button_w, 22, "Yes", selected == model::FilterSelection::yes, [on_change] {
            on_change(model::FilterSelection::yes);
        });
        add_radio(start_x + (button_w * 2), y + 6, button_w, 22, "No", selected == model::FilterSelection::no, [on_change] {
            on_change(model::FilterSelection::no);
        });

        group->end();
    }

    void add_radio(int x, int y, int width, int height, std::string_view label, bool selected, Callback callback) {
        auto* radio = new Fl_Round_Button(x, y, width, height);
        set_widget_label(*radio, label);
        radio->type(FL_RADIO_BUTTON);
        radio->value(selected ? 1 : 0);
        bind(*radio, std::move(callback));
    }

    void bind(Fl_Widget& widget, Callback callback) {
        callbacks_.push_back(std::make_unique<Callback>(std::move(callback)));
        widget.callback(widget_callback, callbacks_.back().get());
    }

    static void widget_callback(Fl_Widget*, void* data) {
        (*static_cast<Callback*>(data))();
    }

    void toggle_search() {
        search_visible_ = !search_visible_;
        if (!search_visible_) {
            model_.main().set_search_query("");
        } else {
            const auto end_position = static_cast<int>(model_.main().search_query().size());
            search_focus_after_rebuild_ = SearchFocusState{
                .position = end_position,
                .mark = end_position,
            };
        }
        rebuild();
    }

    void restore_search_focus(Fl_Input& input) {
        if (!search_focus_after_rebuild_) {
            return;
        }

        const auto max_position = static_cast<int>(std::string_view{input.value()}.size());
        const auto position = std::clamp(search_focus_after_rebuild_->position, 0, max_position);
        const auto mark = std::clamp(search_focus_after_rebuild_->mark, 0, max_position);
        input.position(position, mark);
        focus_after_rebuild_ = &input;
    }

    void show_current_combination_video() {
        try {
            const auto& current = model_.main().current_combination().combination.get();
            if (const auto url = video_url(current.url)) {
                open_url(*url);
            }
        } catch (const std::exception& err) {
            show_error(err);
        }
    }

    void show_error(const std::exception& err) {
        fl_alert("%s", err.what());
    }

    void schedule_timer() {
        Fl::add_timeout(model::next_delay().count() / 1000.0, timer_callback, this);
    }

    static void timer_callback(void* data) {
        auto& self = *static_cast<TrainerWindow*>(data);
        if (self.model_.active_view() == model::ActiveView::defense && !self.model_.defense().pause()) {
            self.model_.defense().next_attack();
            fl_beep(FL_BEEP_DEFAULT);
            self.rebuild();
        }
        self.schedule_timer();
    }

    model::Model model_;
    bool search_visible_ = false;
    bool rebuilding_ = false;
    std::optional<SearchFocusState> search_focus_after_rebuild_;
    Fl_Input* focus_after_rebuild_ = nullptr;
    std::vector<std::unique_ptr<Callback>> callbacks_;
};

int run_desktop_app(int argc, char** argv) {
    (void)argc;
    (void)argv;

    try {
        auto window = std::make_unique<TrainerWindow>(combo::load_data());
        window->show();
        return Fl::run();
    } catch (const std::exception& err) {
        fl_alert("%s", err.what());
        return 1;
    }
}

} // namespace boxing_trainer::view
