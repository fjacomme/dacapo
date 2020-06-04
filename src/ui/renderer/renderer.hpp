#pragma once
#include "app.hpp"
#include "ui/ui.hpp"

#include <memory>
#include <string>

struct render_options {
    int x, y, w, h;

    std::string title;

    bool maximized = false;
};

class renderer {
    std::unique_ptr<ui> target;
    renderer(renderer const&) = delete;
    renderer& operator=(renderer const&) = delete;

    public:
    renderer(app& a, render_options const& options);
    ~renderer();
    void run();
};