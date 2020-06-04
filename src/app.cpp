#include "app.hpp"

app::mix::mix(std::string const& n, sound_defs const& d)
    : name(n)
    , pars(d)
{
}
void app::mix::write_file()
{
    std::ofstream(pars.filename, std::ofstream::trunc) << pars.buffer;
    saved = true;
}

void app::mix::read_file()
{
    std::ifstream t(pars.filename);
    pars.buffer.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    saved = true;
}

app::app()
    : sg()
    , ch(sg)
{
    set_file("temp.dcp");
}

void app::new_file(std::filesystem::path const& folder, std::string const& filename)
{
    auto const filepath = folder / filename;
    std::filesystem::create_directories(filepath.parent_path());
    std::ofstream(filepath.generic_string()) << "";
    if (folder.generic_string() == current_folder) {
        add_file(filepath);
    }
    else {
        set_file(filepath);
    }
}

void app::set_file(std::filesystem::path const& p)
{
    mixes.clear();
    current_folder.clear();
    ch.asts.clear();
    add_file(p);
}

void app::add_file(std::filesystem::path const& p)
{
    std::string const filename = p.generic_string();
    std::string const mixname  = p.filename().stem().string();
    for (auto& mx : mixes) {
        if (mx.second.pars.filename == filename) {
            mx.second.read_file();
            return;
        }
    }
    auto mx = mixes.emplace(std::piecewise_construct, std::forward_as_tuple(mixname),
                            std::forward_as_tuple(mixname, sg.defs));
    mx.first->second.pars.filename = filename;
    mx.first->second.read_file();
    parse(mx.first->second);
}

void app::set_folder(std::filesystem::path const& p)
{
    current_folder = p.generic_string();
    mixes.clear();
    ch.asts.clear();
    namespace fs = std::filesystem;
    using dir_it = fs::directory_iterator;
    for (auto it = dir_it(p); it != dir_it(); it++) {
        if (it->is_regular_file() && it->path().extension() == ".dcp") {
            add_file(it->path());
        }
    }
}

void app::update()
{
    if (is_running) {
        ch.update();
    }
}

void app::zero()
{
    ch.beat     = 1;
    ch.measure  = 1;
    ch.sub_beat = 0;
    parse_all();
}

void app::parse(std::string const& mn)
{
    auto mix = mixes.find(mn);
    if (mix == mixes.end()) {
        return;
    }
    parse(mix->second);
}

void app::parse(mix& m)
{
    if (m.pars.parse()) {
        ch.asts[m.name] = m.pars.tree;
        if (auto_save)
            m.write_file();
    }
}

void app::parse_all()
{
    for (auto& mix : mixes) {
        parse(mix.second);
    }
}

void app::read_all()
{
    auto const prev_as = auto_save;
    auto_save          = false;
    for (auto& mix : mixes) {
        mix.second.read_file();
        parse(mix.second);
    }
    auto_save = true;
}

void app::write_all()
{
    for (auto& mix : mixes) {
        mix.second.write_file();
    }
}