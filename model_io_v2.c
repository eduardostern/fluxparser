/*
 * model_io_v2.c - Model serialization for Autograd V2
 * Save and load transformer models with arena-allocated parameters
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transformer_v2.h"
#include "model_io_v2.h"

#define MODEL_MAGIC 0x464C5558  // "FLUX" in hex
#define MODEL_VERSION 2

/* Save transformer model to binary file */
int transformer_save(TransformerV2 *model, const char *filepath) {
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filepath);
        return -1;
    }

    /* Write header */
    uint32_t magic = MODEL_MAGIC;
    uint32_t version = MODEL_VERSION;
    fwrite(&magic, sizeof(uint32_t), 1, f);
    fwrite(&version, sizeof(uint32_t), 1, f);

    /* Write architecture */
    fwrite(&model->vocab_size, sizeof(int), 1, f);
    fwrite(&model->d_model, sizeof(int), 1, f);
    fwrite(&model->n_heads, sizeof(int), 1, f);
    fwrite(&model->n_layers, sizeof(int), 1, f);
    fwrite(&model->d_ff, sizeof(int), 1, f);
    fwrite(&model->max_seq_len, sizeof(int), 1, f);

    /* Get all parameters */
    VariableV2 **params;
    int n_params;
    transformer_get_params(model, &params, &n_params);

    /* Write parameter count */
    fwrite(&n_params, sizeof(int), 1, f);

    /* Write each parameter */
    for (int i = 0; i < n_params; i++) {
        VariableV2 *param = params[i];
        TensorV2 *tensor = param->data;

        /* Write tensor metadata */
        fwrite(&tensor->rank, sizeof(int), 1, f);
        fwrite(tensor->shape, sizeof(int), tensor->rank, f);
        fwrite(&tensor->size, sizeof(int), 1, f);

        /* Write tensor data */
        fwrite(tensor->data, sizeof(double), tensor->size, f);
    }

    fclose(f);
    free(params);

    /* Calculate file size */
    FILE *check = fopen(filepath, "rb");
    fseek(check, 0, SEEK_END);
    long file_size = ftell(check);
    fclose(check);

    printf("✅ Model saved to %s (%.2f MB)\n", filepath, file_size / 1024.0 / 1024.0);
    printf("   Architecture: vocab=%d, d_model=%d, heads=%d, layers=%d\n",
           model->vocab_size, model->d_model, model->n_heads, model->n_layers);
    printf("   Parameters: %d tensors, %d total values\n", n_params,
           model->vocab_size * model->d_model + /* embeddings */
           model->d_model * model->vocab_size + /* output projection */
           n_params * model->d_model); /* rough estimate */

    return 0;
}

/* Load transformer model from binary file */
int transformer_load(TransformerV2 *model, const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s for reading\n", filepath);
        return -1;
    }

    /* Check header */
    uint32_t magic, version;
    fread(&magic, sizeof(uint32_t), 1, f);
    fread(&version, sizeof(uint32_t), 1, f);

    if (magic != MODEL_MAGIC) {
        fprintf(stderr, "Error: Invalid model file (bad magic number)\n");
        fclose(f);
        return -1;
    }

    if (version != MODEL_VERSION) {
        fprintf(stderr, "Error: Incompatible model version (got %d, expected %d)\n",
                version, MODEL_VERSION);
        fclose(f);
        return -1;
    }

    /* Read and verify architecture */
    int vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len;
    fread(&vocab_size, sizeof(int), 1, f);
    fread(&d_model, sizeof(int), 1, f);
    fread(&n_heads, sizeof(int), 1, f);
    fread(&n_layers, sizeof(int), 1, f);
    fread(&d_ff, sizeof(int), 1, f);
    fread(&max_seq_len, sizeof(int), 1, f);

    /* Verify architecture matches */
    if (vocab_size != model->vocab_size || d_model != model->d_model ||
        n_heads != model->n_heads || n_layers != model->n_layers ||
        d_ff != model->d_ff || max_seq_len != model->max_seq_len) {
        fprintf(stderr, "Error: Model architecture mismatch!\n");
        fprintf(stderr, "  File:  vocab=%d, d=%d, heads=%d, layers=%d, ff=%d, seq=%d\n",
                vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len);
        fprintf(stderr, "  Model: vocab=%d, d=%d, heads=%d, layers=%d, ff=%d, seq=%d\n",
                model->vocab_size, model->d_model, model->n_heads,
                model->n_layers, model->d_ff, model->max_seq_len);
        fclose(f);
        return -1;
    }

    /* Get model parameters */
    VariableV2 **params;
    int n_params_model;
    transformer_get_params(model, &params, &n_params_model);

    /* Read parameter count */
    int n_params_file;
    fread(&n_params_file, sizeof(int), 1, f);

    if (n_params_file != n_params_model) {
        fprintf(stderr, "Error: Parameter count mismatch (file=%d, model=%d)\n",
                n_params_file, n_params_model);
        free(params);
        fclose(f);
        return -1;
    }

    /* Load each parameter */
    for (int i = 0; i < n_params_model; i++) {
        VariableV2 *param = params[i];
        TensorV2 *tensor = param->data;

        /* Read and verify tensor metadata */
        int rank, size;
        int shape[8];
        fread(&rank, sizeof(int), 1, f);
        fread(shape, sizeof(int), rank, f);
        fread(&size, sizeof(int), 1, f);

        /* Verify shape matches */
        if (rank != tensor->rank || size != tensor->size) {
            fprintf(stderr, "Error: Tensor %d shape mismatch\n", i);
            free(params);
            fclose(f);
            return -1;
        }

        for (int j = 0; j < rank; j++) {
            if (shape[j] != tensor->shape[j]) {
                fprintf(stderr, "Error: Tensor %d dimension %d mismatch\n", i, j);
                free(params);
                fclose(f);
                return -1;
            }
        }

        /* Load tensor data */
        fread(tensor->data, sizeof(double), tensor->size, f);
    }

    fclose(f);
    free(params);

    printf("✅ Model loaded from %s\n", filepath);
    printf("   Architecture: vocab=%d, d_model=%d, heads=%d, layers=%d\n",
           model->vocab_size, model->d_model, model->n_heads, model->n_layers);
    printf("   Parameters: %d tensors loaded\n", n_params_model);

    return 0;
}

