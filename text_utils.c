/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * TEXT UTILITIES IMPLEMENTATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "text_utils.h"

/* ============================================================================
 * VOCABULARY
 * ============================================================================ */

Vocabulary* vocab_create_from_text(const char *text, size_t text_len) {
    Vocabulary *vocab = malloc(sizeof(Vocabulary));

    /* Count unique characters */
    bool seen[256] = {false};
    int count = 0;
    for (size_t i = 0; i < text_len; i++) {
        unsigned char c = (unsigned char)text[i];
        if (!seen[c]) {
            seen[c] = true;
            count++;
        }
    }

    vocab->vocab_size = count;
    vocab->chars = malloc(sizeof(char*) * count);
    vocab->char_to_idx = malloc(sizeof(int) * 256);

    /* Initialize all mappings to -1 (invalid) */
    for (int i = 0; i < 256; i++) {
        vocab->char_to_idx[i] = -1;
    }

    /* Build character list and mapping */
    int idx = 0;
    for (int c = 0; c < 256; c++) {
        if (seen[c]) {
            vocab->chars[idx] = malloc(2);
            vocab->chars[idx][0] = (char)c;
            vocab->chars[idx][1] = '\0';
            vocab->char_to_idx[c] = idx;
            idx++;
        }
    }

    return vocab;
}

void vocab_free(Vocabulary *vocab) {
    if (!vocab) return;
    for (int i = 0; i < vocab->vocab_size; i++) {
        free(vocab->chars[i]);
    }
    free(vocab->chars);
    free(vocab->char_to_idx);
    free(vocab);
}

int vocab_char_to_idx(const Vocabulary *vocab, char c) {
    return vocab->char_to_idx[(unsigned char)c];
}

char vocab_idx_to_char(const Vocabulary *vocab, int idx) {
    if (idx < 0 || idx >= vocab->vocab_size) return '?';
    return vocab->chars[idx][0];
}

void vocab_print(const Vocabulary *vocab) {
    printf("Vocabulary (%d characters):\n", vocab->vocab_size);
    for (int i = 0; i < vocab->vocab_size && i < 80; i++) {
        char c = vocab->chars[i][0];
        if (isprint(c)) {
            printf("  %3d: '%c'\n", i, c);
        } else if (c == '\n') {
            printf("  %3d: '\\n'\n", i);
        } else if (c == '\t') {
            printf("  %3d: '\\t'\n", i);
        } else {
            printf("  %3d: (0x%02x)\n", i, (unsigned char)c);
        }
    }
    if (vocab->vocab_size > 80) {
        printf("  ... (%d more)\n", vocab->vocab_size - 80);
    }
}

/* ============================================================================
 * TOKENIZATION
 * ============================================================================ */

int* tokenize(const Vocabulary *vocab, const char *text, size_t text_len, size_t *out_len) {
    int *tokens = malloc(sizeof(int) * text_len);

    for (size_t i = 0; i < text_len; i++) {
        tokens[i] = vocab_char_to_idx(vocab, text[i]);
        if (tokens[i] == -1) {
            fprintf(stderr, "Warning: Unknown character '%c' (0x%02x) at position %zu\n",
                    text[i], (unsigned char)text[i], i);
            tokens[i] = 0;  /* Map to first char as fallback */
        }
    }

    *out_len = text_len;
    return tokens;
}

char* detokenize(const Vocabulary *vocab, const int *indices, size_t len) {
    char *text = malloc(len + 1);

    for (size_t i = 0; i < len; i++) {
        text[i] = vocab_idx_to_char(vocab, indices[i]);
    }
    text[len] = '\0';

    return text;
}

/* ============================================================================
 * DATA LOADING
 * ============================================================================ */

TextDataset* dataset_load_file(const char *filename) {
    /* Open file */
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return NULL;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read entire file */
    char *text = malloc(file_size + 1);
    size_t bytes_read = fread(text, 1, file_size, f);
    text[bytes_read] = '\0';
    fclose(f);

    /* Create dataset */
    TextDataset *dataset = malloc(sizeof(TextDataset));
    dataset->text = text;
    dataset->text_len = bytes_read;

    /* Build vocabulary */
    dataset->vocab = vocab_create_from_text(text, bytes_read);

    /* Tokenize */
    dataset->tokens = tokenize(dataset->vocab, text, bytes_read, &dataset->num_tokens);

    return dataset;
}

void dataset_free(TextDataset *dataset) {
    if (!dataset) return;
    free(dataset->text);
    free(dataset->tokens);
    vocab_free(dataset->vocab);
    free(dataset);
}

void dataset_print_stats(const TextDataset *dataset) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                   DATASET STATISTICS                           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    printf("File size:       %zu bytes\n", dataset->text_len);
    printf("Number of tokens: %zu\n", dataset->num_tokens);
    printf("Vocabulary size:  %d\n", dataset->vocab->vocab_size);

    /* Show first 200 characters */
    printf("\nFirst 200 characters:\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    for (size_t i = 0; i < 200 && i < dataset->text_len; i++) {
        putchar(dataset->text[i]);
    }
    if (dataset->text_len > 200) {
        printf("...");
    }
    printf("\n─────────────────────────────────────────────────────────────────\n\n");
}

/* ============================================================================
 * BATCHING
 * ============================================================================ */

Batch* batch_create_random(const TextDataset *dataset, int batch_size, int block_size) {
    Batch *batch = malloc(sizeof(Batch));
    batch->batch_size = batch_size;
    batch->block_size = block_size;

    /* Allocate input and target arrays */
    batch->inputs = malloc(sizeof(int*) * batch_size);
    batch->targets = malloc(sizeof(int*) * batch_size);

    for (int i = 0; i < batch_size; i++) {
        batch->inputs[i] = malloc(sizeof(int) * block_size);
        batch->targets[i] = malloc(sizeof(int) * block_size);

        /* Random starting position */
        int max_start = (int)dataset->num_tokens - block_size - 1;
        if (max_start < 0) max_start = 0;
        int start_idx = rand() % (max_start + 1);

        /* Copy input and target sequences */
        for (int j = 0; j < block_size; j++) {
            batch->inputs[i][j] = dataset->tokens[start_idx + j];
            batch->targets[i][j] = dataset->tokens[start_idx + j + 1];  /* Next token */
        }
    }

    return batch;
}

void batch_free(Batch *batch) {
    if (!batch) return;
    for (int i = 0; i < batch->batch_size; i++) {
        free(batch->inputs[i]);
        free(batch->targets[i]);
    }
    free(batch->inputs);
    free(batch->targets);
    free(batch);
}
