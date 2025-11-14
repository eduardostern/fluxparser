/*
 * generate.c - Text generation with trained transformer model
 * Inference mode with temperature sampling and beam search
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "transformer_v2.h"
#include "model_io_v2.h"
#include "dataset.h"

/* Sample from probability distribution */
int sample_categorical(double *probs, int n) {
    double r = (double)rand() / RAND_MAX;
    double cumsum = 0.0;

    for (int i = 0; i < n; i++) {
        cumsum += probs[i];
        if (r < cumsum) {
            return i;
        }
    }
    return n - 1;
}

/* Apply temperature to logits and sample */
int sample_with_temperature(double *logits, int vocab_size, double temperature) {
    /* Apply temperature */
    double scaled_logits[vocab_size];
    for (int i = 0; i < vocab_size; i++) {
        scaled_logits[i] = logits[i] / temperature;
    }

    /* Compute softmax */
    double max_logit = -INFINITY;
    for (int i = 0; i < vocab_size; i++) {
        if (scaled_logits[i] > max_logit) {
            max_logit = scaled_logits[i];
        }
    }

    double sum = 0.0;
    double probs[vocab_size];
    for (int i = 0; i < vocab_size; i++) {
        probs[i] = exp(scaled_logits[i] - max_logit);
        sum += probs[i];
    }

    for (int i = 0; i < vocab_size; i++) {
        probs[i] /= sum;
    }

    /* Sample from distribution */
    return sample_categorical(probs, vocab_size);
}

/* Top-k sampling */
int sample_top_k(double *logits, int vocab_size, int k, double temperature) {
    typedef struct {
        int idx;
        double val;
    } IndexValue;

    IndexValue items[vocab_size];
    for (int i = 0; i < vocab_size; i++) {
        items[i].idx = i;
        items[i].val = logits[i];
    }

    /* Partial sort to get top k */
    for (int i = 0; i < k && i < vocab_size; i++) {
        for (int j = i + 1; j < vocab_size; j++) {
            if (items[j].val > items[i].val) {
                IndexValue temp = items[i];
                items[i] = items[j];
                items[j] = temp;
            }
        }
    }

    /* Sample from top k with temperature */
    double top_k_logits[k];
    for (int i = 0; i < k && i < vocab_size; i++) {
        top_k_logits[i] = items[i].val;
    }

    int sampled_idx = sample_with_temperature(top_k_logits,
                                              k < vocab_size ? k : vocab_size,
                                              temperature);

    return items[sampled_idx].idx;
}

/* Generate text with model */
void generate_text(TransformerV2 *model, CharTokenizer *tokenizer,
                  const char *prompt, int max_length,
                  double temperature, int top_k) {
    /* Tokenize prompt */
    int prompt_len = strlen(prompt);
    int context[1024];
    int context_len = 0;

    for (int i = 0; i < prompt_len && context_len < 1024; i++) {
        context[context_len++] = char_to_token(prompt[i], tokenizer);
    }

    printf("Prompt: \"%s\"\n", prompt);
    printf("Generating (temp=%.1f, top_k=%d):\n", temperature, top_k);
    printf("=====================================\n");
    printf("%s", prompt);
    fflush(stdout);

    /* Generate tokens */
    for (int step = 0; step < max_length; step++) {
        /* Get context window (last max_seq_len tokens) */
        int start = 0;
        int window_len = context_len;
        if (context_len > model->max_seq_len) {
            start = context_len - model->max_seq_len;
            window_len = model->max_seq_len;
        }

        /* Forward pass */
        VariableV2 *logits = transformer_forward(model, context + start, window_len);

        /* Get logits for last position */
        double *last_logits = logits->data->data + (window_len - 1) * model->vocab_size;

        /* Sample next token */
        int next_token;
        if (temperature == 0.0) {
            /* Greedy decoding */
            next_token = 0;
            double max_val = -INFINITY;
            for (int i = 0; i < model->vocab_size; i++) {
                if (last_logits[i] > max_val) {
                    max_val = last_logits[i];
                    next_token = i;
                }
            }
        } else if (top_k > 0) {
            /* Top-k sampling */
            next_token = sample_top_k(last_logits, model->vocab_size, top_k, temperature);
        } else {
            /* Temperature sampling */
            next_token = sample_with_temperature(last_logits, model->vocab_size, temperature);
        }

        /* Add to context */
        if (context_len < 1024) {
            context[context_len++] = next_token;
        } else {
            /* Shift context left */
            memmove(context, context + 1, (context_len - 1) * sizeof(int));
            context[context_len - 1] = next_token;
        }

        /* Print character */
        char c = token_to_char(next_token, tokenizer);
        printf("%c", c);
        fflush(stdout);

        /* Stop at end of text markers */
        if (c == '\0' || c == '\x04') {
            break;
        }

        /* Reset arena for next iteration */
        autograd_reset_iteration();
    }

    printf("\n=====================================\n");
}

