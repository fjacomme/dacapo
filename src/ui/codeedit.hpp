#pragma once

#include <memory>
#include <string>
#include <vector>

class app;

class codeedit {
    std::string name;
    app&        ap;

    public:
    codeedit(app& ap);
    ~codeedit();
    void draw();

    private:
    void reset_editors();
    struct pimpl;
    std::vector<std::unique_ptr<pimpl>> editors;
    codeedit(codeedit const&) = delete;
    codeedit operator=(codeedit const&) = delete;
};