/*
 * train_bpe.c - Training with BPE tokenization (like GPT)
 *
 * BPE provides the best of both worlds:
 * - Common words/phrases as single tokens (fast learning)
 * - Rare words split into subwords (no OOV)
 * - Typical compression: 3-4x vs characters
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "transformer_v2.h"
#include "model_io_v2.h"
#include "bpe_tokenizer.h"

/* Load Shakespeare text */
static char* load_text(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *text = malloc(size + 1);
    fread(text, 1, size, f);
    text[size] = '\0';
    fclose(f);

    return text;
}

int main(int argc, char *argv[]) {
    printf("=== BPE Tokenizer Training (GPT-style) ===\n\n");

    /* Configuration */
    int bpe_vocab_size = 500;   /* Small vocab for fast training */
    int d_model = 256;
    int n_heads = 8;
    int n_layers = 4;
    int d_ff = 1024;
    int seq_len = 64;
    int n_iters = 50000;
    double base_lr = 5e-4;

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--vocab") == 0 && i + 1 < argc) {
            bpe_vocab_size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--small") == 0) {
            d_model = 128; n_heads = 4; n_layers = 2; d_ff = 512;
            n_iters = 20000;
        } else if (strcmp(argv[i], "--medium") == 0) {
            d_model = 256; n_heads = 8; n_layers = 4; d_ff = 1024;
            n_iters = 50000;
        } else if (argv[i][0] != '-') {
            n_iters = atoi(argv[i]);
        }
    }

    printf("BPE vocab size: %d\n", bpe_vocab_size);
    printf("Model: d=%d, heads=%d, layers=%d\n", d_model, n_heads, n_layers);
    printf("Training: %d iterations\n\n", n_iters);

    /* Load text */
    char *text = load_text("data/shakespeare.txt");
    if (!text) {
        fprintf(stderr, "Error: Cannot load data/shakespeare.txt\n");
        return 1;
    }
    int text_len = strlen(text);
    printf("Loaded %d bytes of text\n", text_len);

    /* Create or load BPE tokenizer */
    BPETokenizer *tok = NULL;
    FILE *tok_file = fopen("models/bpe_tokenizer.bin", "rb");
    if (tok_file) {
        fclose(tok_file);
        printf("Loading existing BPE tokenizer...\n");
        tok = bpe_load("models/bpe_tokenizer.bin");
    }

    if (!tok) {
        printf("Building BPE tokenizer from scratch...\n");
        tok = bpe_create(text, bpe_vocab_size);
        system("mkdir -p models");
        bpe_save(tok, "models/bpe_tokenizer.bin");
        printf("Tokenizer saved to models/bpe_tokenizer.bin\n");
    }

    printf("Vocab size: %d, Merges: %d\n\n", tok->vocab_size, tok->n_merges);

    /* Show some vocabulary examples */
    printf("Sample vocabulary:\n");
    for (int i = 0; i < tok->vocab_size && i < 30; i++) {
        const char *t = bpe_get_token(tok, i);
        if (strlen(t) > 1) {
            printf("  %3d: '%s'\n", i, t);
        }
    }
    printf("  ...\n\n");

    /* Tokenize full text */
    printf("Tokenizing text...\n");
    int n_tokens;
    int *tokens = bpe_encode(tok, text, &n_tokens);
    printf("Tokenized: %d tokens (%.1fx compression vs chars)\n\n",
           n_tokens, (float)text_len / n_tokens);

    free(text);

    /* Initialize autograd */
    srand(time(NULL));
    autograd_v2_init();

    /* Create model */
    printf("Creating transformer model...\n");
    TransformerV2 *model = transformer_create(
        tok->vocab_size, d_model, n_heads, n_layers, d_ff, 128
    );

    VariableV2 **params;
    int n_params;
    transformer_get_params(model, &params, &n_params);

    int total_params = 0;
    for (int i = 0; i < n_params; i++) {
        total_params += params[i]->data->size;
    }
    printf("Parameters: %d (%.2f M)\n\n", total_params, total_params / 1e6);

    /* Create optimizer */
    AdamOptimizerV2 *optimizer = adam_create(base_lr);
    for (int i = 0; i < n_params; i++) {
        adam_add_param(optimizer, params[i]);
    }
    free(params);

    /* Training loop */
    printf("Starting training...\n");
    printf("=========================================\n");
    fflush(stdout);

    int *batch_input = malloc(seq_len * sizeof(int));
    int *batch_target = malloc(seq_len * sizeof(int));

    time_t start_time = time(NULL);
    double best_loss = 999.0;
    double total_loss = 0.0;
    int loss_count = 0;

    for (int iter = 0; iter < n_iters; iter++) {
        /* Learning rate schedule */
        double lr;
        int warmup = 500;
        if (iter < warmup) {
            lr = base_lr * ((double)iter / warmup);
        } else {
            double min_lr = base_lr * 0.1;
            double progress = (double)(iter - warmup) / (n_iters - warmup);
            double cosine = 0.5 * (1.0 + cos(M_PI * progress));
            lr = min_lr + (base_lr - min_lr) * cosine;
        }
        optimizer->learning_rate = lr;

        /* Zero gradients */
        for (int p = 0; p < optimizer->n_params; p++) {
            if (optimizer->params[p]->grad) {
                for (int j = 0; j < optimizer->params[p]->data->size; j++) {
                    optimizer->params[p]->grad->data[j] = 0.0;
                }
            }
        }

        /* Random batch */
        int start = rand() % (n_tokens - seq_len - 1);
        for (int i = 0; i < seq_len; i++) {
            batch_input[i] = tokens[start + i];
            batch_target[i] = tokens[start + i + 1];
        }

        /* Forward */
        VariableV2 *logits = transformer_forward(model, batch_input, seq_len);
        VariableV2 *loss = compute_cross_entropy_loss(logits, batch_target, seq_len);
        double loss_val = loss->data->data[0];
        total_loss += loss_val;
        loss_count++;

        if (loss_val < best_loss) best_loss = loss_val;

        /* Backward */
        loss->grad->data[0] = 1.0;
        tape_backward(g_tape);

        /* Update */
        adam_step(optimizer);
        autograd_reset_iteration();

        /* Logging */
        if ((iter + 1) % 500 == 0) {
            double avg_loss = total_loss / loss_count;
            time_t now = time(NULL);
            double elapsed = difftime(now, start_time);
            double speed = (iter + 1) / elapsed;
            double eta = (n_iters - iter - 1) / speed;

            printf("Iter %5d/%d | Loss: %.4f | Best: %.4f | LR: %.2e | Speed: %.1f it/s | ETA: %.0fs\n",
                   iter + 1, n_iters, avg_loss, best_loss, lr, speed, eta);
            fflush(stdout);

            total_loss = 0.0;
            loss_count = 0;
        }

        /* Generate sample */
        if ((iter + 1) % 5000 == 0) {
            printf("  Sample: \"");

            /* Start with "To " */
            int ctx[128];
            int ctx_len = 0;

            /* Encode prompt */
            int prompt_len;
            int *prompt_tokens = bpe_encode(tok, "To ", &prompt_len);
            for (int i = 0; i < prompt_len && ctx_len < 128; i++) {
                ctx[ctx_len++] = prompt_tokens[i];
            }
            free(prompt_tokens);

            /* Generate */
            for (int g = 0; g < 30 && ctx_len < 128; g++) {
                VariableV2 *out = transformer_forward(model, ctx, ctx_len);
                double *last = out->data->data + (ctx_len - 1) * tok->vocab_size;

                /* Temperature sampling */
                double temp = 0.8;
                double max_l = -1e30;
                for (int v = 0; v < tok->vocab_size; v++) {
                    if (last[v] > max_l) max_l = last[v];
                }
                double sum = 0;
                for (int v = 0; v < tok->vocab_size; v++) {
                    sum += exp((last[v] - max_l) / temp);
                }
                double r = (double)rand() / RAND_MAX * sum;
                double cum = 0;
                int next = 0;
                for (int v = 0; v < tok->vocab_size; v++) {
                    cum += exp((last[v] - max_l) / temp);
                    if (cum >= r) { next = v; break; }
                }

                printf("%s", bpe_get_token(tok, next));
                ctx[ctx_len++] = next;
                autograd_reset_iteration();
            }
            printf("\"\n\n");
            fflush(stdout);
        }
    }

    printf("=========================================\n");
    printf("Training complete!\n");
    printf("Best loss: %.4f (perplexity: %.1f)\n", best_loss, exp(best_loss));

    /* Save model */
    transformer_save(model, "models/model_bpe.bin");
    printf("Model saved to models/model_bpe.bin\n");

    /* Cleanup */
    free(batch_input);
    free(batch_target);
    free(tokens);
    adam_free(optimizer);
    transformer_free(model);
    bpe_free(tok);
    autograd_v2_cleanup();

    return 0;
}
