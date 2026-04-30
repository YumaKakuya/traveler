set_project("traveler")
set_version("0.1.0")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

target("traveler")
    set_kind("binary")
    add_files("src/main.cpp")