/* Save training checkpoint with optimizer state */
int checkpoint_save(TransformerV2 *model, AdamOptimizerV2 *optimizer,
                   int iteration, double loss, const char *filepath) {
    char checkpoint_path[256];
    snprintf(checkpoint_path, sizeof(checkpoint_path),
             "%s.iter_%06d.ckpt", filepath, iteration);

    FILE *f = fopen(checkpoint_path, "wb");
    if (!f) {
        fprintf(stderr, "Error: Cannot create checkpoint %s\n", checkpoint_path);
        return -1;
    }

    /* Write checkpoint header */
    uint32_t magic = MODEL_MAGIC;
    uint32_t version = MODEL_VERSION;
    fwrite(&magic, sizeof(uint32_t), 1, f);
    fwrite(&version, sizeof(uint32_t), 1, f);

    /* Write training state */
    fwrite(&iteration, sizeof(int), 1, f);
    fwrite(&loss, sizeof(double), 1, f);
    fwrite(&optimizer->learning_rate, sizeof(double), 1, f);

    /* Write model architecture */
    fwrite(&model->vocab_size, sizeof(int), 1, f);
    fwrite(&model->d_model, sizeof(int), 1, f);
    fwrite(&model->n_heads, sizeof(int), 1, f);
    fwrite(&model->n_layers, sizeof(int), 1, f);
    fwrite(&model->d_ff, sizeof(int), 1, f);
    fwrite(&model->max_seq_len, sizeof(int), 1, f);

    /* Get parameters */
    VariableV2 **params;
    int n_params;
    transformer_get_params(model, &params, &n_params);
    fwrite(&n_params, sizeof(int), 1, f);

    /* Write parameters and optimizer states */
    for (int i = 0; i < n_params; i++) {
        TensorV2 *tensor = params[i]->data;

        /* Write parameter */
        fwrite(&tensor->rank, sizeof(int), 1, f);
        fwrite(tensor->shape, sizeof(int), tensor->rank, f);
        fwrite(&tensor->size, sizeof(int), 1, f);
        fwrite(tensor->data, sizeof(double), tensor->size, f);

        /* Write optimizer state (m and v for Adam) */
        /* Note: This assumes optimizer stores states - would need to extend optimizer */
        /* For now, just write zeros as placeholders */
        double *zeros = calloc(tensor->size, sizeof(double));
        fwrite(zeros, sizeof(double), tensor->size, f);  // m
        fwrite(zeros, sizeof(double), tensor->size, f);  // v
        free(zeros);
    }

    fclose(f);
    free(params);

    printf("💾 Checkpoint saved: %s (iter=%d, loss=%.4f)\n",
           checkpoint_path, iteration, loss);

    return 0;
}

/* Load training checkpoint */
int checkpoint_load(TransformerV2 **model, AdamOptimizerV2 **optimizer,
                   int *iteration, double *loss, const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open checkpoint %s\n", filepath);
        return -1;
    }

    /* Check header */
    uint32_t magic, version;
    fread(&magic, sizeof(uint32_t), 1, f);
    fread(&version, sizeof(uint32_t), 1, f);

    if (magic != MODEL_MAGIC || version != MODEL_VERSION) {
        fprintf(stderr, "Error: Invalid checkpoint file\n");
        fclose(f);
        return -1;
    }

    /* Read training state */
    double lr;
    fread(iteration, sizeof(int), 1, f);
    fread(loss, sizeof(double), 1, f);
    fread(&lr, sizeof(double), 1, f);

    /* Read architecture */
    int vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len;
    fread(&vocab_size, sizeof(int), 1, f);
    fread(&d_model, sizeof(int), 1, f);
    fread(&n_heads, sizeof(int), 1, f);
    fread(&n_layers, sizeof(int), 1, f);
    fread(&d_ff, sizeof(int), 1, f);
    fread(&max_seq_len, sizeof(int), 1, f);

    /* Create model */
    *model = transformer_create(vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len);

    /* Create optimizer */
    *optimizer = adam_create(lr);

    /* Read parameters */
    int n_params;
    fread(&n_params, sizeof(int), 1, f);

    VariableV2 **params;
    int n_params_model;
    transformer_get_params(*model, &params, &n_params_model);

    for (int i = 0; i < n_params; i++) {
        /* Read parameter */
        int rank, size;
        int shape[8];
        fread(&rank, sizeof(int), 1, f);
        fread(shape, sizeof(int), rank, f);
        fread(&size, sizeof(int), 1, f);
        fread(params[i]->data->data, sizeof(double), size, f);

        /* Read optimizer states (skip for now) */
        double *temp = malloc(size * sizeof(double));
        fread(temp, sizeof(double), size, f);  // m
        fread(temp, sizeof(double), size, f);  // v
        free(temp);

        /* Add to optimizer */
        adam_add_param(*optimizer, params[i]);
    }

    fclose(f);
    free(params);

    printf("✅ Checkpoint loaded: iter=%d, loss=%.4f\n", *iteration, *loss);

    return 0;
}