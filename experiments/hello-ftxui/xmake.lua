set_project("hello-ftxui")
set_version("0.1.0")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

add_requires("ftxui")

target("hello-ftxui")
    set_kind("binary")
    add_files("src/main.cpp")
    add_packages("ftxui")
