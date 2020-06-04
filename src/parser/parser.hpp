#pragma once

#include "chef/ast.hpp"

#include <memory>

enum class char_type { none, number, keyword, var, str, op, brack, comment, error, _count };

struct parser {
    std::string            filename;
    std::string            buffer;
    ast                    tree;
    std::vector<char_type> char_types;
    std::string            error;
    int                    line = -1;
    int                    col  = -1;

    parser(sound_defs const& sounds);
    ~parser();

    bool parse();

    private:
    struct pimpl;
    std::unique_ptr<pimpl> _p;
    parser(parser const&) = delete;
    parser& operator=(parser const&) = delete;
};
