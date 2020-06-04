#include "parser/parser.hpp"

#include "parser/lexertk.hpp"

#include <iostream>

using token = lexertk::token;

char_type tok_char_type(token const& tok)
{
    switch (tok.type) {
    case token::e_none:
    case token::e_eof: return char_type::none;
    case token::e_error:
    case token::e_err_symbol:
    case token::e_err_number:
    case token::e_err_string:
    case token::e_err_sfunc: return char_type::error;
    case token::e_number: return char_type::number;
    case token::e_symbol: {
        if (tok.value == "on")
            return char_type::keyword;
        else
            return char_type::var;
    }
    case token::e_string: return char_type::str;
    case token::e_assign:
    case token::e_shr:
    case token::e_shl:
    case token::e_lte:
    case token::e_ne:
    case token::e_gte:
    case token::e_lt:
    case token::e_gt:
    case token::e_eq:
    case token::e_comma:
    case token::e_add:
    case token::e_sub:
    case token::e_div:
    case token::e_mul:
    case token::e_mod:
    case token::e_pow:
    case token::e_colon: return char_type::op;
    case token::e_rbracket:
    case token::e_lbracket:
    case token::e_rsqrbracket:
    case token::e_lsqrbracket:
    case token::e_rcrlbracket:
    case token::e_lcrlbracket: return char_type::brack;
    case token::e_comment: return char_type::comment;
    }
    return char_type::none;
}

struct parser::pimpl {
    sound_defs const&  sounds;
    std::string const& buffer;
    parser&            result;
    int                curr_line = 0;

    lexertk::generator lexer;

    size_t tok_ind = 0;

    int curr_ind = -1;

    bool buffer_lexed = false;

    void update_res_pos(size_t failpos)
    {
        int l = 1;
        int c = 0;
        for (size_t i = 0; i < failpos; i++) {
            if (buffer[i] == '\n') {
                l++;
                c = 0;
            }
            c++;
        }
        result.line = l;
        result.col  = c;
    }

    pimpl(sound_defs const& sounds, parser& prsng)
        : sounds(sounds)
        , buffer(prsng.buffer)
        , result(prsng)
    {
    }

    void parse()
    {
        result.error.clear();
        result.col  = -1;
        result.line = -1;
        result.tree.clear();
        result.char_types.resize(result.buffer.size());

        bool const lexok = lex();

        update_char_types();

        if (lexok) {
            update_ast();
        }
    }

    private:
    bool lex()
    {
        lexer.clear();
        if (!lexer.process(buffer)) {
            if (lexer.size() > 0) {
                tok_ind = lexer.size() - 1;
                return err("Parse failure");
            }
            return false;
        }
        return true;
    }

    void update_char_types()
    {
        auto& ctypes = result.char_types;
        for (size_t i = 0; i < lexer.size(); i++) {
            auto const& tok = lexer[i];
            auto const  ct  = tok_char_type(tok);
            auto        pos = ct == char_type::str || ct == char_type::comment ? tok.position - 1
                                                                        : tok.position;
            auto end = ct == char_type::str ? tok.end + 1 : tok.end;
            std::fill(ctypes.begin() + int(pos),
                      end >= ctypes.size() ? ctypes.end() : ctypes.begin() + int(end), ct);
        }
    }
    bool update_ast()
    {
        for (tok_ind = 0; tok_ind < lexer.size(); tok_ind++) {
            auto const& tok = lexer[tok_ind];
            if (parse_comment(tok, result.tree))
                continue;
            if (parse_play(tok, result.tree))
                continue;
            if (parse_on_measure(tok, result.tree))
                continue;
            if (parse_on_beat(tok, result.tree))
                continue;
            if (parse_seq(tok, result.tree))
                continue;
            if (parse_affect(tok, result.tree))
                continue;
        }
        return result.error.empty();
    }

    bool is_last() const { return tok_ind + 1 >= lexer.size(); }

    bool err(std::string const& e)
    {
        result.error       = e;
        auto       tok     = lexer[tok_ind];
        auto const tok_pos = tok.position;
        if (e.empty() && tok.type == token::e_error) {
            result.error = tok.value;
        }
        update_res_pos(tok_pos);
        tok_ind = lexer.size();
        return false;
    }

    int get_id() const { return int(tok_ind * 10); }

    token const& next_token()
    {
        ++tok_ind;
        return lexer[tok_ind];
    }

    token const* peek_token()
    {
        if (is_last())
            return nullptr;
        return &lexer[tok_ind + 1];
    }

    bool parse_comment(token const& tok, ast& a)
    {
        if (tok.type != token::e_comment) {
            return false;
        }
        a.push_back(comment { tok.value });
        return true;
    }

    bool parse_seq(token const& tok, ast& a)
    {
        if (tok.type != token::e_symbol || tok.value != "seq") {
            return false;
        }
        if (is_last()) {
            return err("expected number of measure after 'seq'");
        }
        auto const& num_tok = next_token();
        if (num_tok.type != token::e_number) {
            return err("expected number of measure after 'seq', found " + num_tok.value);
        }
        auto&     st  = a.emplace_back(sequence { std::stoi(num_tok.value) });
        sequence& seq = std::get<sequence>(st);
        while (!is_last()) {
            if (peek_token()->type == token::e_number) {
                return true;
            }
            auto const& ntok = next_token();
            if (parse_comment(ntok, seq.statements))
                continue;
            if (parse_play(ntok, seq.statements))
                continue;
            if (parse_on_beat(ntok, seq.statements))
                continue;
            if (parse_seq(ntok, a))
                break;
            if (parse_on_measure(ntok, a))
                break;
            if (parse_affect(ntok, seq.statements))
                continue;
        }

        return true;
    }

