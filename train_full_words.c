/*
 * train_full_words.c - Full transformer training with WORD-LEVEL tokenization
 * Uses WordTokenizer instead of CharTokenizer for better semantic understanding
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

/* Learning rate schedule with warmup and cosine annealing */
double get_learning_rate(int iter, TrainingConfig *config) {
    if (iter < config->warmup_iters) {
        /* Linear warmup */
        return config->learning_rate * ((double)iter / config->warmup_iters);
    } else {
        /* Cosine annealing with minimum LR floor
         * Prevents LR from decaying to zero, ensuring continued learning */
        double min_lr = config->learning_rate * 0.1;  /* Minimum 10% of base LR */
        double progress = (double)(iter - config->warmup_iters) /
                         (config->n_iters - config->warmup_iters);
        double cosine_decay = 0.5 * (1.0 + cos(M_PI * progress));

        /* Scale between min_lr and base_lr using cosine */
        return min_lr + (config->learning_rate - min_lr) * cosine_decay;
    }
}

/* Generate sample text during training */
void generate_sample_words(TransformerV2 *model, WordTokenizer *tokenizer,
                          const char *prompt, int max_words) {
    printf("  Sample: \"%s", prompt);

    /* Tokenize prompt by words */
    int tokens[256];
    int n_tokens = 0;

    char word_buf[256];
    int pos = 0;

    /* Extract words from prompt */
    while (prompt[pos] && n_tokens < 256) {
        /* Skip whitespace/punctuation */
        while (prompt[pos] && (prompt[pos] == ' ' || prompt[pos] == ',' || prompt[pos] == '.')) {
            pos++;
        }

        /* Extract word */
        int len = 0;
        while (prompt[pos] && prompt[pos] != ' ' && prompt[pos] != ',' && prompt[pos] != '.' && len < 255) {
            word_buf[len++] = (prompt[pos] >= 'A' && prompt[pos] <= 'Z') ?
                             (prompt[pos] + 32) : prompt[pos];  /* lowercase */
            pos++;
        }

        if (len > 0) {
            word_buf[len] = '\0';
            tokens[n_tokens++] = word_to_token(word_buf, tokenizer);
        }
    }

    /* Generate continuation */
    for (int i = 0; i < max_words; i++) {
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

        /* Temperature sampling (skip <UNK> = token 0) */
        double temperature = 0.8;

        /* Apply temperature and softmax */
        double max_logit = -INFINITY;
        for (int v = 1; v < model->vocab_size; v++) {  /* Skip <UNK> */
            if (last_logits[v] > max_logit) max_logit = last_logits[v];
        }

        double sum = 0.0;
        double probs[8000];
        for (int v = 1; v < model->vocab_size; v++) {
            probs[v] = exp((last_logits[v] - max_logit) / temperature);
            sum += probs[v];
        }

        /* Sample from distribution */
        double r = (double)rand() / RAND_MAX * sum;
        double cumsum = 0.0;
        int next_token = 1;  /* Default to first non-UNK word */
        for (int v = 1; v < model->vocab_size; v++) {
            cumsum += probs[v];
            if (cumsum >= r) {
                next_token = v;
                break;
            }
        }

        /* Print word and add to context */
        printf(" %s", token_to_word(next_token, tokenizer));

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
    int num_threads = 1;  /* Default: 1 thread (sequential) */
    int use_word_tokens = 0;  /* Default: character-level tokenization */

    /* Process all arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--resume") == 0) {
            use_resume = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                strncpy(resume_path, argv[i + 1], sizeof(resume_path) - 1);
                i++;  /* Skip next arg (path) */
            } else {
                snprintf(resume_path, sizeof(resume_path), "models/checkpoint.iter_001000.ckpt");
            }
            printf("🔄 Resume mode: Loading from %s\n\n", resume_path);
        } else if (strcmp(argv[i], "--tiny") == 0) {
            /* Ultra-low memory: tiny model + tiny dataset */
            config.d_model = 64;
            config.n_heads = 2;
            config.n_layers = 1;
            config.d_ff = 128;
            config.seq_len = 32;
            config.n_iters = 2000;
            use_tiny_dataset = 1;
            printf("🔹 Tiny mode: Low memory, fast training\n\n");
        } else if (strcmp(argv[i], "--small") == 0) {
            config.d_model = 128;
            config.n_heads = 4;
            config.n_layers = 2;
            config.d_ff = 512;
            config.n_iters = 1000;  /* Default: 1K iterations */
            use_tiny_dataset = 0;  /* Use full Shakespeare dataset */
        } else if (strcmp(argv[i], "--medium") == 0) {
            config.d_model = 192;
            config.n_heads = 6;
            config.n_layers = 3;
            config.d_ff = 768;
            config.n_iters = 10000;
            printf("🎯 Medium mode: 1M+ params, optimized for word-level\n\n");
        } else if (strcmp(argv[i], "--large") == 0) {
            config.d_model = 512;
            config.n_heads = 16;
            config.n_layers = 6;
            config.d_ff = 2048;
        } else if (strcmp(argv[i], "--batch-size") == 0) {
            if (i + 1 < argc) {
                config.batch_size = atoi(argv[i + 1]);
                printf("🔹 Batch size: %d samples per iteration\n\n", config.batch_size);
                i++;  /* Skip next arg (batch size value) */
            } else {
                fprintf(stderr, "Error: --batch-size requires a numeric argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) {
                num_threads = atoi(argv[i + 1]);
                if (num_threads < 1) num_threads = 1;
                printf("🔹 Threads: %d worker threads\n\n", num_threads);
                i++;  /* Skip next arg (thread count) */
            } else {
                fprintf(stderr, "Error: --threads requires a numeric argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--words") == 0) {
            use_word_tokens = 1;
            printf("🔹 Tokenization: Word-level\n\n");
        } else if (argv[i][0] != '-') {
            /* Numeric argument = iteration count */
            config.n_iters = atoi(argv[i]);
        }
    }

    /* Show final configuration */
    printf("Configuration: %d iterations, batch_size=%d, threads=%d\n\n",
           config.n_iters, config.batch_size, num_threads);

    /* Thread pool not yet implemented for word-level training */
    (void)num_threads;  /* Suppress unused warning */

    /* Initialize */
    srand(time(NULL));
    autograd_v2_init();

    /* Load dataset */
    printf("Loading dataset with WORD-LEVEL tokenization...\n");
    fflush(stdout);
    WordTokenizer *tokenizer = NULL;
    Dataset *dataset = NULL;

    /* Try to load Shakespeare, but use fallback if it fails or for tiny mode */
    if (!use_tiny_dataset) {
        dataset = load_shakespeare_words(&tokenizer);
    }

    if (!dataset || use_tiny_dataset) {
        printf("⚠️  Shakespeare dataset not available. Using built-in small dataset.\n");
        printf("   For full Shakespeare (1MB): Download manually to data/shakespeare.txt\n");
        printf("   URL: https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt\n\n");

        /* Use small built-in dataset with word tokenization */
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

        /* Create word tokenizer for built-in dataset */
        tokenizer = create_word_tokenizer(text, 1000);

        /* Tokenize text into words */
        char word_buf[256];
        int pos = 0;
        int word_count = 0;

        /* Count words */
        while (text[pos]) {
            while (text[pos] && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == ',' || text[pos] == '.')) pos++;
            if (!text[pos]) break;
            word_count++;
            while (text[pos] && text[pos] != ' ' && text[pos] != '\n' && text[pos] != ',' && text[pos] != '.') pos++;
        }

        dataset = malloc(sizeof(Dataset));
        dataset->length = word_count;
        dataset->tokens = malloc(dataset->length * sizeof(int));

        /* Tokenize */
        pos = 0;
        int idx = 0;
        while (text[pos] && idx < word_count) {
            while (text[pos] && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == ',' || text[pos] == '.')) pos++;
            if (!text[pos]) break;

            int len = 0;
            while (text[pos] && text[pos] != ' ' && text[pos] != '\n' && text[pos] != ',' && text[pos] != '.' && len < 255) {
                word_buf[len++] = (text[pos] >= 'A' && text[pos] <= 'Z') ? (text[pos] + 32) : text[pos];
                pos++;
            }
            word_buf[len] = '\0';

            if (len > 0) {
                dataset->tokens[idx++] = word_to_token(word_buf, tokenizer);
            }
        }
    }

    config.vocab_size = tokenizer->vocab_size;
    printf("Dataset: %d tokens, vocab size: %d\n", dataset->length, config.vocab_size);
    printf("Memory usage: ~%.2f MB (dataset + model)\n\n",
           (dataset->length * sizeof(int) + 10 * 1024 * 1024) / 1024.0 / 1024.0);

    /* Create model directory */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", config.model_dir);
    system(cmd);

    /* Note: Word tokenizer save/load not yet implemented */
    printf("Note: Using word-level tokenization (vocab size: %d)\n", tokenizer->vocab_size);

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

        model = transformer_create(
            config.vocab_size, config.d_model, config.n_heads,
            config.n_layers, config.d_ff, config.max_seq_len
        );

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
    int *batch_inputs = malloc(config.batch_size * config.seq_len * sizeof(int));
    int *batch_targets = malloc(config.batch_size * config.seq_len * sizeof(int));

    for (int iter = start_iter; iter < end_iter; iter++) {
        /* Update learning rate */
        double lr = get_learning_rate(iter, &config);
        optimizer->learning_rate = lr;

        /* Zero gradients from previous iteration */
        for (int p = 0; p < optimizer->n_params; p++) {
            VariableV2 *param = optimizer->params[p];
            if (param->grad) {
                for (int j = 0; j < param->data->size; j++) {
                    param->grad->data[j] = 0.0;
                }
            }
        }

        /* Get batch */
        get_batch(dataset, config.batch_size, config.seq_len,
                 batch_inputs, batch_targets);

        /* Process batch (accumulate gradients) */
        double batch_loss = 0.0;

        for (int b = 0; b < config.batch_size; b++) {
            /* Get this batch item's input/target */
            int *item_input = batch_inputs + b * config.seq_len;
            int *item_target = batch_targets + b * config.seq_len;

            /* Forward pass */
            VariableV2 *logits = transformer_forward(model, item_input, config.seq_len);

            /* Compute loss */
            VariableV2 *loss = compute_cross_entropy_loss(logits, item_target,
                                                          config.seq_len);

            double loss_val = loss->data->data[0];
            batch_loss += loss_val;

            /* Backward pass (accumulates gradients) */
            loss->grad->data[0] = 1.0;
            tape_backward(g_tape);

            /* Reset arena for next batch item (keeps gradients in param->grad) */
            autograd_reset_iteration();
        }

        /* Average loss over batch */
        double avg_batch_loss = batch_loss / config.batch_size;
        total_loss += avg_batch_loss;
        loss_count++;

        /* Scale gradients by 1/batch_size (since they've been accumulated) */
        if (config.batch_size > 1) {
            double scale = 1.0 / config.batch_size;
            for (int p = 0; p < optimizer->n_params; p++) {
                VariableV2 *param = optimizer->params[p];
                if (param->grad) {
                    for (int j = 0; j < param->data->size; j++) {
                        param->grad->data[j] *= scale;
                    }
                }
            }
        }

        /* Update weights with scaled gradients */
        adam_step(optimizer);

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
            generate_sample_words(model, tokenizer, "To be", 20);  /* Generate 20 words */
        }

        /* Checkpointing */
        if ((iter + 1) % config.checkpoint_interval == 0) {
            char checkpoint_path[512];
            snprintf(checkpoint_path, sizeof(checkpoint_path),
                    "%s/checkpoint", config.model_dir);
            checkpoint_save(model, optimizer, iter + 1,
                          avg_batch_loss, checkpoint_path);
        }

        /* Save model */
        if ((iter + 1) % config.save_interval == 0) {
            char model_path[512];
            snprintf(model_path, sizeof(model_path),
                    "%s/model_iter_%06d.bin", config.model_dir, iter + 1);
            transformer_save(model, model_path);
        }

        /* Reset arena at END of loop - after all allocations */
        autograd_reset_iteration();
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
    printf("  Note: Word-level generation not yet integrated with ./generate\n");
    printf("  Model saved to: %s\n", final_model_path);

    /* Cleanup */
    free(batch_inputs);
    free(batch_targets);
    adam_free(optimizer);
    transformer_free(model);
    free_dataset(dataset);
    free_word_tokenizer(tokenizer);
    autograd_v2_cleanup();

    return 0;
}