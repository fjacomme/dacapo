#include "ui/ui.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "implot.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

namespace fs = std::filesystem;

ui::ui(app& a)
    : ap(a)
    , codeedit(a)
{
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\FiraCode-Regular.ttf", 20.0f);
}

ui::~ui()
{
}

bool ui::frame(int width, int height)
{
    ap.update();

    handle_shortcuts();

    if (!draw_menu()) {
        return false;
    }

    draw_dialog();

    float const y = 30.f;
    float const w = float(width);
    float const h = float(height) - y;

    ImGui::SetNextWindowPos(ImVec2(0, y));
    ImGui::SetNextWindowSize(ImVec2(w * .7f, h), ImGuiCond_Always);
    ImGui::Begin("Editor", nullptr, ImGuiWindowFlags_NoDecoration);
    codeedit.draw();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(w * .7f, y));
    ImGui::SetNextWindowSize(ImVec2(w * .3f, h), ImGuiCond_Always);
    ImGui::Begin("data", nullptr, ImGuiWindowFlags_NoDecoration);

    if (ImPlot::BeginPlot("Out Wave")) {
        double vals[4] = { 1, 2, 3, 4 };
        ImPlot::PlotBars("s", vals, 4);

        ImPlot::EndPlot();
    }

    if (ImGui::CollapsingHeader("Synth")) {
        int i = 0;
        for (auto& s : ap.sg.defs.synths) {
            ImGui::PushID(i);
            if (ImGui::Button(">")) {
                ap.sg.play(synth { i, s, { { synth::note, 60.f } } });
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::Text(s.c_str());
            i++;
        }
    }

    if (ImGui::CollapsingHeader("Samples")) {
        int i = 0;
        for (auto& s : ap.sg.defs.samples) {
            ImGui::PushID(i);
            if (ImGui::Button(">")) {
                ap.sg.play(sample { i, s });
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::Text(s.c_str());
            i++;
        }
    }
    ImGui::End();

    return true;
}

void ui::handle_shortcuts()
{
    auto& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed('N', false)) {
        new_file();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed('O', false)) {
        open_file();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed('D', false)) {
        open_folder();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed('S', false)) {
        save();
    }
    if (io.KeyAlt && ImGui::IsKeyPressed('R', false)) {
        ap.is_running = !ap.is_running;
    }
    if (io.KeyAlt && ImGui::IsKeyPressed('Z', false)) {
        ap.zero();
    }
}

bool ui::draw_menu()
{
    enum { None, NewFile, OpenFile, OpenFolder, Save } menuAction = None;
    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New File", "Ctrl+N")) {
            menuAction = NewFile;
        }
        if (ImGui::MenuItem("Open File", "Ctrl+O")) {
            menuAction = OpenFile;
        }
        if (ImGui::MenuItem("Open Folder", "Ctrl+D")) {
            menuAction = OpenFolder;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            menuAction = Save;
        }
        ImGui::MenuItem("Auto Save", "", &ap.auto_save);
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            return false;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Maestro")) {
        ImGui::MenuItem("Play", "Alt+R", &ap.is_running);
        if (ImGui::MenuItem("Reset", "Alt+Z")) {
            ap.zero();
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Tempo");
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Measure", &ap.ch.tempo, 1, 10)) {
        if (ap.ch.tempo < 1)
            ap.ch.tempo = 1;
    }
    ImGui::SetNextItemWidth(100);
    if (ImGui::InputInt("Beat", &ap.ch.measure, 1, 10)) {
        if (ap.ch.measure < 1)
            ap.ch.measure = 1;
        ap.ch.beat     = 1;
        ap.ch.sub_beat = 0;
    }
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderInt("/", &ap.ch.beat, 1, ap.ch.beats_per_measure)) {
    }
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("", &ap.ch.beats_per_measure);

    ImGui::Text("SubBeat %02d/%02d", ap.ch.sub_beat, ap.ch.sub_beats_per_beat);
    ImGui::EndMainMenuBar();

    // these need to be executed event when the menu is closed
    switch (menuAction) {
    case NewFile: new_file(); break;
    case OpenFile: open_file(); break;
    case OpenFolder: open_folder(); break;
    case Save: save(); break;
    case None: break;
    }
    return true;
}

