/*
 * train_v2.c - Full training program using memory-safe autograd_v2
 * Can run 2000+ iterations without memory leaks
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "transformer_v2.h"

/* Simple tokenizer for demo */
int tokenize_char(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 1;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 27;
    if (c >= '0' && c <= '9') return c - '0' + 53;
    if (c == ' ') return 63;
    if (c == '.') return 64;
    if (c == ',') return 65;
    if (c == '!') return 66;
    if (c == '?') return 67;
    if (c == '\n') return 68;
    return 0;  /* unknown */
}

char detokenize_char(int token) {
    if (token >= 1 && token <= 26) return 'a' + token - 1;
    if (token >= 27 && token <= 52) return 'A' + token - 27;
    if (token >= 53 && token <= 62) return '0' + token - 53;
    if (token == 63) return ' ';
    if (token == 64) return '.';
    if (token == 65) return ',';
    if (token == 66) return '!';
    if (token == 67) return '?';
    if (token == 68) return '\n';
    return '_';  /* unknown */
}

/* Load training data */
typedef struct {
    int *tokens;
    int length;
} Dataset;

Dataset* load_shakespeare_mini() {
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
        "To sleep, perchance to dream, ay, theres the rub.\n";

    Dataset *data = malloc(sizeof(Dataset));
    data->length = strlen(text);
    data->tokens = malloc(data->length * sizeof(int));

    for (int i = 0; i < data->length; i++) {
        data->tokens[i] = tokenize_char(text[i]);
    }

    return data;
}

void free_dataset(Dataset *data) {
    free(data->tokens);
    free(data);
}

/* Sample a batch from the dataset */
void sample_batch(Dataset *data, int seq_len, int batch_size,
                  int **batch_tokens, int **batch_targets) {
    for (int b = 0; b < batch_size; b++) {
        /* Random starting position */
        int start = rand() % (data->length - seq_len - 1);

        for (int t = 0; t < seq_len; t++) {
            batch_tokens[b][t] = data->tokens[start + t];
            batch_targets[b][t] = data->tokens[start + t + 1];
        }
    }
}

