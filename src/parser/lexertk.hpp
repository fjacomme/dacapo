/*
 *****************************************************************
 *                 Simple C++ Lexer Toolkit Library              *
 *                                                               *
 * Author: Arash Partow (2001)                                   *
 * URL: http://www.partow.net/programming/lexertk/index.html     *
 *                                                               *
 * Copyright notice:                                             *
 * Free use of the Simple C++ Lexer Toolkit Library is permitted *
 * under the guidelines and in accordance with the MIT License.  *
 * http://www.opensource.org/licenses/MIT                        *
 *                                                               *
 *                                                               *
 * The lexer will tokenize input against the following BNF:      *
 *                                                               *
 * expression ::= term { +|- term }                              *
 * term       ::= (symbol | factor) {operator symbol | factor}   *
 * factor     ::= symbol | ( '(' {-} expression ')' )            *
 * symbol     ::= number | gensymb | string                      *
 * gensymb    ::= alphabet {alphabet | digit}                    *
 * string     ::= '"' {alphabet | digit | operator } '"'         *
 * operator   ::= * | / | % | ^ | < | > | <= | >= | << | >> !=   *
 * alphabet   ::= a | b | .. | z | A | B | .. | Z                *
 * digit      ::= 0 | 1 | 2 | 3 | 4 | 5 | 6 |  7 | 8 | 9         *
 * sign       ::= + | -                                          *
 * edef       ::= e | E                                          *
 * decimal    ::= {digit} (digit [.] | [.] digit) {digit}        *
 * exponent   ::= edef [sign] digit {digit}                      *
 * real       ::= [sign] decimal [exponent]                      *
 * integer    ::= [sign] {digit}                                 *
 * number     ::= real | integer                                 *
 *                                                               *
 *                                                               *
 * Note: This lexer has been taken from the ExprTk Library.      *
 *                                                               *
 *****************************************************************
 */

#ifndef INCLUDE_LEXERTK_HPP
#define INCLUDE_LEXERTK_HPP

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <limits>
#include <map>
#include <set>
#include <stack>
#include <stdexcept>
#include <string>

namespace lexertk {

namespace details {

inline bool is_whitespace(const char c)
{
    return (' ' == c) || ('\n' == c) || ('\r' == c) || ('\t' == c) || ('\b' == c) || ('\v' == c)
        || ('\f' == c);
}

inline bool is_operator_char(const char c)
{
    return ('+' == c) || ('-' == c) || ('*' == c) || ('/' == c) || ('^' == c) || ('<' == c)
        || ('>' == c) || ('=' == c) || (',' == c) || ('!' == c) || ('(' == c) || (')' == c)
        || ('[' == c) || (']' == c) || ('{' == c) || ('}' == c) || ('%' == c) || (':' == c)
        || ('?' == c) || ('&' == c) || ('|' == c) || (';' == c);
}

inline bool is_letter(const char c)
{
    return (('a' <= c) && (c <= 'z')) || (('A' <= c) && (c <= 'Z'));
}

inline bool is_digit(const char c)
{
    return ('0' <= c) && (c <= '9');
}

inline bool is_letter_or_digit(const char c)
{
    return is_letter(c) || is_digit(c);
}

inline bool is_left_bracket(const char c)
{
    return ('(' == c) || ('[' == c) || ('{' == c);
}

inline bool is_right_bracket(const char c)
{
    return (')' == c) || (']' == c) || ('}' == c);
}

inline bool is_bracket(const char c)
{
    return is_left_bracket(c) || is_right_bracket(c);
}

inline bool is_sign(const char c)
{
    return ('+' == c) || ('-' == c);
}

inline bool is_invalid(const char c)
{
    return !is_whitespace(c) && !is_operator_char(c) && !is_letter(c) && !is_digit(c) && ('.' != c)
        && ('_' != c) && ('$' != c) && ('~' != c) && ('\'' != c);
}

inline bool imatch(const char c1, const char c2)
{
    return std::tolower(c1) == std::tolower(c2);
}

inline bool imatch(const std::string& s1, const std::string& s2)
{
    if (s1.size() == s2.size()) {
        for (std::size_t i = 0; i < s1.size(); ++i) {
            if (std::tolower(s1[i]) != std::tolower(s2[i])) {
                return false;
            }
        }

        return true;
    }

    return false;
}

struct ilesscompare {
    inline bool operator()(const std::string& s1, const std::string& s2) const
    {
        const std::size_t length = std::min(s1.size(), s2.size());

        for (std::size_t i = 0; i < length; ++i) {
            if (std::tolower(s1[i]) > std::tolower(s2[i]))
                return false;
            else if (std::tolower(s1[i]) < std::tolower(s2[i]))
                return true;
        }

        return s1.size() < s2.size();
    }
};

inline void cleanup_escapes(std::string& s)
{
    typedef std::string::iterator str_itr_t;

    str_itr_t itr1 = s.begin();
    str_itr_t itr2 = s.begin();
    str_itr_t end  = s.end();

    std::size_t removal_count = 0;

    while (end != itr1) {
        if ('\\' == (*itr1)) {
            ++removal_count;

            if (end == ++itr1)
                break;
            else if ('\\' != (*itr1)) {
                switch (*itr1) {
                case 'n': (*itr1) = '\n'; break;
                case 'r': (*itr1) = '\r'; break;
                case 't': (*itr1) = '\t'; break;
                }

                continue;
            }
        }

        if (itr1 != itr2) {
            (*itr2) = (*itr1);
        }

        ++itr1;
        ++itr2;
    }

    s.resize(s.size() - removal_count);
}
} // namespace details

struct token {
    enum token_type {
        e_none        = 0,
        e_error       = 1,
        e_err_symbol  = 2,
        e_err_number  = 3,
        e_err_string  = 4,
        e_err_sfunc   = 5,
        e_eof         = 6,
        e_number      = 7,
        e_symbol      = 8,
        e_string      = 9,
        e_assign      = 10,
        e_shr         = 11,
        e_shl         = 12,
        e_lte         = 13,
        e_ne          = 14,
        e_gte         = 15,
        e_lt          = '<',
        e_gt          = '>',
        e_eq          = '=',
        e_rbracket    = ')',
        e_lbracket    = '(',
        e_rsqrbracket = ']',
        e_lsqrbracket = '[',
        e_rcrlbracket = '}',
        e_lcrlbracket = '{',
        e_comma       = ',',
        e_add         = '+',
        e_sub         = '-',
        e_div         = '/',
        e_mul         = '*',
        e_mod         = '%',
        e_pow         = '^',
        e_colon       = ':',
        e_comment     = 99,
    };

