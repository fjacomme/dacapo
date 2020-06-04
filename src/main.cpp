#include "app.hpp"
#include "parser/parser.hpp"
#include "soundgen/soundgen.hpp"
#include "ui/renderer/renderer.hpp"

#include <conio.h>
#include <iostream>
#include <thread>

int main()
#ifndef _DEBUG
try
#endif
{
    app app;

    render_options const opt { 50, 0, 1800, 1000, "dacapo", true };

    renderer renderer(app, opt);

    renderer.run();

    return 0;
}
#ifndef _DEBUG
catch(std::exception const& e) {
    std::cerr << "FATAL ERROR: " << e.what() << std::endl;
}
#endif