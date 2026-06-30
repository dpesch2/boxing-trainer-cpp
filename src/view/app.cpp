module;

#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <wx/wx.h>

module boxing_trainer.app;

import std;
import boxing_trainer.combo;
import boxing_trainer.model;
import boxing_trainer.video_url;

namespace boxing_trainer::view {

namespace {

constexpr int window_width = 1200;
constexpr int window_height = 720;
constexpr int margin = 16;
constexpr int gap = 8;

[[nodiscard]] wxString to_wx(std::string_view text) {
    const std::string copy{text};
    return wxString::FromUTF8(copy.c_str());
}

[[nodiscard]] std::string from_wx(const wxString& text) {
    return text.ToStdString(wxConvUTF8);
}

[[nodiscard]] int filter_to_index(model::FilterSelection selection) noexcept {
    switch (selection) {
    case model::FilterSelection::yes:
        return 1;
    case model::FilterSelection::no:
        return 2;
    case model::FilterSelection::all:
    default:
        return 0;
    }
}

[[nodiscard]] model::FilterSelection index_to_filter(int index) noexcept {
    switch (index) {
    case 1:
        return model::FilterSelection::yes;
    case 2:
        return model::FilterSelection::no;
    case 0:
    default:
        return model::FilterSelection::all;
    }
}

[[nodiscard]] wxFont make_font(int point_size, bool bold = false) {
    auto font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    font.SetPointSize(point_size);
    font.SetWeight(bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
    return font;
}

} // namespace

class TrainerFrame final : public wxFrame {
public:
    explicit TrainerFrame(std::vector<combo::Combination> data)
        : wxFrame(nullptr, wxID_ANY, "Boxing Trainer", wxDefaultPosition, wxSize(window_width, window_height))
        , model_{std::move(data)}
        , defense_timer_{this} {
        root_ = new wxPanel(this);
        root_->SetWindowStyle(root_->GetWindowStyle() | wxWANTS_CHARS);

        Bind(wxEVT_CHAR_HOOK, &TrainerFrame::on_char_hook, this);
        Bind(wxEVT_TIMER, &TrainerFrame::on_defense_timer, this);

        rebuild();
        schedule_timer();
    }

private:
    using Callback = std::function<void()>;

    void rebuild() {
        header_label_ = nullptr;
        combination_label_ = nullptr;
        title_label_ = nullptr;
        search_ = nullptr;
        list_ = nullptr;

        root_->Freeze();
        root_->SetSizer(nullptr, false);
        root_->DestroyChildren();

        root_sizer_ = new wxBoxSizer(wxVERTICAL);
        root_->SetSizer(root_sizer_);

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

        root_->Layout();
        root_->Thaw();
        Layout();
    }

    void build_main_view() {
        add_main_header();
        add_main_buttons();
        add_search_control();
        add_main_filters();
        add_combination_list();
        update_main_view();
    }

    void add_main_buttons() {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        add_button(*row, "Next [->]", [this] {
            model_.main().next();
            rebuild();
        });
        add_button(*row, "Previous [<-]", [this] {
            model_.main().previous();
            rebuild();
        });
        add_button(*row, "Shuffle", [this] {
            model_.main().reset_in_random_order();
            rebuild();
        });
        add_button(*row, "In Order", [this] {
            model_.main().reset_in_order();
            rebuild();
        });
        add_button(*row, "Show [H]", [this] { show_current_combination_video(); });
        add_button(*row, "Info [I]", [this] {
            model_.set_active_view(model::ActiveView::info);
            rebuild();
        });
        add_button(*row, "Favorite [F]", [this] {
            model_.main().toggle_favorite();
            rebuild();
        });
        add_button(*row, "Defense [D]", [this] {
            model_.set_active_view(model::ActiveView::defense);
            rebuild();
        });
        add_button(*row, "Search [S]", [this] { toggle_search(); });

        root_sizer_->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    }

    void add_search_control() {
        if (!search_visible_) {
            return;
        }

        search_ = new wxSearchCtrl(root_, wxID_ANY, to_wx(model_.main().search_query()), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        search_->ShowCancelButton(true);
        search_->SetDescriptiveText("Search");
        search_->SetFocus();
        search_->SetInsertionPointEnd();
        search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            handle_action([this] {
                model_.main().set_search_query(from_wx(search_->GetValue()));
                update_main_view();
            });
        });
        search_->Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, [this](wxCommandEvent&) {
            handle_action([this] {
                search_->SetValue({});
                model_.main().set_search_query({});
                update_main_view();
            });
        });