    token()
        : type(e_none)
        , value("")
        , position(std::numeric_limits<std::size_t>::max())
        , end(std::numeric_limits<std::size_t>::max())
    {
    }

    void clear()
    {
        type     = e_none;
        value    = "";
        position = std::numeric_limits<std::size_t>::max();
        end      = std::numeric_limits<std::size_t>::max();
    }

    template<typename Iterator>
    inline token& set_operator(const token_type tt,
                               const Iterator   begin,
                               const Iterator   end,
                               const Iterator   base_begin = Iterator(0))
    {
        type = tt;
        value.assign(begin, end);
        if (base_begin) {
            position  = (size_t)std::distance(base_begin, begin);
            this->end = position + value.size();
        }
        return *this;
    }

    template<typename Iterator>
    inline token& set_symbol(const Iterator begin,
                             const Iterator end,
                             const Iterator base_begin = Iterator(0))
    {
        type = e_symbol;
        value.assign(begin, end);
        if (base_begin) {
            position  = (size_t)std::distance(base_begin, begin);
            this->end = position + value.size();
        }
        return *this;
    }

    template<typename Iterator>
    inline token& set_numeric(const Iterator begin,
                              const Iterator end,
                              const Iterator base_begin = Iterator(0))
    {
        type = e_number;
        value.assign(begin, end);
        if (base_begin) {
            position  = (size_t)std::distance(base_begin, begin);
            this->end = position + value.size();
        }
        return *this;
    }

    template<typename Iterator>
    inline token& set_string(const Iterator begin,
                             const Iterator end,
                             const Iterator base_begin = Iterator(0))
    {
        type = e_string;
        value.assign(begin, end);
        if (base_begin) {
            position  = (size_t)std::distance(base_begin, begin);
            this->end = position + value.size();
        }
        return *this;
    }

    inline token& set_string(const std::string& s, const std::size_t p)
    {
        type     = e_string;
        value    = s;
        position = p;
        end      = position + value.size();
        return *this;
    }

