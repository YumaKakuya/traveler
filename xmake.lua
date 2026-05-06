set_project("traveler")
set_version("0.1.0")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

includes("third_party/")

add_requires("ftxui", "tl_expected", "nlohmann_json", "sqlite3")

-- Cloud provider support (cpp-httplib+openssl) is Linux-only for now.
-- xmake detects the GitHub Windows runner as `mingw`, so guard both names.
if not is_plat("macosx", "windows", "mingw") then
    add_requires("openssl")
    add_requires("cpp-httplib", {configs = {ssl = true}})
    add_defines("CPPHTTPLIB_OPENSSL_SUPPORT")
end

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
    add_files("src/core/dispatcher.cpp")
    add_files("src/command/parser.cpp")
    add_files("src/tui/layout.cpp")
    add_files("src/roles/parser.cpp")
    add_files("src/roles/registry.cpp")
    add_files("src/safety/canonicalize.cpp")
    add_files("src/safety/pipeline.cpp")
    add_files("src/safety/event.cpp")
    add_files("src/safety/queue.cpp")
    add_files("src/safety/redact.cpp")
    add_files("src/cockpit/state.cpp")
    add_files("src/cockpit/mount.cpp")
    add_files("src/cockpit/snapshot.cpp")
    add_files("src/cockpit/strip.cpp")
    add_files("src/cockpit/bg_stream.cpp")
    add_files("src/cockpit/budget_check.cpp")
    add_files("src/util/hash.cpp")
    add_files("src/auth/credentials.cpp")
    add_files("src/auth/migration.cpp")
    add_files("src/llm/session.cpp")
    add_files("src/persist/sessions_db.cpp")
    if not is_plat("macosx", "windows", "mingw") then
        add_files("src/auth/oauth.cpp")
        add_files("src/auth/callback_server.cpp")
        add_files("src/auth/fetch_wrapper.cpp")
        add_files("src/adapters/anthropic_adapter.cpp")
        add_files("src/adapters/openai_adapter.cpp")
        add_files("src/adapters/google_adapter.cpp")
        add_files("src/adapters/llamacpp_adapter.cpp")
        add_files("src/adapters/tool_call.cpp")
    end
    add_options("with_llama", "asm_hot_paths")
    if has_config("with_llama") then
        add_defines("TRAVELER_WITH_LLAMA")
        add_includedirs("third_party/llama.cpp/include")
        add_deps("llama")
    end
    if has_config("asm_hot_paths") then
        add_defines("TRAVELER_ASM_HOT_PATHS")
    end
    add_packages("ftxui", "tl_expected", "nlohmann_json", "sqlite3")
    if not is_plat("macosx", "windows", "mingw") then
        add_packages("cpp-httplib", "openssl")
    end
    if is_plat("windows", "mingw") then
        add_syslinks("Advapi32")
    end

target("hello-ftxui")
    set_kind("binary")
    add_files("experiments/hello-ftxui/src/main.cpp")
    add_packages("ftxui")

-- ============================================================================
-- GATE-P0-4 Test Targets
-- ============================================================================

target("test_roles")
    set_kind("binary")
    add_includedirs("src")
    add_files("tests/unit/roles_test.cpp")
    add_files("src/roles/parser.cpp")
    add_files("src/roles/registry.cpp")
    add_packages("tl_expected")
    add_defines('TRAVELER_PROJECT_DIR="' .. os.projectdir() .. '"')
    set_group("test")

target("test_safety_corpus")
    set_kind("binary")
    add_includedirs("src")
    add_files("tests/unit/safety_corpus_test.cpp")
    add_files("src/safety/canonicalize.cpp")
    add_files("src/safety/pipeline.cpp")
    add_defines('TRAVELER_PROJECT_DIR="' .. os.projectdir() .. '"')
    set_group("test")

target("test_safety_queue")
    set_kind("binary")
    add_includedirs("src")
    add_files("tests/unit/safety_queue_test.cpp")
    add_files("src/safety/queue.cpp")
    add_files("src/safety/event.cpp")
    add_files("src/safety/canonicalize.cpp")
    set_group("test")

target("test_safety_redact")
    set_kind("binary")
    add_includedirs("src")
    add_files("tests/unit/safety_redact_test.cpp")
    add_files("src/safety/redact.cpp")
    add_files("src/safety/canonicalize.cpp")
    add_files("src/safety/queue.cpp")
    add_files("src/safety/event.cpp")
    set_group("test")
