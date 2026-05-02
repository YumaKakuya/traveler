set_project("traveler")
set_version("0.1.0")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

add_requires("ftxui")

option("with_llama")
    set_default(true)
    set_showmenu(true)
    set_description("Enable llama.cpp integration (--with_llama=y/n, default true)")
option_end()

option("asm_hot_paths")
    set_default(false)
    set_showmenu(true)
    set_description("Enable ASM hot paths (--asm_hot_paths=y/n, default false)")
option_end()

target("traveler")
    set_kind("binary")
    add_includedirs("src")
    add_files("src/main.cpp")
    add_options("with_llama", "asm_hot_paths")
    if has_config("with_llama") then
        add_defines("TRAVELER_WITH_LLAMA")
    end
    if has_config("asm_hot_paths") then
        add_defines("TRAVELER_ASM_HOT_PATHS")
    end

target("hello-ftxui")
    set_kind("binary")
    add_files("experiments/hello-ftxui/src/main.cpp")
    add_packages("ftxui")
