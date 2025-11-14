/*
 * train_full.c - Full transformer training with proper dataset loading,
 * checkpointing, and model saving
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "transformer_v2.h"
#include "model_io_v2.h"
#include "dataset.h"

/* Training configuration */
typedef struct {
    /* Model architecture */
    int vocab_size;
    int d_model;
    int n_heads;
    int n_layers;
    int d_ff;
    int max_seq_len;

    /* Training hyperparameters */
    int batch_size;
    int seq_len;
    double learning_rate;
    int n_iters;
    int warmup_iters;

    /* Checkpointing */
    int checkpoint_interval;
    int save_interval;
    char model_dir[256];

    /* Logging */
    int log_interval;
    int sample_interval;
} TrainingConfig;

/* Default configuration */
TrainingConfig get_default_config(void) {
    TrainingConfig config = {
        /* GPT-2 small-like architecture */
        .vocab_size = 256,      /* Will be updated from dataset */
        .d_model = 256,
        .n_heads = 8,
        .n_layers = 4,
        .d_ff = 1024,
        .max_seq_len = 128,

        /* Training */
        .batch_size = 1,        /* Single batch for now */
        .seq_len = 64,
        .learning_rate = 3e-4,
        .n_iters = 10000,
        .warmup_iters = 100,

        /* Checkpointing */
        .checkpoint_interval = 1000,
        .save_interval = 5000,
        .model_dir = "models",

        /* Logging */
        .log_interval = 100,
        .sample_interval = 500
    };
    return config;
}

/* Learning rate schedule with warmup */
double get_learning_rate(int iter, TrainingConfig *config) {
    if (iter < config->warmup_iters) {
        /* Linear warmup */
        return config->learning_rate * ((double)iter / config->warmup_iters);
    } else {
        /* Cosine decay */
        double progress = (double)(iter - config->warmup_iters) /
                         (config->n_iters - config->warmup_iters);
        return config->learning_rate * (0.5 * (1.0 + cos(M_PI * progress)));
    }
}

/* Generate sample text during training */
void generate_sample(TransformerV2 *model, CharTokenizer *tokenizer,
                    const char *prompt, int length) {
    printf("  Sample: \"%s", prompt);

    /* Tokenize prompt */
    int tokens[256];
    int n_tokens = 0;
    for (int i = 0; prompt[i] && n_tokens < 256; i++) {
        tokens[n_tokens++] = char_to_token(prompt[i], tokenizer);
    }

    /* Generate continuation */
    for (int i = 0; i < length; i++) {
        /* Forward pass */
        int window_start = 0;
        int window_len = n_tokens;
        if (n_tokens > model->max_seq_len) {
            window_start = n_tokens - model->max_seq_len;
            window_len = model->max_seq_len;
        }

        VariableV2 *logits = transformer_forward(model,
                                                 tokens + window_start,
                                                 window_len);

        /* Get last position logits */
        double *last_logits = logits->data->data +
                             (window_len - 1) * model->vocab_size;

        /* Greedy sampling for now */
        int next_token = 0;
        double max_val = -INFINITY;
        for (int v = 0; v < model->vocab_size; v++) {
            if (last_logits[v] > max_val) {
                max_val = last_logits[v];
                next_token = v;
            }
        }

        /* Print and add to context */
        printf("%c", token_to_char(next_token, tokenizer));

        if (n_tokens < 256) {
            tokens[n_tokens++] = next_token;
        } else {
            /* Shift left */
            memmove(tokens, tokens + 1, 255 * sizeof(int));
            tokens[255] = next_token;
        }

        /* Reset arena */
        autograd_reset_iteration();
    }

    printf("\"\n");
}

