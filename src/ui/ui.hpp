#pragma once
#include "app.hpp"
#include "imgui.h"
#include "ui/codeedit.hpp"

class ui {
    public:
    app& ap;

    codeedit codeedit;

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ui(app& ap);

    bool frame(int w, int h);

    void handle_shortcuts();

    bool draw_menu();

    void draw_dialog();

    void new_file();
    void open_file();
    void open_folder();
    void save();

    ~ui();

    private:

    ui(ui const&) = delete;
    ui& operator=(ui const&) = delete;
};