        root_sizer_->Add(search_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    }

    void add_main_filters() {
        const auto selection = model_.main().selection();
        auto* grid = new wxFlexGridSizer(2, 3, gap, gap);
        for (int col = 0; col < 3; ++col) {
            grid->AddGrowableCol(col, 1);
        }

        add_filter_box(*grid, "Long", selection.long_range, [this](auto value) {
            auto selection = model_.main().selection();
            selection.long_range = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_filter_box(*grid, "Defense", selection.defense, [this](auto value) {
            auto selection = model_.main().selection();
            selection.defense = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_filter_box(*grid, "Faint", selection.feint, [this](auto value) {
            auto selection = model_.main().selection();
            selection.feint = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_filter_box(*grid, "Body", selection.body, [this](auto value) {
            auto selection = model_.main().selection();
            selection.body = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_filter_box(*grid, "Counter", selection.counter, [this](auto value) {
            auto selection = model_.main().selection();
            selection.counter = value;
            model_.main().set_selection(selection);
            rebuild();
        });
        add_filter_box(*grid, "Favorites", selection.favorites, [this](auto value) {
            auto selection = model_.main().selection();
            selection.favorites = value;
            model_.main().set_selection(selection);
            rebuild();
        });

        root_sizer_->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);
    }

    void add_combination_list() {
        list_ = new wxListBox(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, {}, wxLB_SINGLE | wxLB_NEEDED_SB);
        list_->SetFont(make_font(15));
        list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
            handle_action([this] {
                const auto selected = list_->GetSelection();
                if (selected != wxNOT_FOUND) {
                    model_.main().set(selected);
                    update_main_view();
                }
            });
        });
        root_sizer_->Add(list_, 1, wxEXPAND | wxALL, margin);
    }

    void build_info_view() {
        add_main_header();
        add_button(*root_sizer_, "Close [C]", [this] {
            model_.set_active_view(model::ActiveView::main);
            rebuild();
        }, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);

        auto* scroll = new wxScrolledWindow(root_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_SIMPLE);
        scroll->SetScrollRate(0, 10);
        auto* scroll_sizer = new wxBoxSizer(wxVERTICAL);
        scroll->SetSizer(scroll_sizer);

        try {
            const auto& current = model_.main().current_combination().combination.get();
            for (const auto& [key, value] : current.values) {
                add_scroll_text(*scroll, *scroll_sizer, std::format("{}: {}", key, value));
            }

            add_scroll_text(*scroll, *scroll_sizer, std::format("position: {}", current.position));

            const auto& features = current.features;
            const std::string distance = features.is_long ? "long" : "short";
            add_scroll_text(
                *scroll,
                *scroll_sizer,
                std::format(
                    "distance: {}, defense: {}, body: {}, faint: {}",
                    distance,
                    model::yes_no_to_bool(features.has_defense),
                    model::yes_no_to_bool(features.targets_body),
                    model::yes_no_to_bool(features.is_feint)));

            if (const auto url = video_url(current.url)) {
                auto* open = new wxButton(scroll, wxID_ANY, "Open video");
                open->Bind(wxEVT_BUTTON, [this, url](wxCommandEvent&) {
                    handle_action([url] { wxLaunchDefaultBrowser(to_wx(*url)); });
                });
                scroll_sizer->Add(open, 0, wxTOP, gap);
            }
        } catch (const std::exception&) {
        }

        root_sizer_->Add(scroll, 1, wxEXPAND | wxALL, margin);
    }

    void build_defense_view() {
        add_defense_header();

        auto* row = new wxBoxSizer(wxHORIZONTAL);
        add_button(*row, model_.defense().pause() ? "Start" : "Pause", [this] {
            model_.defense().toggle_pause();
            rebuild();
        });
        add_button(*row, "Close [C]", [this] {
            model_.set_active_view(model::ActiveView::main);
            rebuild();
        });
        root_sizer_->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, margin);

        const auto selection = model_.defense().selection();
        auto* grid = new wxFlexGridSizer(1, 2, gap, gap);
        grid->AddGrowableCol(0, 1);
        grid->AddGrowableCol(1, 1);

        add_filter_box(*grid, "Long", selection.long_range, [this](auto value) {
            auto selection = model_.defense().selection();
            selection.long_range = value;
            selection.body = model::FilterSelection::all;
            model_.defense().set_selection(selection);
            rebuild();
        });
        add_filter_box(*grid, "Body", selection.body, [this](auto value) {
            auto selection = model_.defense().selection();
            selection.body = value;
            selection.long_range = model::FilterSelection::all;
            model_.defense().set_selection(selection);
            rebuild();
        });

        root_sizer_->Add(grid, 0, wxEXPAND | wxALL, margin);
    }

    void add_main_header() {
        header_label_ = add_text(34);
        combination_label_ = add_text(56, true);
        title_label_ = add_text(16);
        update_main_header();
    }

    void add_defense_header() {
        header_label_ = add_text(34);
        combination_label_ = add_text(64, true);
        header_label_->SetLabel(to_wx(model_.defense().header_label()));
        combination_label_->SetLabel(to_wx(model_.defense().combination_name()));
    }

    wxStaticText* add_text(int point_size, bool bold = false) {
        auto* text = new wxStaticText(root_, wxID_ANY, {});
        text->SetFont(make_font(point_size, bold));
        root_sizer_->Add(text, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, root_sizer_->IsEmpty() ? margin : 2);
        return text;
    }

    void add_scroll_text(wxWindow& parent, wxSizer& sizer, std::string_view text) {
        auto* label = new wxStaticText(&parent, wxID_ANY, to_wx(text));
        label->SetFont(make_font(15));
        sizer.Add(label, 0, wxEXPAND | wxBOTTOM, gap);
    }

    void add_button(
        wxSizer& sizer,
        std::string_view text,
        Callback callback,
        int proportion = 1,
        int flags = wxEXPAND | wxRIGHT,
        int border = gap) {
        auto* button = new wxButton(root_, wxID_ANY, to_wx(text));
        button->Bind(wxEVT_BUTTON, [this, callback = std::move(callback)](wxCommandEvent&) {
            handle_action(callback);
        });
        sizer.Add(button, proportion, flags, border);
    }

    void add_filter_box(
        wxSizer& sizer,
        std::string_view label,
        model::FilterSelection selected,
        std::function<void(model::FilterSelection)> on_change) {
        wxArrayString choices;
        choices.Add("All");
        choices.Add("Yes");
        choices.Add("No");

        auto* box = new wxRadioBox(
            root_,
            wxID_ANY,
            to_wx(label),
            wxDefaultPosition,
            wxDefaultSize,
            choices,
            1,
            wxRA_SPECIFY_ROWS);
        box->SetSelection(filter_to_index(selected));
        box->Bind(wxEVT_RADIOBOX, [this, box, on_change = std::move(on_change)](wxCommandEvent&) {
            handle_action([&] { on_change(index_to_filter(box->GetSelection())); });
        });
        sizer.Add(box, 1, wxEXPAND);
    }

    void update_main_view() {
        update_main_header();
        update_combination_list();
        root_->Layout();
    }

    void update_main_header() {
        if (header_label_ == nullptr || combination_label_ == nullptr || title_label_ == nullptr) {
            return;
        }

        try {
            (void)model_.main().current_combination();
            header_label_->SetLabel(to_wx(model_.main().header_label()));
            combination_label_->SetLabel(to_wx(model_.main().combination_name()));
            title_label_->SetLabel(to_wx(model_.main().title()));
        } catch (const std::exception&) {
            header_label_->SetLabel("No combination selected");
            combination_label_->SetLabel({});
            title_label_->SetLabel({});
        }
    }

    void update_combination_list() {
        if (list_ == nullptr) {
            return;
        }

        list_->Freeze();
        list_->Clear();
        for (const auto& item : model_.main().combinations()) {
            list_->Append(to_wx(item.label()));
        }
        if (!model_.main().combinations().empty()) {
            list_->SetSelection(model_.main().current());
        }
        list_->Thaw();
    }

    void toggle_search() {
        search_visible_ = !search_visible_;
        if (!search_visible_) {
            model_.main().set_search_query("");
        }
        rebuild();
    }

    void show_current_combination_video() {
        const auto& current = model_.main().current_combination().combination.get();
        if (const auto url = video_url(current.url)) {
            if (!wxLaunchDefaultBrowser(to_wx(*url))) {
                throw std::runtime_error("could not open video URL");
            }
        }
    }

    void on_char_hook(wxKeyEvent& event) {
        if (search_has_focus()) {
            event.Skip();
            return;
        }

        try {
            const auto active = model_.active_view();
            const auto key = event.GetKeyCode();
            if (active == model::ActiveView::main && key == WXK_LEFT) {
                model_.main().previous();
                rebuild();
                return;
            }
            if (active == model::ActiveView::main && key == WXK_RIGHT) {
                model_.main().next();
                rebuild();
                return;
            }
            if (active == model::ActiveView::main && key == WXK_SPACE) {
                model_.main().toggle_favorite();
                rebuild();
                return;
            }

            const auto ch = key >= 0 && key <= 255
                ? static_cast<char>(std::toupper(static_cast<unsigned char>(key)))
                : '\0';
            if (active == model::ActiveView::main && ch == 'F') {
                model_.main().toggle_favorite();
                rebuild();
                return;
            }
            if (active == model::ActiveView::main && ch == 'S') {
                toggle_search();
                return;
            }
            if (active == model::ActiveView::main && ch == 'H') {
                show_current_combination_video();
                return;
            }
            if (ch == 'D') {
                model_.set_active_view(model::ActiveView::defense);
                rebuild();
                return;
            }
            if (ch == 'I') {
                model_.set_active_view(model::ActiveView::info);
                rebuild();
                return;
            }
            if (ch == 'C') {
                model_.set_active_view(model::ActiveView::main);
                rebuild();
                return;
            }
        } catch (const std::exception& err) {
            show_error(err);
            return;
        }

        event.Skip();
    }

    [[nodiscard]] bool search_has_focus() const {
        for (auto* focus = wxWindow::FindFocus(); focus != nullptr; focus = focus->GetParent()) {
            if (focus == search_) {
                return true;
            }
        }
        return false;
    }

    void schedule_timer() {
        defense_timer_.Start(static_cast<int>(model::next_delay().count()), wxTIMER_ONE_SHOT);
    }

    void on_defense_timer(wxTimerEvent&) {
        if (model_.active_view() == model::ActiveView::defense && !model_.defense().pause()) {
            model_.defense().next_attack();
            wxBell();
            rebuild();
        }
        schedule_timer();
    }

    template <typename Fn>
    void handle_action(Fn&& fn) {
        try {
            std::forward<Fn>(fn)();
        } catch (const std::exception& err) {
            show_error(err);
        }
    }

    void show_error(const std::exception& err) {
        wxMessageBox(to_wx(err.what()), "Boxing Trainer", wxOK | wxICON_ERROR, this);
    }

    model::Model model_;
    wxTimer defense_timer_;
    wxPanel* root_ = nullptr;
    wxBoxSizer* root_sizer_ = nullptr;
    wxStaticText* header_label_ = nullptr;
    wxStaticText* combination_label_ = nullptr;
    wxStaticText* title_label_ = nullptr;
    wxSearchCtrl* search_ = nullptr;
    wxListBox* list_ = nullptr;
    bool search_visible_ = false;
};

class TrainerApp final : public wxApp {
public:
    explicit TrainerApp(std::vector<combo::Combination> data)
        : data_{std::move(data)} {}

    bool OnInit() override {
        try {
            auto* frame = new TrainerFrame(std::move(data_));
            frame->Show(true);
            SetTopWindow(frame);
            return true;
        } catch (const std::exception& err) {
            wxMessageBox(to_wx(err.what()), "Boxing Trainer", wxOK | wxICON_ERROR);
            return false;
        }
    }

private:
    std::vector<combo::Combination> data_;
};

int run_desktop_app(int argc, char** argv) {
    try {
        auto data_result = combo::load_data();
        if (!data_result) {
            std::println(stderr, "Boxing Trainer: {}", data_result.error());
            return 1;
        }
        auto* app = new TrainerApp(std::move(data_result.value()));
        wxApp::SetInstance(app);

        int arg_count = argc;
        if (!wxEntryStart(arg_count, argv)) {
            wxApp::SetInstance(nullptr);
            delete app;
            return 1;
        }

        if (!wxTheApp->CallOnInit()) {
            wxTheApp->OnExit();
            wxEntryCleanup();
            return 1;
        }

        const auto result = wxTheApp->OnRun();
        wxTheApp->OnExit();
        wxEntryCleanup();
        return result;
    } catch (const std::exception& err) {
        std::println(stderr, "Boxing Trainer: {}", err.what());
        return 1;
    }
}

} // namespace boxing_trainer::view
