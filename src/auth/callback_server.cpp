// Reference: ~/hatch-v3/packages/opencode/src/plugin/claude-sub/index.ts (startOAuthServer, waitForOAuthCallback, HTML pages)
// Reference: Traveler_Phase0_Spec_v0.1.md §7.5 (REQ-OAUTH-4: local callback server)
#include "callback_server.h"
#include "oauth.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <httplib.h>

namespace traveler::auth {

// ============================================================================
// HTML response pages (faithful to Hatch. index.ts HTML_SUCCESS / HTML_ERROR)
// ============================================================================

static const char* HTML_SUCCESS = R"(<!doctype html>
<html>
  <head>
    <title>Traveler - Authorization Successful</title>
    <style>
      body {
        font-family: system-ui, -apple-system, sans-serif;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
        margin: 0;
        background: #131010;
        color: #f1ecec;
      }
      .container { text-align: center; padding: 2rem; }
      h1 { color: #f1ecec; margin-bottom: 1rem; }
      p { color: #b7b1b1; }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Authorization Successful</h1>
      <p>You can close this window and return to Traveler.</p>
    </div>
    <script>setTimeout(function(){window.close()},2000)</script>
  </body>
</html>)";

static std::string html_error(const std::string& error) {
    return std::string(R"(<!doctype html>
<html>
  <head>
    <title>Traveler - Authorization Failed</title>
    <style>
      body {
        font-family: system-ui, -apple-system, sans-serif;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
        margin: 0;
        background: #131010;
        color: #f1ecec;
      }
      .container { text-align: center; padding: 2rem; }
      h1 { color: #fc533a; margin-bottom: 1rem; }
      p { color: #b7b1b1; }
      .error {
        color: #ff917b;
        font-family: monospace;
        margin-top: 1rem;
        padding: 1rem;
        background: #3c140d;
        border-radius: 0.5rem;
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Authorization Failed</h1>
      <p>An error occurred during authorization.</p>
      <div class="error">)") + error + R"(</div>
    </div>
  </body>
</html>)";
}

// ============================================================================
// Server state — shared between background thread and waiting caller
// ============================================================================

struct ServerState {
    std::mutex mtx;
    std::condition_variable cv;
    CallbackResult result;
    std::string expected_state;
    bool completed{false};
};

static std::unique_ptr<httplib::Server> g_server;
static std::unique_ptr<std::thread> g_server_thread;
static std::shared_ptr<ServerState> g_state;
static int g_server_port = 1456;

// ============================================================================
// start / wait / stop
// ============================================================================

tl::expected<std::string, Error> start_oauth_server() {
    if (g_server) {
        std::string uri = "http://localhost:" + std::to_string(g_server_port)
                        + OAUTH_REDIRECT_PATH;
        return uri;
    }

    g_state = std::make_shared<ServerState>();
    g_server = std::make_unique<httplib::Server>();

    auto state = g_state;  // capture shared_ptr for lambda

    // Register the OAuth callback route BEFORE binding, so the handler is
    // ready when the first request arrives.
    g_server->Get(OAUTH_REDIRECT_PATH, [state](const httplib::Request& req,
                                                httplib::Response& res) {
        std::lock_guard<std::mutex> lock(state->mtx);

        auto code = req.get_param_value("code");
        auto req_state = req.get_param_value("state");
        auto error = req.get_param_value("error");
        auto error_desc = req.get_param_value("error_description");

        if (req_state != state->expected_state) {
            state->result.status = CallbackStatus::error;
            state->result.error_message = "Invalid state - potential CSRF attack";
            state->completed = true;
            state->cv.notify_one();
            res.status = 400;
            res.set_content(html_error("Invalid state - potential CSRF attack"),
                            "text/html");
            return;
        }

        if (!error.empty()) {
            state->result.status = CallbackStatus::error;
            state->result.error_message = error_desc.empty() ? error : error_desc;
            state->completed = true;
            state->cv.notify_one();
            res.set_content(html_error(state->result.error_message), "text/html");
            return;
        }

        if (code.empty()) {
            state->result.status = CallbackStatus::error;
            state->result.error_message = "Missing authorization code";
            state->completed = true;
            state->cv.notify_one();
            res.status = 400;
            res.set_content(html_error("Missing authorization code"), "text/html");
            return;
        }

        state->result.status = CallbackStatus::success;
        state->result.authorization_code = code;
        state->completed = true;
        state->cv.notify_one();
        res.set_content(HTML_SUCCESS, "text/html");
    });

    // Bind BEFORE spawning the thread so we can confirm port availability
    // synchronously.  bind_to_port() returns false immediately if the port
    // is in use or inaccessible.
    g_server_port = 1456;
    if (!g_server->bind_to_port("127.0.0.1", g_server_port)) {
        g_server.reset();
        g_state.reset();
        return tl::make_unexpected(
            Error::Provider("Cannot bind to OAuth callback 127.0.0.1:" +
                           std::to_string(g_server_port) +
                           " — port may be in use or permission denied"));
    }

    // Bind succeeded — spin up the accept-loop thread.
    // listen_after_bind() uses the already-bound socket and blocks until stop().
    g_server_thread = std::make_unique<std::thread>([]() {
        if (!g_server->listen_after_bind()) {
            // listen() or the accept loop failed after a successful bind.
            // Signal the error through ServerState so wait_for_oauth_callback()
            // can surface it.
            std::lock_guard<std::mutex> lock(g_state->mtx);
            if (!g_state->completed) {
                g_state->result.status = CallbackStatus::error;
                g_state->result.error_message = "OAuth callback server failed after port bind";
                g_state->completed = true;
                g_state->cv.notify_one();
            }
        }
    });

    std::string redirect_uri =
        "http://localhost:" + std::to_string(g_server_port) + OAUTH_REDIRECT_PATH;
    return redirect_uri;
}

tl::expected<CallbackResult, Error>
wait_for_oauth_callback(std::string_view expected_state,
                         std::chrono::seconds timeout) {
    if (!g_state) {
        return tl::make_unexpected(
            Error::Provider("OAuth server not started"));
    }

    g_state->expected_state = std::string(expected_state);

    {
        std::unique_lock<std::mutex> lock(g_state->mtx);
        bool success = g_state->cv.wait_for(lock, timeout, [] {
            return g_state->completed;
        });

        if (!success) {
            g_state->result.status = CallbackStatus::timeout;
            g_state->result.error_message =
                "OAuth callback timeout - authorization took too long";
            g_state->completed = true;
        }
    }

    return g_state->result;
}

void stop_oauth_server() {
    if (g_server) {
        g_server->stop();
    }
    if (g_server_thread && g_server_thread->joinable()) {
        g_server_thread->join();
    }
    g_server_thread.reset();
    g_server.reset();
    g_state.reset();
}

}  // namespace traveler::auth
