/*
 * train_long.c - Long training run optimized for low loss
 * Target: loss < 2.5 for coherent text generation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "transformer_v2.h"
#include "model_io_v2.h"
#include "dataset.h"

int main(int argc, char *argv[]) {
    printf("=== Long Training Run for Coherent Text ===\n");
    printf("Target: loss < 2.5 (perplexity < 12)\n\n");

    /* Optimized architecture: 3M params */
    int vocab_size = 66;
    int d_model = 256;
    int n_heads = 8;
    int n_layers = 4;
    int d_ff = 1024;
    int max_seq_len = 128;
    int seq_len = 64;
    int n_iters = 100000;
    double base_lr = 5e-4;
    
    /* Parse iterations from command line */
    if (argc > 1) {
        n_iters = atoi(argv[1]);
    }
    
    printf("Architecture: d=%d, heads=%d, layers=%d, ff=%d\n", 
           d_model, n_heads, n_layers, d_ff);
    printf("Training: %d iterations, seq_len=%d\n\n", n_iters, seq_len);

    srand(time(NULL));
    autograd_v2_init();

    /* Load dataset */
    CharTokenizer *tokenizer = NULL;
    Dataset *dataset = load_shakespeare(&tokenizer);
    if (!dataset) {
        fprintf(stderr, "Failed to load dataset\n");
        return 1;
    }
    vocab_size = tokenizer->vocab_size;
    printf("Dataset: %d tokens, vocab: %d\n", dataset->length, vocab_size);

    /* Create model */
    TransformerV2 *model = transformer_create(
        vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len
    );

    VariableV2 **params;
    int n_params;
    transformer_get_params(model, &params, &n_params);

    int total_params = 0;
    for (int i = 0; i < n_params; i++) {
        total_params += params[i]->data->size;
    }
    printf("Parameters: %d (%.2f M)\n\n", total_params, total_params / 1e6);
    fflush(stdout);

    /* Create optimizer */
    AdamOptimizerV2 *optimizer = adam_create(base_lr);
    for (int i = 0; i < n_params; i++) {
        adam_add_param(optimizer, params[i]);
    }
    free(params);

    /* Create model directory */
    system("mkdir -p models");

    /* Training loop */
    printf("Starting training...\n");
    printf("=========================================\n");
    
    int *inputs = malloc(seq_len * sizeof(int));
    int *targets = malloc(seq_len * sizeof(int));
    
    time_t start_time = time(NULL);
    double best_loss = 999.0;
    double total_loss = 0.0;
    int loss_count = 0;

    for (int iter = 0; iter < n_iters; iter++) {
        /* Cosine learning rate with warmup and 10% floor */
        double lr;
        int warmup = 1000;
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

        /* Get batch */
        get_batch(dataset, 1, seq_len, inputs, targets);

        /* Forward */
        VariableV2 *logits = transformer_forward(model, inputs, seq_len);
        VariableV2 *loss = compute_cross_entropy_loss(logits, targets, seq_len);
        double loss_val = loss->data->data[0];
        total_loss += loss_val;
        loss_count++;

        /* Backward */
        loss->grad->data[0] = 1.0;
        tape_backward(g_tape);

        /* Update */
        adam_step(optimizer);
        autograd_reset_iteration();

        /* Logging every 500 iterations */
        if ((iter + 1) % 500 == 0) {
            double avg_loss = total_loss / loss_count;
            time_t now = time(NULL);
            double elapsed = difftime(now, start_time);
            double speed = (iter + 1) / elapsed;
            double eta = (n_iters - iter - 1) / speed;
            
            printf("Iter %6d/%d | Loss: %.4f | LR: %.2e | Speed: %.1f it/s | ETA: %.0fs\n",
                   iter + 1, n_iters, avg_loss, lr, speed, eta);
            fflush(stdout);
            
            if (avg_loss < best_loss) {
                best_loss = avg_loss;
            }
            
            total_loss = 0.0;
            loss_count = 0;
            
            /* Early stopping check */
            if (avg_loss < 2.5) {
                printf("\n🎉 TARGET REACHED! Loss %.4f < 2.5\n", avg_loss);
                printf("Saving model and generating samples...\n\n");
                
                /* Save checkpoint */
                checkpoint_save(model, optimizer, iter + 1, avg_loss, "models/target_reached");
                break;
            }
        }

        /* Generate sample every 5000 iterations */
        if ((iter + 1) % 5000 == 0) {
            printf("  Sample: \"");
            int ctx[128] = {0};
            ctx[0] = char_to_token('T', tokenizer);
            ctx[1] = char_to_token('o', tokenizer);
            ctx[2] = char_to_token(' ', tokenizer);
            int ctx_len = 3;
            
            for (int i = 0; i < 60; i++) {
                VariableV2 *out = transformer_forward(model, ctx, ctx_len);
                double *last = out->data->data + (ctx_len - 1) * vocab_size;
                
                /* Temperature sampling */
                double temp = 0.8;
                double max_l = -1e30;
                for (int v = 0; v < vocab_size; v++) {
                    if (last[v] > max_l) max_l = last[v];
                }
                double sum = 0;
                for (int v = 0; v < vocab_size; v++) {
                    sum += exp((last[v] - max_l) / temp);
                }
                double r = (double)rand() / RAND_MAX * sum;
                double cum = 0;
                int next = 0;
                for (int v = 0; v < vocab_size; v++) {
                    cum += exp((last[v] - max_l) / temp);
                    if (cum >= r) { next = v; break; }
                }
                
                printf("%c", token_to_char(next, tokenizer));
                
                if (ctx_len < 128) {
                    ctx[ctx_len++] = next;
                } else {
                    memmove(ctx, ctx + 1, 127 * sizeof(int));
                    ctx[127] = next;
                }
                autograd_reset_iteration();
            }
            printf("\"\n\n");
        }

        /* Checkpoint every 10000 iterations */
        if ((iter + 1) % 10000 == 0) {
            char path[256];
            snprintf(path, sizeof(path), "models/long_train_%06d", iter + 1);
            checkpoint_save(model, optimizer, iter + 1, best_loss, path);
        }
    }

    time_t end_time = time(NULL);
    double total_time = difftime(end_time, start_time);
    
    printf("=========================================\n");
    printf("Training complete!\n");
    printf("Total time: %.1f minutes\n", total_time / 60.0);
    printf("Best loss: %.4f\n", best_loss);
    printf("Final model saved to: models/model_final.bin\n");

    transformer_save(model, "models/model_final.bin");

    /* Cleanup */
    free(inputs);
    free(targets);
    adam_free(optimizer);
    transformer_free(model);
    free_dataset(dataset);
    free_tokenizer(tokenizer);
    autograd_v2_cleanup();

    return 0;
}
