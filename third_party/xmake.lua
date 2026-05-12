-- Reference: Traveler_Phase0_Spec_v0.1.md §9.3 (REQ-LLAMA-1: submodule + static link)
-- Reference: Proposal v0.2-FROZEN §4.6 / §9.4 T-8 (A-4)
-- Builds llama.cpp as a static library for static-link into the traveler binary
--
-- CPU-only backend. GPU backends (CUDA, Metal, Vulkan, etc.) are excluded by
-- pattern — only ggml/src/* and ggml/src/ggml-cpu/** sources are included.

target("llama")
    set_kind("static")
    set_basename("llama")

    add_includedirs(
        "llama.cpp/include",          -- llama.h (public API)
        "llama.cpp/src",              -- internal llama headers
        "llama.cpp/ggml/include",     -- ggml.h, ggml-alloc.h, etc.
        "llama.cpp/ggml/src",         -- ggml-impl.h, ggml-common.h
        "llama.cpp/ggml/src/ggml-cpu", -- ggml-cpu-impl.h (amx/ submodule include)
        "llama.cpp/common"            -- common utilities (build-info, etc.)
    )

    -- =========================================================================
    -- LLaMA core library (src/) + model definitions (src/models/)
    -- =========================================================================
    add_files("llama.cpp/src/*.cpp")
    add_files("llama.cpp/src/models/*.cpp")

    -- =========================================================================
    -- GGML tensor library core (ggml/src/)
    -- =========================================================================
    add_files("llama.cpp/ggml/src/ggml.c")
    add_files("llama.cpp/ggml/src/ggml.cpp")
    add_files("llama.cpp/ggml/src/ggml-alloc.c")
    add_files("llama.cpp/ggml/src/ggml-backend.cpp")
    add_files("llama.cpp/ggml/src/ggml-backend-meta.cpp")
    add_files("llama.cpp/ggml/src/ggml-backend-reg.cpp")
    add_files("llama.cpp/ggml/src/ggml-backend-dl.cpp")
    add_files("llama.cpp/ggml/src/ggml-opt.cpp")
    add_files("llama.cpp/ggml/src/ggml-quants.c")
    add_files("llama.cpp/ggml/src/ggml-threading.cpp")
    add_files("llama.cpp/ggml/src/gguf.cpp")

    -- =========================================================================
    -- GGML CPU backend (ggml/src/ggml-cpu/)
    -- =========================================================================
    add_files("llama.cpp/ggml/src/ggml-cpu/ggml-cpu.c")
    add_files("llama.cpp/ggml/src/ggml-cpu/ggml-cpu.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/ops.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/binary-ops.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/unary-ops.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/quants.c")
    add_files("llama.cpp/ggml/src/ggml-cpu/repack.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/vec.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/traits.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/hbm.cpp")

    -- CPU sub-modules: AMX, llamafile SGEMM, kleidiai
    add_files("llama.cpp/ggml/src/ggml-cpu/amx/amx.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/amx/mmq.cpp")
    add_files("llama.cpp/ggml/src/ggml-cpu/llamafile/sgemm.cpp")

    -- =========================================================================
    -- Build configuration
    -- =========================================================================
    set_languages("c11", "c++20")

    -- Compile definitions matching CMake build (ggml/src/CMakeLists.txt + parent CMakeLists.txt)
    -- GGML_VERSION_BASE = "0.10.2" from ggml/CMakeLists.txt
    add_defines("GGML_VERSION=\"0.10.2\"")
    -- GGML_BUILD_COMMIT = git rev-parse --short HEAD of the submodule
    add_defines("GGML_COMMIT=\"eff06702b\"")
    -- POSIX feature macros are platform-specific. `_XOPEN_SOURCE=600` hides
    -- Darwin BSD typedefs (`u_int`, `u_char`, `u_short`) used by macOS SDK headers.
    if is_plat("macosx") then
        add_defines("_DARWIN_C_SOURCE")
    elseif is_plat("linux") then
        add_defines("_XOPEN_SOURCE=600")
        add_defines("_GNU_SOURCE")
    end
    -- GGML_SCHED_MAX_COPIES from ggml/src/CMakeLists.txt:4
    add_defines("GGML_SCHED_MAX_COPIES=4")

    -- Enable OpenMP only where the CI compiler accepts GCC-style flags.
    if is_plat("linux") then
        add_cflags("-fopenmp")
        add_cxxflags("-fopenmp")
        add_ldflags("-fopenmp")
    end

    -- System libraries matching CMake where they exist on the target platform.
    if is_plat("linux") then
        add_syslinks("m", "dl", "pthread")
    elseif is_plat("macosx") then
        add_syslinks("m", "pthread")
    end
target_end()

-- ============================================================================
-- Tree-sitter Grammar Libraries — REQ-EDITOR-1 (10 mainstream languages)
-- Reference: Traveler_Phase0_Spec_v0.1.md §6.5
-- Single static library bundling all 10 tree-sitter grammar sources.
-- Each grammar provides a tree_sitter_<lang>() entry point.
-- All grammars are MIT-licensed; licenses verified locally.
-- ============================================================================
target("treesitter_grammars")
    set_kind("static")
    set_basename("treesitter_grammars")

    -- Include paths for each grammar's src/ directory
    -- Each grammar provides its own tree_sitter/{parser,array,alloc}.h
    add_includedirs(
        "tree-sitter/tree-sitter-c/src",
        "tree-sitter/tree-sitter-cpp/src",
        "tree-sitter/tree-sitter-python/src",
        "tree-sitter/tree-sitter-javascript/src",
        "tree-sitter/tree-sitter-typescript/typescript/src",
        "tree-sitter/tree-sitter-rust/src",
        "tree-sitter/tree-sitter-go/src",
        "tree-sitter/tree-sitter-java/src",
        "tree-sitter/tree-sitter-ruby/src",
        "tree-sitter/tree-sitter-bash/src"
    )

    -- Grammar source files: parser.c + scanner.c per language
    add_files("tree-sitter/tree-sitter-c/src/parser.c")
    add_files("tree-sitter/tree-sitter-cpp/src/parser.c")
    add_files("tree-sitter/tree-sitter-cpp/src/scanner.c")
    add_files("tree-sitter/tree-sitter-python/src/parser.c")
    add_files("tree-sitter/tree-sitter-python/src/scanner.c")
    add_files("tree-sitter/tree-sitter-javascript/src/parser.c")
    add_files("tree-sitter/tree-sitter-javascript/src/scanner.c")
    add_files("tree-sitter/tree-sitter-typescript/typescript/src/parser.c")
    add_files("tree-sitter/tree-sitter-typescript/typescript/src/scanner.c")
    add_files("tree-sitter/tree-sitter-rust/src/parser.c")
    add_files("tree-sitter/tree-sitter-rust/src/scanner.c")
    add_files("tree-sitter/tree-sitter-go/src/parser.c")
    add_files("tree-sitter/tree-sitter-java/src/parser.c")
    add_files("tree-sitter/tree-sitter-ruby/src/parser.c")
    add_files("tree-sitter/tree-sitter-ruby/src/scanner.c")
    add_files("tree-sitter/tree-sitter-bash/src/parser.c")
    add_files("tree-sitter/tree-sitter-bash/src/scanner.c")

    set_languages("c11")
target_end()
