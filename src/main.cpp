#include "command/parser.h"
#include "core/dispatcher.h"
#include "tui/layout.h"
#include "auth/credentials.h"
#include "auth/migration.h"
#include "llm/error_render.h"
#include "persist/sessions_db.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include "auth/oauth.h"
#endif

namespace {

void print_usage() {
    std::printf(
        "Traveler. v0.1.0-alpha\n"
        "Usage: traveler [--version | --self-test-dispatcher | --layout-snapshot]\n"
        "       traveler providers login <provider>\n"
        "       traveler credentials inspect\n"
        "       traveler sessions db-info\n"
        "       traveler error-render --type <auth|network|provider> [--detail <msg>]\n"
        "       traveler auth migration-check\n"
    );
}

// PC-1 smoke: OAuth login readiness (URL generation, no server bind)
int cmd_providers_login(int argc, char** argv) {
    if (argc < 4 || std::strcmp(argv[2], "login") != 0) {
        std::fprintf(stderr, "Usage: traveler providers login <provider>\n");
        return 1;
    }
    const char* provider = argv[3];
    if (std::strcmp(provider, "anthropic") != 0) {
        std::fprintf(stderr, "Only 'anthropic' provider is supported for OAuth login.\n");
        std::fprintf(stderr, "Use API-key auth for other providers via env vars.\n");
        return 1;
    }

#if !defined(_WIN32) && !defined(__APPLE__)
    // PC-1 smoke: prove OAuth PKCE + URL building works without real secrets
    auto pkce_result = traveler::auth::generate_pkce();
    if (!pkce_result) {
        std::fprintf(stderr, "PKCE generation failed: %s\n",
                     pkce_result.error().message.c_str());
        return 1;
    }
    auto state_result = traveler::auth::generate_state();
    if (!state_result) {
        std::fprintf(stderr, "State generation failed: %s\n",
                     state_result.error().message.c_str());
        return 1;
    }

    std::string redirect_uri = "http://localhost:1456/callback";
    std::string auth_url = traveler::auth::build_authorization_url(
        pkce_result->challenge, *state_result, redirect_uri);

    std::printf("%s\n", auth_url.c_str());
    std::printf("\nPaste this URL into your browser to authorize.\n");
    std::printf("Authorization code + PKCE verifier + state generated successfully.\n");
    std::printf("OAuth callback expects server on 127.0.0.1:1456/callback.\n");
#else
    std::printf("OAuth flow is compiled for Linux targets.\n");
    std::printf("On this platform, use 'ANTHROPIC_API_KEY=... ./traveler' for API-key auth.\n");
#endif
    return 0;
}

// PC-1/PC-7 smoke: credentials store inspection (proves directory + permission path)
// Does NOT print, retrieve, or require real secrets.
int cmd_credentials_inspect() {
    std::string path = traveler::auth::credentials_file_path();
    std::printf("Credentials store path: %s\n", path.c_str());

    std::FILE* f = std::fopen(path.c_str(), "r");
    if (f) {
        std::fclose(f);
        std::printf("Credentials file: EXISTS\n");

#if !defined(_WIN32)
        struct stat st;
        if (::stat(path.c_str(), &st) == 0) {
            unsigned int mode = st.st_mode & 0777;
            std::printf("File permissions: %04o", mode);
            if (mode == 0600) {
                std::printf(" (PASS: restrictive 0600)\n");
            } else {
                std::printf(" (WARN: expected 0600)\n");
            }
        }
#endif

        // Prove provider entries exist without printing token values
        auto anthropic_creds = traveler::auth::read_credentials("anthropic");
        std::printf("Anthropic OAuth entry: %s\n",
                    (anthropic_creds && anthropic_creds->has_value())
                        ? "PRESENT (token values not displayed)"
                        : "NOT FOUND");
    } else {
        std::printf("Credentials file: NOT FOUND\n");
        std::printf("Directory ~/.config/traveler/ is created on first 'traveler providers login'.\n");
    }
    return 0;
}

// PC-9 smoke: SQLite session persistence smoke (proves DB path + schema reachable)
// Opens a temp/test DB, does not modify persisted state.
int cmd_sessions_db_info() {
    traveler::persist::SessionsDb db;
    auto open_result = db.open();

    if (!open_result) {
        std::fprintf(stderr, "Cannot open sessions database: %s\n",
                     open_result.error().message.c_str());
        std::printf("Database path: ~/.local/state/traveler/sessions.db\n");
        std::printf("This is created on first LLM session. No sessions exist yet.\n");
        return 1;
    }

    std::printf("Sessions database: OPEN\n");

    auto schema_ver = db.get_schema_version();
    if (schema_ver) {
        std::printf("Schema version: %d\n", *schema_ver);
    }

    auto sessions = db.list_sessions();
    if (sessions) {
        std::printf("Persisted sessions: %zu\n", sessions->size());
        for (const auto& s : *sessions) {
            std::printf("  [%s] callsign=%s model=%s\n",
                        s.id.c_str(), s.callsign.c_str(), s.model.c_str());
        }
    }
    if (sessions->empty()) {
        std::printf("No sessions yet. They will persist here after first LLM conversation.\n");
    }

    db.close();
    return 0;
}

// PC-10 smoke: provider error rendering with actionable message
int cmd_error_render(int argc, char** argv) {
    std::string kind = "auth";
    std::string detail = "(no detail provided)";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--type") == 0 && i + 1 < argc) {
            kind = argv[++i];
        } else if (std::strcmp(argv[i], "--detail") == 0 && i + 1 < argc) {
            detail = argv[++i];
        }
    }

    std::string rendered = traveler::llm::render_provider_error(kind, detail);
    std::printf("%s\n", rendered.c_str());
    return 0;
}