    bool parse_on_beat(token const& tok, ast& a)
    {
        if (tok.type != token::e_symbol || tok.value != "on") {
            return false;
        }
        if (is_last()) {
            return err("expected number after 'on'");
        }
        auto const& num_tok = next_token();
        if (num_tok.type != token::e_number) {
            return err("expected number after 'on', found " + num_tok.value);
        }
        auto&    st = a.emplace_back(on_beat { std::stoi(num_tok.value) });
        on_beat& ob = std::get<on_beat>(st);
        if (is_last()) {
            return err("expected something after on beat");
        }
        if (peek_token()->type == token::e_number) {
            ob.sub_beat = std::stoi(next_token().value);
            if (is_last() || next_token().type != token::e_div) {
                return err("expected '/' after sub beat number");
            }
            if (is_last() || peek_token()->type != token::e_number) {
                return err("expected number after 'subbeat/'");
            }
            ob.nb_sub = std::stoi(next_token().value);
        }
        while (!is_last()) {
            if (peek_token()->type == token::e_number) {
                return true;
            }
            auto const& ntok = next_token();
            if (parse_comment(ntok, ob.statements))
                continue;
            if (parse_play(ntok, ob.statements))
                continue;
            if (parse_on_beat(ntok, a))
                break;
            if (parse_affect(ntok, ob.statements))
                continue;
        }

        return true;
    }

    bool parse_on_measure(token const& tok, ast& a)
    {
        if (tok.type != token::e_number) {
            return false;
        }
        if (is_last()) {
            return err("expected ':' or '-' after measure number");
        }
        int const m1 = std::stoi(tok.value);
        int       m2 = m1;
        ast       suba;
        if (peek_token()->type == token::e_sub) {
            next_token();
            if (is_last()) {
                return err("expected number or ':' after '-'");
            }
            if (peek_token()->type == token::e_number) {
                m2 = std::stoi(next_token().value);
                if (m2 < m1) {
                    return err(std::to_string(m2) + " inferior to " + std::to_string(m1));
                }
            }
            else {
                m2 = -1;
            }
        }
        if (next_token().type != token::e_colon) {
            return err("expected ':' after measure number");
        }
        auto&            st = a.emplace_back(between_measure { m1, m2 });
        between_measure& bm = std::get<between_measure>(st);
        while (!is_last()) {
            auto const& ntok = next_token();
            if (parse_comment(ntok, bm.statements))
                continue;
            if (parse_play(ntok, bm.statements))
                continue;
            if (parse_on_beat(ntok, bm.statements))
                continue;
            if (parse_seq(ntok, bm.statements))
                continue;
            if (parse_on_measure(ntok, a))
                break;
            if (parse_affect(ntok, bm.statements))
                continue;
        }
        return true;
    }
    template<typename Sound>
    bool parse_sound_args(int id, std::string const& name, play_sound& ps)
    {
        ps.sound = Sound { id, name };
        Sound& s = std::get<Sound>(ps.sound);
        if (is_last() || peek_token()->type != token::e_lbracket) {
            return true;
        }
        next_token();
        while (!is_last()) {
            auto const& ntok = next_token();
            if (ntok.type == token::e_rbracket) {
                return true;
            }
            int param = -1;
            if (ntok.type != token::e_symbol) {
                return err("invalid param name '" + ntok.value + "'");
            }
            for (int i = 0; i < Sound::_nb_params; i++) {
                if (Sound::param_name(Sound::param(i)) == ntok.value) {
                    param = i;
                    break;
                }
            }
            if (param == -1) {
                return err("invalid param name '" + ntok.value + "'");
            }
            if (is_last() || next_token().type != token::e_colon) {
                return err("expected ':' after param '" + ntok.value + "'");
            }
            if (is_last()) {
                return err("expected value after param '" + ntok.value + "'");
            }
            auto const& nt = next_token();
            if (nt.type != token::e_number) {
                return err("expected number after param '" + ntok.value + "'");
            }
            s.params[Sound::param(param)] = std::stof(nt.value);
        }
        return err("expected ')'");
    }
    bool parse_play(token const& tok, ast& a)
    {
        if (tok.type != token::e_string) {
            return false;
        }
        auto&       st = a.emplace_back(play_sound {});
        play_sound& ps = std::get<play_sound>(st);

        for (size_t i = 0; i < sounds.synths.size(); i++) {
            if (sounds.synths[i] == tok.value) {
                return parse_sound_args<synth>(int(i), tok.value, ps);
            }
        }
        for (size_t i = 0; i < sounds.samples.size(); i++) {
            if (sounds.samples[i] == tok.value) {
                return parse_sound_args<sample>(int(i), tok.value, ps);
            }
        }
        return err("no synth or sample found with name '" + tok.value + "'");
    }

    bool parse_affect(token const& tok, ast& a)
    {
        if (tok.type != token::e_symbol) {
            return false;
        }
        if (is_last()) {
            return err("expected value after " + tok.value);
        }
        auto const& nt = next_token();
        if (nt.type != token::e_number) {
            return err("expected number after " + tok.value);
        }
        a.push_back({ affect { tok.value, std::stof(nt.value) } });
        return true;
    }

    private:
    pimpl(pimpl const&) = delete;
    pimpl& operator=(pimpl const&) = delete;
};

parser::parser(sound_defs const& sounds)
{
    _p = std::make_unique<pimpl>(sounds, *this);
}

parser::~parser()
{
}

bool parser::parse()
{
    _p->parse();
    if (error.empty()) {
        std::cout << "Parse ok" << std::endl;
        return true;
    }
    else {
        std::cerr << "Parse failed: " << line << ":" << col << " " << error << std::endl;
        return false;
    }
}