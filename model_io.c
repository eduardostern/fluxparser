/*
 * FluxParser - Model Serialization
 * Save and load trained transformer models
 */

#include <stdio.h>
#include <stdlib.h>
#include "model_io.h"

/* Save model weights to binary file */
int model_save(const TransformerModel *model, const char *filepath) {
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file for writing: %s\n", filepath);
        return -1;
    }

    /* Write model architecture (for verification when loading) */
    fwrite(&model->vocab_size, sizeof(int), 1, f);
    fwrite(&model->embed_dim, sizeof(int), 1, f);
    fwrite(&model->num_layers, sizeof(int), 1, f);
    fwrite(&model->num_heads, sizeof(int), 1, f);
    fwrite(&model->max_seq_len, sizeof(int), 1, f);

    /* Write number of parameters */
    fwrite(&model->num_params, sizeof(int), 1, f);

    /* Write all parameter tensors */
    for (int i = 0; i < model->num_params; i++) {
        Variable *param = model->parameters[i];
        Tensor *t = param->data;

        /* Write tensor shape */
        fwrite(&t->rank, sizeof(int), 1, f);
        fwrite(t->shape, sizeof(int), t->rank, f);

        /* Write tensor data */
        fwrite(t->data, sizeof(double), t->size, f);
    }

    fclose(f);
    printf("✅ Model saved to: %s\n", filepath);
    return 0;
}

/* Load model weights from binary file */
int model_load(TransformerModel *model, const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file for reading: %s\n", filepath);
        return -1;
    }

    /* Read and verify model architecture */
    int vocab_size, embed_dim, num_layers, num_heads, max_seq_len;
    fread(&vocab_size, sizeof(int), 1, f);
    fread(&embed_dim, sizeof(int), 1, f);
    fread(&num_layers, sizeof(int), 1, f);
    fread(&num_heads, sizeof(int), 1, f);
    fread(&max_seq_len, sizeof(int), 1, f);

    if (vocab_size != model->vocab_size || embed_dim != model->embed_dim ||
        num_layers != model->num_layers || num_heads != model->num_heads ||
        max_seq_len != model->max_seq_len) {
        fprintf(stderr, "Error: Model architecture mismatch!\n");
        fprintf(stderr, "  File: vocab=%d, embed=%d, layers=%d, heads=%d, seq=%d\n",
                vocab_size, embed_dim, num_layers, num_heads, max_seq_len);
        fprintf(stderr, "  Model: vocab=%d, embed=%d, layers=%d, heads=%d, seq=%d\n",
                model->vocab_size, model->embed_dim, model->num_layers,
                model->num_heads, model->max_seq_len);
        fclose(f);
        return -1;
    }

    /* Read number of parameters */
    int num_params;
    fread(&num_params, sizeof(int), 1, f);

    if (num_params != model->num_params) {
        fprintf(stderr, "Error: Parameter count mismatch! File has %d, model has %d\n",
                num_params, model->num_params);
        fclose(f);
        return -1;
    }

    /* Read all parameter tensors */
    for (int i = 0; i < model->num_params; i++) {
        Variable *param = model->parameters[i];
        Tensor *t = param->data;

        /* Read and verify tensor shape */
        int rank;
        int shape[8];
        fread(&rank, sizeof(int), 1, f);
        fread(shape, sizeof(int), rank, f);

        if (rank != t->rank) {
            fprintf(stderr, "Error: Tensor %d rank mismatch!\n", i);
            fclose(f);
            return -1;
        }

        for (int j = 0; j < rank; j++) {
            if (shape[j] != t->shape[j]) {
                fprintf(stderr, "Error: Tensor %d shape mismatch!\n", i);
                fclose(f);
                return -1;
            }
        }

        /* Read tensor data */
        fread(t->data, sizeof(double), t->size, f);
    }

    fclose(f);
    printf("✅ Model loaded from: %s\n", filepath);
    return 0;
}
