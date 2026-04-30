// experiments/ayane-latency/main.cpp
// GATE-P0-0 T0-C: V-2 Ayane model first-token latency PoC harness

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <llama.h>

static void print_usage(const char* argv0) {
    std::cerr << "Usage: " << argv0 << " <model.gguf> [--runs N] [--prompt \"text\"] [--threads T]\n";
    std::cerr << "  --runs    Number of timing trials (default: 10)\n";
    std::cerr << "  --prompt  Fixed prompt string (default: \"Hello, traveler.\")\n";
    std::cerr << "  --threads Number of threads for inference (default: from llama.cpp)\n";
}

static double percentile(std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    size_t idx = static_cast<size_t>(std::floor(p * (sorted.size() - 1)));
    return sorted[idx];
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::string model_path = argv[1];
    int runs = 10;
    std::string prompt = "Hello, traveler.";
    int threads = -1;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--runs" && i + 1 < argc) {
            runs = std::atoi(argv[++i]);
        } else if (arg == "--prompt" && i + 1 < argc) {
            prompt = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            threads = std::atoi(argv[++i]);
        }
    }

    if (runs < 1) runs = 1;

    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Error: model file not found: " << model_path << "\n";
        return EXIT_FAILURE;
    }

    llama_backend_init();

    std::cout << "Ayane Latency PoC Harness\n";
    std::cout << "=========================\n";
    std::cout << "Model path: " << model_path << "\n";
    std::cout << "Prompt:     \"" << prompt << "\"\n";
    std::cout << "Runs:       " << runs << "\n\n";

    // Load model
    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0; // CPU-only for Core-i3-class measurement

    llama_model* model = llama_load_model_from_file(model_path.c_str(), mparams);
    if (!model) {
        std::cerr << "Failed to load model: " << model_path << "\n";
        llama_backend_free();
        return EXIT_FAILURE;
    }

    // Create context
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = 512;
    cparams.n_batch = 512;
    if (threads > 0) {
        cparams.n_threads = threads;
        cparams.n_threads_batch = threads;
    }

    llama_context* ctx = llama_new_context_with_model(model, cparams);
    if (!ctx) {
        std::cerr << "Failed to create context\n";
        llama_free_model(model);
        llama_backend_free();
        return EXIT_FAILURE;
    }

    // Tokenize prompt
    std::vector<llama_token> tokens(prompt.size() + 16);
    int32_t n_tokens = llama_tokenize(
        model, prompt.c_str(), static_cast<int32_t>(prompt.size()),
        tokens.data(), static_cast<int32_t>(tokens.size()), true, false);

    if (n_tokens < 0) {
        int32_t needed = -n_tokens;
        tokens.resize(static_cast<size_t>(needed));
        n_tokens = llama_tokenize(
            model, prompt.c_str(), static_cast<int32_t>(prompt.size()),
            tokens.data(), needed, true, false);
        if (n_tokens < 0) {
            std::cerr << "Failed to tokenize prompt\n";
            llama_free(ctx);
            llama_free_model(model);
            llama_backend_free();
            return EXIT_FAILURE;
        }
    }
    tokens.resize(static_cast<size_t>(n_tokens));

    if (n_tokens == 0) {
        std::cerr << "Error: prompt tokenized to zero tokens\n";
        llama_free(ctx);
        llama_free_model(model);
        llama_backend_free();
        return EXIT_FAILURE;
    }

    // Allocate batch once
    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    if (!batch.token) {
        std::cerr << "Failed to allocate batch\n";
        llama_free(ctx);
        llama_free_model(model);
        llama_backend_free();
        return EXIT_FAILURE;
    }

    std::vector<double> latencies;
    latencies.reserve(runs);

    for (int r = 0; r < runs; ++r) {
        llama_kv_cache_clear(ctx);

        for (int i = 0; i < n_tokens; ++i) {
            batch.token[i] = tokens[i];
            batch.pos[i] = i;
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = 0;
        }
        batch.logits[n_tokens - 1] = 1; // compute logits for last token
        batch.n_tokens = n_tokens;

        auto t0 = std::chrono::high_resolution_clock::now();
        int32_t dec = llama_decode(ctx, batch);
        if (dec != 0) {
            std::cerr << "Error: llama_decode failed with code " << dec << " on run " << (r + 1) << "\n";
            llama_batch_free(batch);
            llama_free(ctx);
            llama_free_model(model);
            llama_backend_free();
            return EXIT_FAILURE;
        }

        llama_sampler* smpl = llama_sampler_init_greedy();
        llama_token tok = llama_sampler_sample(smpl, ctx, -1);
        auto t1 = std::chrono::high_resolution_clock::now();
        llama_sampler_free(smpl);

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        latencies.push_back(ms);
        std::cout << "Run " << (r + 1) << ": " << ms << " ms\n";

        (void)tok; // unused; we only measure latency to first token
    }

    llama_batch_free(batch);
    llama_free(ctx);
    llama_free_model(model);
    llama_backend_free();

    std::sort(latencies.begin(), latencies.end());
    double min_ms = latencies.front();
    double max_ms = latencies.back();
    double median_ms = percentile(latencies, 0.5);

    std::cout << "\nLatency Summary (ms)\n";
    std::cout << "  min    = " << min_ms << "\n";
    std::cout << "  median = " << median_ms << "\n";
    std::cout << "  max    = " << max_ms << "\n";

    return EXIT_SUCCESS;
}