/* Interactive generation mode */
void interactive_mode(TransformerV2 *model, CharTokenizer *tokenizer) {
    char prompt[1024];
    double temperature = 1.0;
    int top_k = 40;
    int max_length = 200;

    printf("\n=== Interactive Generation Mode ===\n");
    printf("Commands:\n");
    printf("  /temp <value>  - Set temperature (0.1-2.0)\n");
    printf("  /topk <value>  - Set top-k sampling (0=off)\n");
    printf("  /len <value>   - Set max generation length\n");
    printf("  /quit          - Exit\n");
    printf("  <text>         - Generate from prompt\n\n");

    while (1) {
        printf("> ");
        if (!fgets(prompt, sizeof(prompt), stdin)) {
            break;
        }

        /* Remove newline */
        prompt[strcspn(prompt, "\n")] = '\0';

        /* Check commands */
        if (strncmp(prompt, "/quit", 5) == 0) {
            break;
        } else if (strncmp(prompt, "/temp ", 6) == 0) {
            temperature = atof(prompt + 6);
            printf("Temperature set to %.1f\n", temperature);
        } else if (strncmp(prompt, "/topk ", 6) == 0) {
            top_k = atoi(prompt + 6);
            printf("Top-k set to %d\n", top_k);
        } else if (strncmp(prompt, "/len ", 5) == 0) {
            max_length = atoi(prompt + 5);
            printf("Max length set to %d\n", max_length);
        } else if (prompt[0] != '\0') {
            /* Generate text */
            generate_text(model, tokenizer, prompt, max_length, temperature, top_k);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <model_file> [tokenizer_file]\n", argv[0]);
        printf("       %s <model_file> --prompt \"text\"\n", argv[0]);
        printf("       %s <model_file> --interactive\n", argv[0]);
        return 1;
    }

    /* Initialize */
    srand(time(NULL));
    autograd_v2_init();

    /* Load tokenizer */
    CharTokenizer *tokenizer = NULL;
    if (argc > 2 && strcmp(argv[2], "--prompt") != 0 && strcmp(argv[2], "--interactive") != 0) {
        tokenizer = load_tokenizer(argv[2]);
        if (!tokenizer) {
            fprintf(stderr, "Failed to load tokenizer from %s\n", argv[2]);
            return 1;
        }
    } else {
        /* Use default tokenizer */
        printf("Loading default tokenizer...\n");
        tokenizer = load_tokenizer("models/tokenizer.bin");
        if (!tokenizer) {
            /* Create basic ASCII tokenizer */
            char ascii[128];
            for (int i = 0; i < 128; i++) {
                ascii[i] = i;
            }
            tokenizer = create_char_tokenizer(ascii);
        }
    }

    printf("Tokenizer loaded: %d tokens\n", tokenizer->vocab_size);

    /* Create model with same architecture */
    printf("Loading model from %s...\n", argv[1]);

    /* First, try to detect architecture from file */
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Cannot open model file: %s\n", argv[1]);
        return 1;
    }

    /* Skip magic and version */
    fseek(f, 8, SEEK_SET);

    /* Read architecture */
    int vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len;
    fread(&vocab_size, sizeof(int), 1, f);
    fread(&d_model, sizeof(int), 1, f);
    fread(&n_heads, sizeof(int), 1, f);
    fread(&n_layers, sizeof(int), 1, f);
    fread(&d_ff, sizeof(int), 1, f);
    fread(&max_seq_len, sizeof(int), 1, f);
    fclose(f);

    /* Create model */
    TransformerV2 *model = transformer_create(vocab_size, d_model, n_heads,
                                              n_layers, d_ff, max_seq_len);

    /* Load weights */
    if (transformer_load(model, argv[1]) != 0) {
        fprintf(stderr, "Failed to load model\n");
        return 1;
    }

    /* Check mode */
    if (argc > 2) {
        if (strcmp(argv[2], "--interactive") == 0) {
            interactive_mode(model, tokenizer);
        } else if (strcmp(argv[2], "--prompt") == 0 && argc > 3) {
            generate_text(model, tokenizer, argv[3], 200, 1.0, 40);
        }
    } else {
        /* Default: interactive mode */
        interactive_mode(model, tokenizer);
    }

    /* Cleanup */
    transformer_free(model);
    free_tokenizer(tokenizer);
    autograd_v2_cleanup();

    return 0;
}