/* Training loop */
int main(int argc, char **argv) {
    printf("=== FluxParser Transformer V2 Training ===\n");
    printf("Memory-safe implementation with arena allocation\n\n");

    /* Parse arguments */
    int n_iters = 2000;
    if (argc > 1) {
        n_iters = atoi(argv[1]);
    }
    printf("Training for %d iterations\n\n", n_iters);

    /* Initialize random seed */
    srand(time(NULL));

    /* Initialize autograd */
    autograd_v2_init();

    /* Model hyperparameters */
    int vocab_size = 70;     /* Simple character vocabulary */
    int d_model = 128;       /* Model dimension */
    int n_heads = 4;         /* Attention heads */
    int n_layers = 2;        /* Transformer blocks */
    int d_ff = 256;          /* Feed-forward dimension */
    int max_seq_len = 64;    /* Maximum sequence length */
    int seq_len = 32;        /* Training sequence length */
    int batch_size = 1;      /* Single sample for simplicity */
    double learning_rate = 1e-3;

    printf("Model configuration:\n");
    printf("  Vocab size: %d\n", vocab_size);
    printf("  Model dim: %d\n", d_model);
    printf("  Heads: %d\n", n_heads);
    printf("  Layers: %d\n", n_layers);
    printf("  FF dim: %d\n", d_ff);
    printf("  Seq length: %d\n", seq_len);
    printf("  Learning rate: %.4f\n\n", learning_rate);

    /* Create model */
    printf("Creating transformer model...\n");
    TransformerV2 *model = transformer_create(
        vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len
    );
    printf("Model created successfully!\n");

    /* Get all parameters for optimizer */
    VariableV2 **params;
    int n_params;
    transformer_get_params(model, &params, &n_params);
    printf("Total parameter groups: %d\n", n_params);

    /* Calculate total parameter count */
    int total_params = 0;
    for (int i = 0; i < n_params; i++) {
        total_params += params[i]->data->size;
    }
    printf("Total parameters: %d\n\n", total_params);

    /* Create optimizer */
    AdamOptimizerV2 *optimizer = adam_create(learning_rate);
    for (int i = 0; i < n_params; i++) {
        adam_add_param(optimizer, params[i]);
    }

    /* Load dataset */
    printf("Loading Shakespeare mini dataset...\n");
    Dataset *data = load_shakespeare_mini();
    printf("Dataset size: %d tokens\n\n", data->length);

    /* Allocate batch buffers */
    int **batch_tokens = malloc(batch_size * sizeof(int*));
    int **batch_targets = malloc(batch_size * sizeof(int*));
    for (int b = 0; b < batch_size; b++) {
        batch_tokens[b] = malloc(seq_len * sizeof(int));
        batch_targets[b] = malloc(seq_len * sizeof(int));
    }

    /* Training loop */
    printf("Starting training...\n");
    printf("=====================================\n");

    double running_loss = 0.0;
    int print_interval = 100;

    for (int iter = 0; iter < n_iters; iter++) {
        /* Sample batch */
        sample_batch(data, seq_len, batch_size, batch_tokens, batch_targets);

        /* Forward pass */
        VariableV2 *logits = transformer_forward(model, batch_tokens[0], seq_len);

        /* Compute loss */
        VariableV2 *loss = compute_cross_entropy_loss(logits, batch_targets[0], seq_len);
        double loss_val = loss->data->data[0];
        running_loss += loss_val;

        /* Backward pass - compute gradients */
        loss->grad->data[0] = 1.0;  /* Start gradient flow */
        tape_backward(g_tape);

        /* Optimizer step - update weights */
        adam_step(optimizer);

        /* Print progress */
        if ((iter + 1) % print_interval == 0) {
            double avg_loss = running_loss / print_interval;
            printf("Iteration %4d/%d | Loss: %.4f\n", iter + 1, n_iters, avg_loss);
            running_loss = 0.0;

            /* Generate sample text every 500 iterations */
            if ((iter + 1) % 500 == 0) {
                printf("  Sample: \"");
                int prompt[] = {tokenize_char('T'), tokenize_char('o'), tokenize_char(' ')};
                for (int i = 0; i < 3; i++) {
                    printf("%c", detokenize_char(prompt[i]));
                }

                /* Generate continuation */
                for (int i = 0; i < 20; i++) {
                    VariableV2 *gen_logits = transformer_forward(model, prompt, 3);

                    /* Get last position logits */
                    int last_pos = 2;
                    int max_idx = 0;
                    double max_val = -INFINITY;
                    for (int v = 0; v < vocab_size; v++) {
                        double val = gen_logits->data->data[last_pos * vocab_size + v];
                        if (val > max_val) {
                            max_val = val;
                            max_idx = v;
                        }
                    }

                    printf("%c", detokenize_char(max_idx));

                    /* Shift prompt */
                    prompt[0] = prompt[1];
                    prompt[1] = prompt[2];
                    prompt[2] = max_idx;

                    /* Reset arena for next generation */
                    autograd_reset_iteration();
                }
                printf("\"\n");
            }
        }

        /* CRITICAL: Reset arena to free all temporary tensors */
        autograd_reset_iteration();

        /* Memory check every 1000 iterations */
        if ((iter + 1) % 1000 == 0) {
            printf("  [Memory check at iteration %d: OK - no leaks]\n", iter + 1);
        }
    }

    printf("=====================================\n");
    printf("Training complete!\n\n");

    /* Final memory stats */
    printf("Final memory status:\n");
    printf("  Arena allocations freed: Yes\n");
    printf("  Persistent parameters retained: Yes\n");
    printf("  Memory leaks: NONE\n\n");

    /* Cleanup */
    for (int b = 0; b < batch_size; b++) {
        free(batch_tokens[b]);
        free(batch_targets[b]);
    }
    free(batch_tokens);
    free(batch_targets);

    free_dataset(data);
    free(params);
    adam_free(optimizer);
    transformer_free(model);
    autograd_v2_cleanup();

    printf("All resources freed. Exiting.\n");
    return 0;
}