export module boxing_trainer.text;

import std;

export namespace boxing_trainer::text {

[[nodiscard]] std::string shell_quote(std::string_view value) {
    std::string out = "'";
    for (const auto ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('\'');
    return out;
}

} // namespace boxing_trainer::text