    template<typename Iterator>
    inline token& set_error(const token_type et,
                            const Iterator   begin,
                            const Iterator   end,
                            const Iterator   base_begin = Iterator(0))
    {
        if ((e_error == et) || (e_err_symbol == et) || (e_err_number == et)
            || (e_err_string == et)) {
            type = e_error;
        }
        else
            type = e_error;

        value.assign(begin, end);

        if (base_begin) {
            position  = (size_t)std::distance(base_begin, begin);
            this->end = position + value.size();
        }

        return *this;
    }

    static inline std::string to_str(token_type t)
    {
        switch (t) {
        case e_none: return "NONE";
        case e_error: return "ERROR";
        case e_err_symbol: return "ERROR_SYMBOL";
        case e_err_number: return "ERROR_NUMBER";
        case e_err_string: return "ERROR_STRING";
        case e_err_sfunc: return "ERROR_SFUNC";
        case e_eof: return "EOF";
        case e_number: return "NUMBER";
        case e_symbol: return "SYMBOL";
        case e_string: return "STRING";
        case e_assign: return ":=";
        case e_shr: return ">>";
        case e_shl: return "<<";
        case e_lte: return "<=";
        case e_ne: return "!=";
        case e_gte: return ">=";
        case e_lt: return "<";
        case e_gt: return ">";
        case e_eq: return "=";
        case e_rbracket: return ")";
        case e_lbracket: return "(";
        case e_rsqrbracket: return "]";
        case e_lsqrbracket: return "[";
        case e_rcrlbracket: return "}";
        case e_lcrlbracket: return "{";
        case e_comma: return ",";
        case e_add: return "+";
        case e_sub: return "-";
        case e_div: return "/";
        case e_mul: return "*";
        case e_mod: return "%";
        case e_pow: return "^";
        case e_colon: return ":";
        case e_comment: return "COMMENT";
        default: return "UNKNOWN";
        }
    }

    inline bool is_error() const
    {
        return ((e_error == type) || (e_err_symbol == type) || (e_err_number == type)
                || (e_err_string == type));
    }

    token_type  type;
    std::string value;
    std::size_t position;
    std::size_t end;
};

class generator {
    public:
    typedef token                         token_t;
    typedef std::deque<token_t>           token_list_t;
    typedef std::deque<token_t>::iterator token_list_itr_t;

    generator()
        : base_itr_(0)
        , s_itr_(0)
        , s_end_(0)
    {
        clear();
    }

    inline void clear()
    {
        base_itr_ = 0;
        s_itr_    = 0;
        s_end_    = 0;
        token_list_.clear();
        token_itr_       = token_list_.end();
        store_token_itr_ = token_list_.end();
    }

    inline bool process(const std::string& str)
    {
        base_itr_ = str.data();
        s_itr_    = str.data();
        s_end_    = str.data() + str.size();

        eof_token_.set_operator(token_t::e_eof, s_end_, s_end_, base_itr_);
        token_list_.clear();

        while (!is_end(s_itr_)) {
            scan_token();

            if (token_list_.empty())
                return true;
            else if (token_list_.back().is_error()) {
                return false;
            }
        }
        return true;
    }

    inline bool empty() const { return token_list_.empty(); }

    inline std::size_t size() const { return token_list_.size(); }

    inline void begin()
    {
        token_itr_       = token_list_.begin();
        store_token_itr_ = token_list_.begin();
    }

    inline void store() { store_token_itr_ = token_itr_; }

    inline void restore() { token_itr_ = store_token_itr_; }

    inline token_t& next_token()
    {
        if (token_list_.end() != token_itr_) {
            return *token_itr_++;
        }
        else
            return eof_token_;
    }

    inline token_t& peek_next_token()
    {
        if (token_list_.end() != token_itr_) {
            return *token_itr_;
        }
        else
            return eof_token_;
    }

    inline token_t& operator[](const std::size_t& index)
    {
        if (index < token_list_.size())
            return token_list_[index];
        else
            return eof_token_;
    }

    inline token_t operator[](const std::size_t& index) const
    {
        if (index < token_list_.size())
            return token_list_[index];
        else
            return eof_token_;
    }

    inline bool finished() const { return (token_list_.end() == token_itr_); }

    inline std::string remaining() const
    {
        if (finished())
            return "";
        else if (token_list_.begin() != token_itr_)
            return std::string(base_itr_ + (token_itr_ - 1)->position, s_end_);
        else
            return std::string(base_itr_ + token_itr_->position, s_end_);
    }

