-- experiments/ayane-latency/xmake.lua
-- Local build definition for the Ayane latency PoC harness.

set_project("ayane-latency-poc")
set_version("0.1.0")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

add_requires("llama.cpp 3775")

target("ayane-latency")
    set_kind("binary")
    add_files("main.cpp")
    add_packages("llama.cpp")
    set_optimize("fastest")
