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

/* ============================================================================
 * WORD-LEVEL TOKENIZER IMPLEMENTATION
 * ============================================================================ */

/* Helper: check if character is word boundary */
static int is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '\'' || c == '-';
}

/* Helper: convert to lowercase */
static void to_lowercase(char *word) {
    for (int i = 0; word[i]; i++) {
        if (word[i] >= 'A' && word[i] <= 'Z') {
            word[i] += 32;
        }
    }
}

/* Word frequency struct for sorting */
typedef struct {
    char *word;
    int count;
} WordFreq;

/* Compare function for qsort (descending by count) */
static int compare_freq(const void *a, const void *b) {
    return ((WordFreq*)b)->count - ((WordFreq*)a)->count;
}

/* Create word tokenizer from text */
WordTokenizer* create_word_tokenizer(const char *text, int max_vocab) {
    /* First pass: count all unique words */
    int capacity = 10000;
    WordFreq *freq_table = calloc(capacity, sizeof(WordFreq));
    int n_unique = 0;

    int pos = 0;
    char word_buf[256];

    while (text[pos]) {
        /* Skip non-word characters */
        while (text[pos] && !is_word_char(text[pos])) {
            pos++;
        }

        /* Extract word */
        int len = 0;
        while (text[pos] && is_word_char(text[pos]) && len < 255) {
            word_buf[len++] = text[pos++];
        }
        word_buf[len] = '\0';

        if (len == 0) continue;

        /* Convert to lowercase */
        to_lowercase(word_buf);

        /* Find or add to frequency table */
        int found = 0;
        for (int i = 0; i < n_unique; i++) {
            if (strcmp(freq_table[i].word, word_buf) == 0) {
                freq_table[i].count++;
                found = 1;
                break;
            }
        }

        if (!found) {
            /* Expand if needed */
            if (n_unique >= capacity) {
                capacity *= 2;
                freq_table = realloc(freq_table, capacity * sizeof(WordFreq));
            }
            freq_table[n_unique].word = strdup(word_buf);
            freq_table[n_unique].count = 1;
            n_unique++;
        }
    }

    printf("Found %d unique words in text\n", n_unique);

    /* Sort by frequency (descending) */
    qsort(freq_table, n_unique, sizeof(WordFreq), compare_freq);

    /* Create tokenizer with top max_vocab words */
    WordTokenizer *tok = malloc(sizeof(WordTokenizer));
    tok->max_vocab_size = max_vocab;
    tok->vocab_size = (n_unique < max_vocab) ? n_unique + 1 : max_vocab;  /* +1 for <UNK> */

    tok->words = malloc(tok->vocab_size * sizeof(char*));

    /* Token 0 is <UNK> */
    tok->words[0] = strdup("<UNK>");

    /* Add most frequent words */
    for (int i = 0; i < tok->vocab_size - 1 && i < n_unique; i++) {
        tok->words[i + 1] = strdup(freq_table[i].word);
    }

    printf("Created word tokenizer with %d tokens (max %d)\n",
           tok->vocab_size, max_vocab);

    /* Free frequency table */
    for (int i = 0; i < n_unique; i++) {
        free(freq_table[i].word);
    }
    free(freq_table);

    return tok;
}

/* Free word tokenizer */
void free_word_tokenizer(WordTokenizer *tok) {
    if (tok) {
        for (int i = 0; i < tok->vocab_size; i++) {
            free(tok->words[i]);
        }
        free(tok->words);
        free(tok);
    }
}

/* Convert word to token ID (0 = <UNK>) */
int word_to_token(const char *word, WordTokenizer *tok) {
    /* Make lowercase copy */
    char lower[256];
    strncpy(lower, word, 255);
    lower[255] = '\0';
    to_lowercase(lower);

    /* Linear search (could optimize with hash table) */
    for (int i = 1; i < tok->vocab_size; i++) {
        if (strcmp(tok->words[i], lower) == 0) {
            return i;
        }
    }
    return 0;  /* <UNK> */
}

/* Convert token ID to word */
const char* token_to_word(int token, WordTokenizer *tok) {
    if (token >= 0 && token < tok->vocab_size) {
        return tok->words[token];
    }
    return "<UNK>";
}

/* Load Shakespeare with word tokenization */
Dataset* load_shakespeare_words(WordTokenizer **tokenizer_out) {
    /* Load raw text */
    const char *path = "data/shakespeare.txt";
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *text = malloc(size + 1);
    fread(text, 1, size, f);
    text[size] = '\0';
    fclose(f);

    printf("Loaded %ld bytes from %s\n", size, path);

    /* Create word tokenizer (max 2000 words - balance of coverage and learning) */
    WordTokenizer *tok = create_word_tokenizer(text, 2000);
    *tokenizer_out = tok;

    /* Second pass: tokenize text */
    int *tokens = malloc(size * sizeof(int));  /* Upper bound */
    int n_tokens = 0;

    int pos = 0;
    char word_buf[256];

    while (text[pos]) {
        /* Skip non-word characters */
        while (text[pos] && !is_word_char(text[pos])) {
            pos++;
        }

        /* Extract word */
        int len = 0;
        while (text[pos] && is_word_char(text[pos]) && len < 255) {
            word_buf[len++] = text[pos++];
        }
        word_buf[len] = '\0';

        if (len > 0) {
            tokens[n_tokens++] = word_to_token(word_buf, tok);
        }
    }

    /* Shrink tokens array */
    tokens = realloc(tokens, n_tokens * sizeof(int));

    free(text);

    Dataset *ds = malloc(sizeof(Dataset));
    ds->tokens = tokens;
    ds->length = n_tokens;

    printf("Tokenized into %d word tokens (%.1fx compression vs chars)\n",
           n_tokens, (double)size / n_tokens);

    return ds;
}

/* Save word tokenizer */
int save_word_tokenizer(WordTokenizer *tok, const char *filepath) {
    FILE *f = fopen(filepath, "wb");
    if (!f) return -1;

    /* Write vocab size */
    fwrite(&tok->vocab_size, sizeof(int), 1, f);

    /* Write each word (length + chars) */
    for (int i = 0; i < tok->vocab_size; i++) {
        int len = strlen(tok->words[i]);
        fwrite(&len, sizeof(int), 1, f);
        fwrite(tok->words[i], 1, len, f);
    }

    fclose(f);
    return 0;
}

/* Load word tokenizer */
WordTokenizer* load_word_tokenizer(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    WordTokenizer *tok = malloc(sizeof(WordTokenizer));

    fread(&tok->vocab_size, sizeof(int), 1, f);
    tok->max_vocab_size = tok->vocab_size;
    tok->words = malloc(tok->vocab_size * sizeof(char*));

    for (int i = 0; i < tok->vocab_size; i++) {
        int len;
        fread(&len, sizeof(int), 1, f);
        tok->words[i] = malloc(len + 1);
        fread(tok->words[i], 1, len, f);
        tok->words[i][len] = '\0';
    }

    fclose(f);
    return tok;
}