    private:
    inline bool is_end(const char* itr) { return (s_end_ == itr); }

    inline void skip_whitespace()
    {
        while (!is_end(s_itr_) && details::is_whitespace(*s_itr_)) {
            ++s_itr_;
        }
    }

    inline void scan_token()
    {
        skip_whitespace();

        if (is_end(s_itr_)) {
            return;
        }
        else if ('~' == *s_itr_) {
            scan_comments();
            return;
        }
        else if (details::is_operator_char(*s_itr_)) {
            scan_operator();
            return;
        }
        else if (details::is_letter(*s_itr_)) {
            scan_symbol();
            return;
        }
        else if (details::is_digit((*s_itr_)) || ('.' == (*s_itr_))) {
            scan_number();
            return;
        }
        else if ('\'' == (*s_itr_)) {
            scan_string();
            return;
        }
        else {
            token_t t;
            t.set_error(token::e_error, s_itr_, s_itr_ + 2, base_itr_);
            token_list_.push_back(t);
            ++s_itr_;
        }
    }

    inline void scan_operator()
    {
        token_t t;

        if (!is_end(s_itr_ + 1)) {
            token_t::token_type ttype = token_t::e_none;

            char c0 = s_itr_[0];
            char c1 = s_itr_[1];

            if ((c0 == '<') && (c1 == '='))
                ttype = token_t::e_lte;
            else if ((c0 == '>') && (c1 == '='))
                ttype = token_t::e_gte;
            else if ((c0 == '<') && (c1 == '>'))
                ttype = token_t::e_ne;
            else if ((c0 == '!') && (c1 == '='))
                ttype = token_t::e_ne;
            else if ((c0 == '=') && (c1 == '='))
                ttype = token_t::e_eq;
            else if ((c0 == ':') && (c1 == '='))
                ttype = token_t::e_assign;
            else if ((c0 == '<') && (c1 == '<'))
                ttype = token_t::e_shl;
            else if ((c0 == '>') && (c1 == '>'))
                ttype = token_t::e_shr;

            if (token_t::e_none != ttype) {
                t.set_operator(ttype, s_itr_, s_itr_ + 2, base_itr_);
                token_list_.push_back(t);
                s_itr_ += 2;
                return;
            }
        }

        if ('<' == *s_itr_)
            t.set_operator(token_t::e_lt, s_itr_, s_itr_ + 1, base_itr_);
        else if ('>' == *s_itr_)
            t.set_operator(token_t::e_gt, s_itr_, s_itr_ + 1, base_itr_);
        else if (';' == *s_itr_)
            t.set_operator(token_t::e_eof, s_itr_, s_itr_ + 1, base_itr_);
        else if ('&' == *s_itr_)
            t.set_symbol(s_itr_, s_itr_ + 1, base_itr_);
        else if ('|' == *s_itr_)
            t.set_symbol(s_itr_, s_itr_ + 1, base_itr_);
        else
            t.set_operator(token_t::token_type(*s_itr_), s_itr_, s_itr_ + 1, base_itr_);

        token_list_.push_back(t);

        ++s_itr_;
    }

    inline void scan_symbol()
    {
        const char* begin = s_itr_;
        while ((!is_end(s_itr_)) && (details::is_letter_or_digit(*s_itr_) || ((*s_itr_) == '_'))) {
            ++s_itr_;
        }
        token_t t;
        t.set_symbol(begin, s_itr_, base_itr_);
        token_list_.push_back(t);
    }

