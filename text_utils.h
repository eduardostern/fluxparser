/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * TEXT UTILITIES - Tokenization and Data Loading
 * For character-level and word-level language modeling
 */

#ifndef TEXT_UTILS_H
#define TEXT_UTILS_H

#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * VOCABULARY
 * ============================================================================ */

typedef struct {
    char **chars;           /* Array of characters (as strings) */
    int *char_to_idx;       /* Mapping: character ASCII → index */
    int vocab_size;         /* Number of unique characters */
} Vocabulary;

/* Create vocabulary from text */
Vocabulary* vocab_create_from_text(const char *text, size_t text_len);

/* Free vocabulary */
void vocab_free(Vocabulary *vocab);

/* Convert character to index */
int vocab_char_to_idx(const Vocabulary *vocab, char c);

/* Convert index to character */
char vocab_idx_to_char(const Vocabulary *vocab, int idx);

/* Print vocabulary */
void vocab_print(const Vocabulary *vocab);

/* ============================================================================
 * TOKENIZATION
 * ============================================================================ */

/* Tokenize text into indices */
int* tokenize(const Vocabulary *vocab, const char *text, size_t text_len, size_t *out_len);

/* Detokenize indices back to text */
char* detokenize(const Vocabulary *vocab, const int *indices, size_t len);

/* ============================================================================
 * DATA LOADING
 * ============================================================================ */

typedef struct {
    char *text;             /* Full text content */
    size_t text_len;        /* Length of text */
    int *tokens;            /* Tokenized indices */
    size_t num_tokens;      /* Number of tokens */
    Vocabulary *vocab;      /* Vocabulary */
} TextDataset;

/* Load text file */
TextDataset* dataset_load_file(const char *filename);

/* Free dataset */
void dataset_free(TextDataset *dataset);

/* Print dataset statistics */
void dataset_print_stats(const TextDataset *dataset);

/* ============================================================================
 * BATCHING
 * ============================================================================ */

typedef struct {
    int **inputs;           /* Input sequences [batch_size][block_size] */
    int **targets;          /* Target sequences [batch_size][block_size] */
    int batch_size;
    int block_size;
} Batch;

/* Create random batch from dataset */
Batch* batch_create_random(const TextDataset *dataset, int batch_size, int block_size);

/* Free batch */
void batch_free(Batch *batch);

#endif /* TEXT_UTILS_H */