int main(int argc, char *argv[]) {
    printf("=== FluxParser Transformer Training (Full Version) ===\n");
    printf("With dataset loading, checkpointing, and model saving\n\n");

    /* Get configuration */
    TrainingConfig config = get_default_config();

    /* Parse arguments */
    int use_tiny_dataset = 0;
    int use_resume = 0;
    char resume_path[512] = "";

    if (argc > 1) {
        if (strcmp(argv[1], "--resume") == 0) {
            use_resume = 1;
            if (argc > 2) {
                strncpy(resume_path, argv[2], sizeof(resume_path) - 1);
            } else {
                snprintf(resume_path, sizeof(resume_path), "models/checkpoint.iter_001000.ckpt");
            }
            printf("🔄 Resume mode: Loading from %s\n\n", resume_path);
        } else if (strcmp(argv[1], "--tiny") == 0) {
            /* Ultra-low memory: tiny model + tiny dataset */
            config.d_model = 64;
            config.n_heads = 2;
            config.n_layers = 1;
            config.d_ff = 128;
            config.seq_len = 32;
            config.n_iters = 2000;
            use_tiny_dataset = 1;
            printf("🔹 Tiny mode: Low memory, fast training\n\n");
        } else if (strcmp(argv[1], "--small") == 0) {
            config.d_model = 128;
            config.n_heads = 4;
            config.n_layers = 2;
            config.d_ff = 512;
            config.n_iters = 1000;  /* Safe: 1K iterations per run */
            use_tiny_dataset = 0;  /* Use full Shakespeare dataset */
        } else if (strcmp(argv[1], "--medium") == 0) {
            config.d_model = 256;
            config.n_heads = 8;
            config.n_layers = 4;
            config.d_ff = 1024;
        } else if (strcmp(argv[1], "--large") == 0) {
            config.d_model = 512;
            config.n_heads = 16;
            config.n_layers = 6;
            config.d_ff = 2048;
        } else {
            config.n_iters = atoi(argv[1]);
        }
    }

    /* Initialize */
    srand(time(NULL));
    autograd_v2_init();

    /* Load dataset */
    printf("Loading dataset...\n");
    fflush(stdout);
    CharTokenizer *tokenizer = NULL;
    Dataset *dataset = NULL;

    /* Try to load Shakespeare, but use fallback if it fails or for tiny mode */
    if (!use_tiny_dataset) {
        printf("[DEBUG] Attempting to load Shakespeare dataset...\n");
        fflush(stdout);
        dataset = load_shakespeare(&tokenizer);
        printf("[DEBUG] Shakespeare load returned: %p\n", (void*)dataset);
        fflush(stdout);
    }

    if (!dataset || use_tiny_dataset) {
        printf("⚠️  Shakespeare dataset not available. Using built-in small dataset.\n");
        printf("   For full Shakespeare (1MB): Download manually to data/shakespeare.txt\n");
        printf("   URL: https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt\n\n");

        /* Use small built-in dataset */
        const char *text =
            "To be or not to be, that is the question.\n"
            "Whether tis nobler in the mind to suffer\n"
            "The slings and arrows of outrageous fortune,\n"
            "Or to take arms against a sea of troubles\n"
            "And by opposing end them. To die, to sleep,\n"
            "No more, and by a sleep to say we end\n"
            "The heartache and the thousand natural shocks\n"
            "That flesh is heir to. Tis a consummation\n"
            "Devoutly to be wished. To die, to sleep,\n"
            "To sleep, perchance to dream. Ay, there's the rub.\n";

        tokenizer = create_char_tokenizer(text);
        dataset = malloc(sizeof(Dataset));
        dataset->length = strlen(text);
        dataset->tokens = malloc(dataset->length * sizeof(int));

        for (int i = 0; i < dataset->length; i++) {
            dataset->tokens[i] = char_to_token(text[i], tokenizer);
        }
    }

    config.vocab_size = tokenizer->vocab_size;
    printf("Dataset: %d tokens, vocab size: %d\n", dataset->length, config.vocab_size);
    printf("Memory usage: ~%.2f MB (dataset + model)\n\n",
           (dataset->length * sizeof(int) + 10 * 1024 * 1024) / 1024.0 / 1024.0);
    fflush(stdout);

    printf("[DEBUG] About to create model directory...\n");
    fflush(stdout);

    /* Create model directory */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", config.model_dir);
    system(cmd);

    /* Save tokenizer */
    char tokenizer_path[512];
    snprintf(tokenizer_path, sizeof(tokenizer_path),
             "%s/tokenizer.bin", config.model_dir);
    save_tokenizer(tokenizer, tokenizer_path);
    printf("Tokenizer saved to %s\n", tokenizer_path);

    /* Create or load model */
    TransformerV2 *model = NULL;
    AdamOptimizerV2 *optimizer = NULL;
    int start_iter = 0;
    double resume_loss = 0.0;

    if (use_resume) {
        /* Load from checkpoint */
        printf("Loading checkpoint from %s...\n", resume_path);
        if (checkpoint_load(&model, &optimizer, &start_iter, &resume_loss, resume_path) != 0) {
            fprintf(stderr, "Failed to load checkpoint!\n");
            return 1;
        }
        printf("✅ Resumed from iteration %d (loss was %.4f)\n\n", start_iter, resume_loss);
    } else {
        /* Create fresh model */
        printf("Creating transformer model...\n");
        printf("  Architecture: d=%d, heads=%d, layers=%d, ff=%d\n",
               config.d_model, config.n_heads, config.n_layers, config.d_ff);
        fflush(stdout);

        printf("[DEBUG] Calling transformer_create...\n");
        fflush(stdout);
        model = transformer_create(
            config.vocab_size, config.d_model, config.n_heads,
            config.n_layers, config.d_ff, config.max_seq_len
        );
        printf("[DEBUG] Model created: %p\n", (void*)model);
        fflush(stdout);

        /* Get parameters */
        VariableV2 **params;
        int n_params;
        transformer_get_params(model, &params, &n_params);

        /* Count total parameters */
        int total_params = 0;
        for (int i = 0; i < n_params; i++) {
            total_params += params[i]->data->size;
        }
        printf("  Total parameters: %d (%.2f M)\n\n", total_params,
               total_params / 1e6);

        /* Create optimizer */
        optimizer = adam_create(config.learning_rate);
        for (int i = 0; i < n_params; i++) {
            adam_add_param(optimizer, params[i]);
        }
        free(params);
    }

    /* Training loop */
    int end_iter = start_iter + config.n_iters;
    if (use_resume) {
        printf("Resuming training: iterations %d to %d...\n", start_iter, end_iter);
    } else {
        printf("Starting training for %d iterations...\n", config.n_iters);
    }
    printf("=====================================\n");
    fflush(stdout);

    double total_loss = 0.0;
    int loss_count = 0;
    time_t start_time = time(NULL);

    /* Allocate batch buffers */
    printf("[DEBUG] Allocating batch buffers...\n");
    fflush(stdout);
    int *batch_inputs = malloc(config.batch_size * config.seq_len * sizeof(int));
    int *batch_targets = malloc(config.batch_size * config.seq_len * sizeof(int));
    printf("[DEBUG] Starting training loop iteration %d...\n", start_iter);
    fflush(stdout);

    for (int iter = start_iter; iter < end_iter; iter++) {
        if (iter < 3) {
            printf("[DEBUG] Iteration %d: start\n", iter);
            fflush(stdout);
        }

        /* Update learning rate */
        double lr = get_learning_rate(iter, &config);
        optimizer->learning_rate = lr;

        if (iter < 3) {
            printf("[DEBUG] Iteration %d: getting batch\n", iter);
            fflush(stdout);
        }

        /* Get batch */
        get_batch(dataset, config.batch_size, config.seq_len,
                 batch_inputs, batch_targets);

        if (iter < 3) {
            printf("[DEBUG] Iteration %d: forward pass\n", iter);
            fflush(stdout);
        }

        /* Forward pass */
        VariableV2 *logits = transformer_forward(model, batch_inputs, config.seq_len);

        if (iter < 3) {
            printf("[DEBUG] Iteration %d: compute loss\n", iter);
            fflush(stdout);
        }

        /* Compute loss */
        VariableV2 *loss = compute_cross_entropy_loss(logits, batch_targets,
                                                      config.seq_len);

        if (iter < 3) {
            printf("[DEBUG] Iteration %d: loss computed, getting value\n", iter);
            fflush(stdout);
        }

        double loss_val = loss->data->data[0];
        total_loss += loss_val;
        loss_count++;

        if (iter < 3) {
            printf("[DEBUG] Iteration %d: loss=%.4f, starting backward\n", iter, loss_val);
            fflush(stdout);
        }

        /* Backward pass */
        loss->grad->data[0] = 1.0;
        tape_backward(g_tape);

        if (iter < 3) {
            printf("[DEBUG] Iteration %d: backward done, calling adam_step\n", iter);
            fflush(stdout);
        }

        /* Update weights */
        adam_step(optimizer);

        if (iter < 3) {
            printf("[DEBUG] Iteration %d: adam_step done\n", iter);
            fflush(stdout);
        }

        /* Logging */
        if ((iter + 1) % config.log_interval == 0) {
            double avg_loss = total_loss / loss_count;
            time_t current_time = time(NULL);
            double elapsed = difftime(current_time, start_time);
            double iters_per_sec = (iter + 1) / elapsed;

            printf("Iter %5d/%d | Loss: %.4f | LR: %.2e | Speed: %.1f it/s\n",
                   iter + 1, config.n_iters, avg_loss, lr, iters_per_sec);

            total_loss = 0.0;
            loss_count = 0;
        }

        /* Generate samples */
        if ((iter + 1) % config.sample_interval == 0) {
            generate_sample(model, tokenizer, "To be ", 50);
        }

        /* Checkpointing */
        if ((iter + 1) % config.checkpoint_interval == 0) {
            char checkpoint_path[512];
            snprintf(checkpoint_path, sizeof(checkpoint_path),
                    "%s/checkpoint", config.model_dir);
            checkpoint_save(model, optimizer, iter + 1,
                          loss_val, checkpoint_path);
        }

        /* Save model */
        if ((iter + 1) % config.save_interval == 0) {
            char model_path[512];
            snprintf(model_path, sizeof(model_path),
                    "%s/model_iter_%06d.bin", config.model_dir, iter + 1);
            transformer_save(model, model_path);
        }

        /* Reset arena at END of loop - after all allocations */
        if (iter < 5) {
            printf("[DEBUG] Iteration %d: calling arena reset\n", iter);
            fflush(stdout);
        }
        autograd_reset_iteration();
        if (iter < 5) {
            printf("[DEBUG] Iteration %d: arena reset done, loop end\n", iter);
            fflush(stdout);
        }
    }

    printf("=====================================\n");
    printf("Training complete!\n\n");

    /* Save final model */
    char final_model_path[512];
    snprintf(final_model_path, sizeof(final_model_path),
            "%s/model_final.bin", config.model_dir);
    transformer_save(model, final_model_path);

    /* Print usage instructions */
    printf("To generate text with the trained model:\n");
    printf("  ./generate %s %s --interactive\n",
           final_model_path, tokenizer_path);
    printf("  ./generate %s %s --prompt \"To be or not to be\"\n",
           final_model_path, tokenizer_path);

    /* Cleanup */
    free(batch_inputs);
    free(batch_targets);
    adam_free(optimizer);
    transformer_free(model);
    free_dataset(dataset);
    free_tokenizer(tokenizer);
    autograd_v2_cleanup();

    return 0;
}