    inline void scan_number()
    {
        /*
           Attempt to match a valid numeric value in one of the following formats:
           01. 123456
           02. 123.456
           03. 123.456e3
           04. 123.456E3
           05. 123.456e+3
           06. 123.456E+3
           07. 123.456e-3
           08. 123.456E-3
           09. .1234
           10. .1234e3
           11. .1234E+3
           12. .1234e+3
           13. .1234E-3
           14. .1234e-3
        */
        const char* begin              = s_itr_;
        bool        dot_found          = false;
        bool        e_found            = false;
        bool        post_e_sign_found  = false;
        bool        post_e_digit_found = false;
        token_t     t;

        while (!is_end(s_itr_)) {
            if ('.' == (*s_itr_)) {
                if (dot_found) {
                    t.set_error(token::e_err_number, begin, s_itr_, base_itr_);
                    token_list_.push_back(t);

                    return;
                }

                dot_found = true;
                ++s_itr_;

                continue;
            }
            else if (details::imatch('e', (*s_itr_))) {
                const char& c = *(s_itr_ + 1);

                if (is_end(s_itr_ + 1)) {
                    t.set_error(token::e_err_number, begin, s_itr_, base_itr_);
                    token_list_.push_back(t);

                    return;
                }
                else if (('+' != c) && ('-' != c) && !details::is_digit(c)) {
                    t.set_error(token::e_err_number, begin, s_itr_, base_itr_);
                    token_list_.push_back(t);

                    return;
                }

                e_found = true;
                ++s_itr_;

                continue;
            }
            else if (e_found && details::is_sign(*s_itr_) && !post_e_digit_found) {
                if (post_e_sign_found) {
                    t.set_error(token::e_err_number, begin, s_itr_, base_itr_);
                    token_list_.push_back(t);

                    return;
                }

                post_e_sign_found = true;
                ++s_itr_;

                continue;
            }
            else if (e_found && details::is_digit(*s_itr_)) {
                post_e_digit_found = true;
                ++s_itr_;

                continue;
            }
            else if (('.' != (*s_itr_)) && !details::is_digit(*s_itr_))
                break;
            else
                ++s_itr_;
        }

        t.set_numeric(begin, s_itr_, base_itr_);

        token_list_.push_back(t);

        return;
    }

    inline void scan_comments()
    {
        ++s_itr_;
        skip_whitespace();
        const char* begin = s_itr_;

        while (!is_end(s_itr_) && ('\n' != *s_itr_)) {
            ++s_itr_;
        }
        token_t t;
        t.set_string(begin, s_itr_, base_itr_);
        t.type = token::e_comment;
        token_list_.push_back(t);
    }

    inline void scan_string()
    {
        const char* begin = s_itr_ + 1;

        token_t t;

        if (std::distance(s_itr_, s_end_) < 2) {
            t.set_error(token::e_err_string, s_itr_, s_end_, base_itr_);
            token_list_.push_back(t);

            return;
        }

        ++s_itr_;

        bool escaped_found = false;
        bool escaped       = false;

        while (!is_end(s_itr_)) {
            if (!escaped && ('\\' == *s_itr_)) {
                escaped_found = true;
                escaped       = true;
                ++s_itr_;

                continue;
            }
            else if (!escaped) {
                if ('\'' == *s_itr_)
                    break;
            }
            else if (escaped)
                escaped = false;

            ++s_itr_;
        }

        if (is_end(s_itr_)) {
            t.set_error(token::e_err_string, begin, s_itr_, base_itr_);
            token_list_.push_back(t);

            return;
        }

        if (!escaped_found)
            t.set_string(begin, s_itr_, base_itr_);
        else {
            std::string parsed_string(begin, s_itr_);
            details::cleanup_escapes(parsed_string);
            t.set_string(parsed_string, (size_t)std::distance(base_itr_, begin));
        }

        token_list_.push_back(t);
        ++s_itr_;

        return;
    }

    private:
    token_list_t     token_list_;
    token_list_itr_t token_itr_;
    token_list_itr_t store_token_itr_;
    token_t          eof_token_;
    const char*      base_itr_;
    const char*      s_itr_;
    const char*      s_end_;

    friend class token_scanner;
    friend class token_modifier;
    friend class token_inserter;
    friend class token_joiner;
};

class helper_interface {
    public:
    virtual void        init() {}
    virtual void        reset() {}
    virtual bool        result() { return true; }
    virtual std::size_t process(generator&) { return 0; }
    virtual ~helper_interface() {}
};

class token_scanner : public helper_interface {
    public:
    virtual ~token_scanner() {}

    explicit token_scanner(const std::size_t& stride)
        : stride_(stride)
    {
        if (stride > 4) {
            throw std::invalid_argument("token_scanner() - Invalid stride value");
        }
    }