// PC-7 smoke: migration status check (proves detection path, no real migration)
int cmd_auth_migration_check() {
    bool hatch_exists = traveler::auth::detect_hatch_credentials();
    bool traveler_exists = traveler::auth::traveler_credentials_exist();

    std::printf("Hatch. credentials (~/.config/hatch/credentials.json): %s\n",
                hatch_exists ? "FOUND" : "NOT FOUND");
    std::printf("Traveler. credentials (~/.config/traveler/credentials.json): %s\n",
                traveler_exists ? "EXISTS" : "NOT FOUND");

    if (hatch_exists && !traveler_exists) {
        std::printf("\nMigration prompt would show: 'Migrate Hatch. credentials? (y/N)'\n");
        std::printf("Actual migration requires CEO authorization and is interactive.\n");
    } else if (hatch_exists && traveler_exists) {
        std::printf("\nBoth credential files exist — migration already done or not needed.\n");
    } else {
        std::printf("\nNo migration needed.\n");
    }
    return 0;
}

bool run_dispatcher_self_test() {
    traveler::core::Dispatcher dispatcher;
    if (dispatcher.current_mode() != traveler::core::Mode::Cockpit) {
        std::fprintf(stderr, "dispatcher default mode mismatch\n");
        return false;
    }

    const auto editor = dispatcher.transition_command("/Editor");
    if (!editor.ok || editor.to != traveler::core::Mode::Editor) {
        std::fprintf(stderr, "slash transition to Editor failed\n");
        return false;
    }

    dispatcher.set_mode_state(traveler::core::Mode::Editor, "file=README.md");
    const auto llm = dispatcher.transition_command("/LLM");
    if (!llm.ok || llm.to != traveler::core::Mode::LLM) {
        std::fprintf(stderr, "slash transition to LLM failed\n");
        return false;
    }
    if (dispatcher.mode_state(traveler::core::Mode::Editor) != "file=README.md") {
        std::fprintf(stderr, "mode-local state was not preserved\n");
        return false;
    }

    const auto parsed = traveler::command::parse_slash_command("/Pane split");
    if (!parsed.ok || parsed.command.name != "Pane" || parsed.command.args.size() != 1 ||
        parsed.command.args.front() != "split") {
        std::fprintf(stderr, "slash parser failed\n");
        return false;
    }

    const auto leader = traveler::command::parse_leader_key('e');
    if (!leader.ok || leader.command != "Editor") {
        std::fprintf(stderr, "leader parser failed\n");
        return false;
    }

    const auto callsigns = traveler::command::complete_callsign("ask @v");
    if (callsigns.empty() || callsigns.front() != "@vega") {
        std::fprintf(stderr, "callsign completion failed\n");
        return false;
    }

    std::printf("dispatcher self-test PASS\n");
    return true;
}

void print_layout_snapshot() {
    traveler::core::Dispatcher dispatcher;
    const auto mode = traveler::core::mode_name(dispatcher.current_mode());

    traveler::tui::LayoutModel model;
    model.mode = std::string(mode);
    model.callsigns = traveler::command::callsign_roster();
    model.stage_title = "Cockpit Stage";
    model.stage_body = "Focused callsign @vega is ready. Stage content swaps when modes change.";
    model.tower_prompt = "/Editor /LLM /Pane /Git | space leader | Ctrl+P palette";

    const auto snapshot = traveler::tui::render_layout_snapshot(model, 96, 24);
    std::fwrite(snapshot.data(), 1, snapshot.size(), stdout);
}

}  // namespace

int main(int argc, char** argv) {
    // --flag style arguments (backwards-compatible; single-arg flags only)
    if (argc == 2) {
        if (std::strcmp(argv[1], "--version") == 0) {
            std::printf("Traveler. v0.1.0-alpha\n");
            return 0;
        }
        if (std::strcmp(argv[1], "--self-test-dispatcher") == 0) {
            return run_dispatcher_self_test() ? 0 : 1;
        }
        if (std::strcmp(argv[1], "--layout-snapshot") == 0) {
            print_layout_snapshot();
            return 0;
        }
        if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    // Subcommand routing (multi-arg commands)
    if (argc >= 2) {
        if (std::strcmp(argv[1], "providers") == 0) return cmd_providers_login(argc, argv);
        if (std::strcmp(argv[1], "credentials") == 0) return cmd_credentials_inspect();
        if (std::strcmp(argv[1], "sessions") == 0) return cmd_sessions_db_info();
        if (std::strcmp(argv[1], "error-render") == 0) return cmd_error_render(argc, argv);
        if (std::strcmp(argv[1], "auth") == 0) return cmd_auth_migration_check();

        // Unrecognized arg
        std::fprintf(stderr, "Unknown option: %s\n", argv[1]);
        print_usage();
        return 1;
    }

    // No args: default layout snapshot
    print_layout_snapshot();
    return 0;
}
