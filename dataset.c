/*
 * dataset.c - Dataset loading and tokenization for transformer training
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dataset.h"

/* Simple character-level tokenizer */
int char_to_token(char c, CharTokenizer *tokenizer) {
    for (int i = 0; i < tokenizer->vocab_size; i++) {
        if (tokenizer->char_to_id[i] == c) {
            return i;
        }
    }
    return 0;  // Unknown token
}

char token_to_char(int token, CharTokenizer *tokenizer) {
    if (token >= 0 && token < tokenizer->vocab_size) {
        return tokenizer->char_to_id[token];
    }
    return '_';  // Unknown
}

/* Create character-level tokenizer from text */
CharTokenizer* create_char_tokenizer(const char *text) {
    CharTokenizer *tokenizer = malloc(sizeof(CharTokenizer));

    /* Count unique characters */
    int char_seen[256] = {0};
    int unique_chars = 0;

    for (int i = 0; text[i]; i++) {
        unsigned char c = text[i];
        if (!char_seen[c]) {
            char_seen[c] = 1;
            unique_chars++;
        }
    }

    /* Allocate vocab (+1 for unknown token) */
    tokenizer->vocab_size = unique_chars + 1;
    tokenizer->char_to_id = malloc(tokenizer->vocab_size * sizeof(char));

    /* Add unknown token at index 0 */
    tokenizer->char_to_id[0] = '\0';

    /* Add all unique characters */
    int idx = 1;
    for (int i = 0; i < 256; i++) {
        if (char_seen[i]) {
            tokenizer->char_to_id[idx++] = (char)i;
        }
    }

    printf("Created tokenizer with %d unique tokens\n", tokenizer->vocab_size);

    return tokenizer;
}

/* Free tokenizer */
void free_tokenizer(CharTokenizer *tokenizer) {
    if (tokenizer) {
        free(tokenizer->char_to_id);
        free(tokenizer);
    }
}

/* Load text file and create dataset */
Dataset* load_text_file(const char *filepath, CharTokenizer **tokenizer_out) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file %s\n", filepath);
        return NULL;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *text = malloc(file_size + 1);
    size_t read_size = fread(text, 1, file_size, f);
    text[read_size] = '\0';
    fclose(f);

    printf("Loaded %ld bytes from %s\n", read_size, filepath);

    /* Create tokenizer */
    CharTokenizer *tokenizer = create_char_tokenizer(text);
    *tokenizer_out = tokenizer;

    /* Tokenize text */
    Dataset *dataset = malloc(sizeof(Dataset));
    dataset->length = read_size;
    dataset->tokens = malloc(dataset->length * sizeof(int));

    for (int i = 0; i < dataset->length; i++) {
        dataset->tokens[i] = char_to_token(text[i], tokenizer);
    }

    free(text);

    printf("Dataset created: %d tokens\n", dataset->length);

    return dataset;
}

/* Load Shakespeare from URL or local file */
Dataset* load_shakespeare(CharTokenizer **tokenizer_out) {
    const char *local_path = "data/shakespeare.txt";
    const char *url = "https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt";

    /* Check if local file exists */
    FILE *f = fopen(local_path, "r");
    if (f) {
        fclose(f);
        printf("Loading Shakespeare from local file: %s\n", local_path);
        return load_text_file(local_path, tokenizer_out);
    }

    /* Download if not exists */
    printf("Downloading Shakespeare dataset...\n");

    /* Create data directory */
    system("mkdir -p data");

    /* Download using curl or wget */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -s -o %s %s 2>/dev/null || wget -q -O %s %s",
             local_path, url, local_path, url);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Error: Failed to download dataset\n");
        fprintf(stderr, "Please manually download from:\n  %s\n", url);
        fprintf(stderr, "And save to: %s\n", local_path);
        return NULL;
    }

    printf("Downloaded successfully to %s\n", local_path);
    return load_text_file(local_path, tokenizer_out);
}

/* Free dataset */
void free_dataset(Dataset *dataset) {
    if (dataset) {
        free(dataset->tokens);
        free(dataset);
    }
}

/* Create training batches */
void get_batch(Dataset *dataset, int batch_size, int seq_len,
               int *batch_inputs, int *batch_targets) {
    for (int b = 0; b < batch_size; b++) {
        /* Random starting position */
        int start = rand() % (dataset->length - seq_len - 1);

        /* Copy sequence */
        for (int i = 0; i < seq_len; i++) {
            batch_inputs[b * seq_len + i] = dataset->tokens[start + i];
            batch_targets[b * seq_len + i] = dataset->tokens[start + i + 1];
        }
    }
}

/* Save tokenizer to file */
int save_tokenizer(CharTokenizer *tokenizer, const char *filepath) {
    FILE *f = fopen(filepath, "wb");
    if (!f) return -1;

    fwrite(&tokenizer->vocab_size, sizeof(int), 1, f);
    fwrite(tokenizer->char_to_id, sizeof(char), tokenizer->vocab_size, f);

    fclose(f);
    return 0;
}

/* Load tokenizer from file */
CharTokenizer* load_tokenizer(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    CharTokenizer *tokenizer = malloc(sizeof(CharTokenizer));

    fread(&tokenizer->vocab_size, sizeof(int), 1, f);
    tokenizer->char_to_id = malloc(tokenizer->vocab_size * sizeof(char));
    fread(tokenizer->char_to_id, sizeof(char), tokenizer->vocab_size, f);

    fclose(f);
    return tokenizer;
}