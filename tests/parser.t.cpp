#include "catch2/catch.hpp"
#include "parser/parser.hpp"

TEST_CASE("Parser")
{
    SECTION("Unit")
    {
        auto parse = [](auto data) {
            parser prs({});
            prs.buffer = data;
            prs.parse();
            return prs.tree;
        };

        REQUIRE(parse("").empty());
        REQUIRE(std::get<comment>(parse("~test").at(0)).text == "test");
        REQUIRE(std::get<affect>(parse("tempo 1").at(0)).name == "tempo");
        REQUIRE(std::get<affect>(parse("tempo 1").at(0)).val == 1);
        REQUIRE(std::get<on_beat>(parse("on 1 rest").at(0)).beat == 1);
        REQUIRE(std::get<on_beat>(parse("on 1 2/3 rest").at(0)).sub_beat == 2);
        REQUIRE(std::get<on_beat>(parse("on 1 2/3 rest").at(0)).nb_sub == 3);
        REQUIRE(std::get<sequence>(parse("seq 3 rest").at(0)).nb_measure == 3);
    }
}