static bool folder_leaf(fs::path const& dir)
{
    using dir_iter = fs::directory_iterator;
    for (auto it = dir_iter(dir); it != dir_iter(); ++it) {
        if (it->is_directory())
            return false;
    }
    return true;
}

enum FileTreeFlags {
    SelectFolder = 1 << 0,
    SelectFile   = 1 << 1,
    ShowFile     = 2 << 2,
};

static void file_tree(fs::path const& dir, fs::path& selected, bool& doubleclick, int flags)
{
    using dir_iter = fs::directory_iterator;
    for (auto it = dir_iter(dir); it != dir_iter(); ++it) {
        bool const is_dir     = it->is_directory();
        bool const show_files = (flags & ShowFile) || (flags & SelectFile);
        if (!is_dir && !it->is_regular_file()) {
            continue;
        }
        if (!is_dir && !show_files) {
            continue;
        }
        auto const& p       = it->path();
        bool const  is_leaf = !is_dir || (is_dir && folder_leaf(p));

        ImGuiTreeNodeFlags const nodeFlags = (is_leaf ? ImGuiTreeNodeFlags_Leaf : 0)
            | (selected == p ? ImGuiTreeNodeFlags_Selected : 0);

        bool const node_open     = ImGui::TreeNodeEx(p.filename().string().c_str(), nodeFlags);
        bool const select_folder = flags & SelectFolder;
        bool const select_file   = flags & SelectFile;
        if ((show_files || (is_dir && select_folder))) {
            if (ImGui::IsItemClicked()) {
                selected
                    = ((is_dir && select_folder) || (!is_dir && select_file)) ? p : p.parent_path();
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                doubleclick = true;
            }
        }
        if (is_dir && node_open) {
            file_tree(p, selected, doubleclick, flags);
            ImGui::TreePop();
        }
        else if (!is_dir) {
            ImGui::TreePop();
        }
    }
}

void ui::draw_dialog()
{
    if (ImGui::BeginPopupModal("Open File", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        static fs::path selected;
        if (ImGui::IsItemClicked()) {
            selected.clear();
        }
        ImGui::BeginChild("tree", ImVec2(300, 400));
        bool dblclick = false;
        file_tree("etc/mix", selected, dblclick, SelectFile);
        ImGui::EndChild();
        ImGui::NewLine();
        if (fs::is_regular_file(selected)) {
            ImGui::Text(selected.generic_string().c_str());
            ImGui::NewLine();
            if (ImGui::Button("Ok") || dblclick) {
                ap.set_file(selected);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    else if (ImGui::BeginPopupModal("Open Folder", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        static fs::path selected;
        ImGui::BeginChild("tree", ImVec2(300, 400));
        bool dblclick = false;
        file_tree("etc/mix", selected, dblclick, SelectFolder);
        ImGui::EndChild();
        ImGui::Text(selected.generic_string().c_str());
        if (ImGui::Button("Ok") || dblclick) {
            ap.set_folder(selected);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    else if (ImGui::BeginPopupModal("New File", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        static fs::path selected = "etc/mix";
        ImGui::BeginChild("tree", ImVec2(300, 400));
        bool dblclick = false;
        ImGui::SetNextTreeNodeOpen(true);
        bool const node_open = ImGui::TreeNodeEx("mix");
        if (ImGui::IsItemClicked()) {
            selected = "etc/mix";
        }
        if (node_open) {
            file_tree("etc/mix", selected, dblclick, SelectFolder | ShowFile);
            ImGui::TreePop();
        }

        ImGui::EndChild();
        static std::string filename;
        ImGui::InputText(".dcp", &filename);
        ImGui::NewLine();
        ImGui::Text((selected / (filename + ".dcp")).generic_string().c_str());
        if (filename.empty()) {
            ImGui::Text("Enter a filename");
        }
        else if (filename.find_first_of(".\\") != std::string::npos) {
            ImGui::Text("Filename cannot contain '.\\'");
        }
        else if (ImGui::Button("Ok")) {
            ap.new_file(selected, filename + ".dcp");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ui::new_file()
{
    ImGui::OpenPopup("New File");
}

void ui::open_file()
{
    ImGui::OpenPopup("Open File");
}

void ui::open_folder()
{
    ImGui::OpenPopup("Open Folder");
}

void ui::save()
{
    ap.write_all();
}