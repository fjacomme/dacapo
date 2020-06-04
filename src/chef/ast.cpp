#include "chef/ast.hpp"

#include <sstream>

std::string tabs(int level)
{
    return std::string(size_t(level * 2), ' ');
}

struct printer {
    int         level = 0;
    std::string operator()(comment const& i) { return " ~ " + i.text + "\n"; }
    std::string operator()(rest const&) { return "rest\n"; }
    std::string operator()(affect const& i)
    {
        std::stringstream s;
        s << i.name << " " << i.val << "\n";
        return s.str();
    }
    std::string operator()(on_beat const& i)
    {
        std::string s = "on " + std::to_string(i.beat);
        if (i.nb_sub > 1) {
            s += " " + std::to_string(i.sub_beat) + "/" + std::to_string(i.nb_sub);
        }
        visit_vec(i.statements, s);
        return s;
    }
    std::string operator()(play_sound const& ps)
    {
        return std::visit(
            [&](auto i) {
                std::stringstream s;
                s << "'" << i.name << "'";
                bool       first           = true;
                size_t     nb_params       = i.params.size();
                bool const too_many_params = nb_params > 4;

                std::string const sep
                    = too_many_params ? ("\n" + tabs(level + 1)) : std::string(" ");
                for (auto& param : i.params) {
                    float const param_val = param.second;
                    if (first) {
                        s << " (";
                        first = false;
                    }
                    s << sep + i.param_name(param.first) + ":" << param_val;
                }
                if (!first) {
                    s << (too_many_params ? ("\n" + tabs(level)) : " ") << ")";
                }
                s << "\n";
                return s.str();
            },
            ps.sound);
    }
    std::string operator()(between_measure const& i)
    {
        std::string s = std::to_string(i.m1);
        if (i.m1 != i.m2) {
            s += "-";
            if (i.m2 > 0) {
                s += std::to_string(i.m2);
            }
        }
        s += ":";
        visit_vec(i.statements, s);
        s += "\n";
        return s;
    }
    std::string operator()(sequence const& i)
    {
        std::string s = "seq " + std::to_string(i.nb_measure);
        visit_vec(i.statements, s);
        s += "\n";
        return s;
    }

    void visit_vec(std::vector<statement> const& stts, std::string& s)
    {
        if (stts.size() == 1) {
            s += " " + std::visit(*this, stts[0]);
            return;
        }
        s += "\n";
        auto    tab = tabs(level + 1);
        printer p2 { level + 1 };
        for (auto& st : stts) {
            s += tab + std::visit(p2, st);
        }
    }
};

void print(ast const& a, std::string& s)
{
    s.clear();
    printer p;
    for (auto& st : a) {
        s += std::visit(p, st);
    }
}

ast test_1()
{
    ast   a;
    synth s1 { 0, "sonic-pi-beep" };
    s1.params[synth::note] = 52;
    synth s2 { 0, "sonic-pi-beep" };
    s2.params[synth::note] = 60;

    {
        between_measure m1;
        m1.m1 = m1.m2 = 1;
        m1.statements.emplace_back(affect { "tempo", 120 });
        a.push_back(m1);
    }
    {
        between_measure ms;
        ms.m1 = 1;
        ms.m2 = 6;
        ms.statements.emplace_back(on_beat { 1, 1, 1, { play_sound { s1 } } });
        s1.params[synth::note] = 54;
        ms.statements.emplace_back(on_beat { 2, 1, 1, { play_sound { s1 } } });
        s1.params[synth::note] = 56;
        ms.statements.emplace_back(on_beat { 3, 1, 1, { play_sound { s1 } } });
        ms.statements.emplace_back(on_beat { 4, 1, 1, { play_sound { s2 } } });
        a.push_back(ms);
    }
    {
        between_measure m1;
        m1.m1 = m1.m2 = 7;
        m1.statements.emplace_back(affect { "tempo", 118 });
        a.push_back(m1);
    }
    {
        between_measure ms;
        ms.m1                  = 7;
        ms.m2                  = 8;
        s1.params[synth::note] = 52;
        ms.statements.emplace_back(on_beat { 1, 1, 1, { play_sound { s1 } } });
        ms.statements.emplace_back(on_beat { 2, 1, 1, { play_sound { s1 }, play_sound { s2 } } });
        s1.params[synth::note] = 56;
        ms.statements.emplace_back(on_beat { 3, 1, 1, { play_sound { s1 }, play_sound { s2 } } });
        ms.statements.emplace_back(on_beat { 4, 1, 1, { play_sound { s2 } } });
        a.push_back(ms);
    }
    {
        between_measure m1;
        m1.m1 = m1.m2 = 9;
        m1.statements.emplace_back(affect { "measure", 1 });
        a.push_back(m1);
    }
    return a;
}