export module boxing_trainer.video_url;

import std;

export namespace boxing_trainer::view {

[[nodiscard]] std::optional<std::string> video_url(std::string_view raw);
[[nodiscard]] bool is_allowed_video_host(std::string_view host);

} // namespace boxing_trainer::view