    inline std::size_t process(generator& g)
    {
        if (!g.token_list_.empty()) {
            for (std::size_t i = 0; i < (g.token_list_.size() - stride_ + 1); ++i) {
                token t;
                switch (stride_) {
                case 1: {
                    const token& t0 = g.token_list_[i];

                    if (!operator()(t0))
                        return i;
                } break;

                case 2: {
                    const token& t0 = g.token_list_[i];
                    const token& t1 = g.token_list_[i + 1];

                    if (!operator()(t0, t1))
                        return i;
                } break;

                case 3: {
                    const token& t0 = g.token_list_[i];
                    const token& t1 = g.token_list_[i + 1];
                    const token& t2 = g.token_list_[i + 2];

                    if (!operator()(t0, t1, t2))
                        return i;
                } break;

                case 4: {
                    const token& t0 = g.token_list_[i];
                    const token& t1 = g.token_list_[i + 1];
                    const token& t2 = g.token_list_[i + 2];
                    const token& t3 = g.token_list_[i + 3];

                    if (!operator()(t0, t1, t2, t3))
                        return i;
                } break;
                }
            }
        }

        return (g.token_list_.size() - stride_ + 1);
    }

    virtual bool operator()(const token&) { return false; }

    virtual bool operator()(const token&, const token&) { return false; }

    virtual bool operator()(const token&, const token&, const token&) { return false; }

    virtual bool operator()(const token&, const token&, const token&, const token&)
    {
        return false;
    }

    private:
    std::size_t stride_;
};

class token_modifier : public helper_interface {
    public:
    inline std::size_t process(generator& g)
    {
        std::size_t changes = 0;

        for (std::size_t i = 0; i < g.token_list_.size(); ++i) {
            if (modify(g.token_list_[i]))
                changes++;
        }

        return changes;
    }

    virtual bool modify(token& t) = 0;
};

class token_inserter : public helper_interface {
    public:
    explicit token_inserter(const std::size_t& stride)
        : stride_(stride)
    {
        if (stride > 5) {
            throw std::invalid_argument("token_inserter() - Invalid stride value");
        }
    }

    inline std::size_t process(generator& g)
    {
        if (g.token_list_.empty())
            return 0;

        std::size_t changes = 0;

        for (std::size_t i = 0; i < (g.token_list_.size() - stride_ + 1); ++i) {
            token t;
            int   insert_index = -1;

            switch (stride_) {
            case 1: insert_index = insert(g.token_list_[i], t); break;

            case 2: insert_index = insert(g.token_list_[i], g.token_list_[i + 1], t); break;

            case 3:
                insert_index
                    = insert(g.token_list_[i], g.token_list_[i + 1], g.token_list_[i + 2], t);
                break;

            case 4:
                insert_index = insert(g.token_list_[i], g.token_list_[i + 1], g.token_list_[i + 2],
                                      g.token_list_[i + 3], t);
                break;

            case 5:
                insert_index = insert(g.token_list_[i], g.token_list_[i + 1], g.token_list_[i + 2],
                                      g.token_list_[i + 3], g.token_list_[i + 4], t);
                break;
            }

            if ((insert_index >= 0) && (insert_index <= (static_cast<int>(stride_) + 1))) {
                g.token_list_.insert(g.token_list_.begin() + int(i + insert_index), t);
                changes++;
            }
        }

        return changes;
    }

    virtual inline int insert(const token&, token&) { return -1; }

    virtual inline int insert(const token&, const token&, token&) { return -1; }

    virtual inline int insert(const token&, const token&, const token&, token&) { return -1; }

    virtual inline int insert(const token&, const token&, const token&, const token&, token&)
    {
        return -1;
    }

    virtual inline int
        insert(const token&, const token&, const token&, const token&, const token&, token&)
    {
        return -1;
    }

    private:
    std::size_t stride_;
};

class token_joiner : public helper_interface {
    public:
    inline std::size_t process(generator& g)
    {
        if (g.token_list_.empty())
            return 0;

        std::size_t changes = 0;

        for (std::size_t i = 0; i < g.token_list_.size() - 1; ++i) {
            token t;

            if (join(g.token_list_[i], g.token_list_[i + 1], t)) {
                g.token_list_[i] = t;
                g.token_list_.erase(g.token_list_.begin() + int64_t(i + 1));

                ++changes;
            }
        }

        return changes;
    }

    virtual bool join(const token&, const token&, token&) = 0;
};

namespace helper {

inline void dump(lexertk::generator& generator)
{
    for (std::size_t i = 0; i < generator.size(); ++i) {
        lexertk::token t = generator[i];
        printf("Token[%02d] @ %03d  %6s  -->  '%s'\n", static_cast<unsigned int>(i),
               static_cast<unsigned int>(t.position), t.to_str(t.type).c_str(), t.value.c_str());
    }
}

} // namespace helper

} // namespace lexertk

#endif
