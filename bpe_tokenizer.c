/*
 * bpe_tokenizer.c - Byte Pair Encoding tokenizer implementation
 *
 * Algorithm:
 * 1. Initialize vocab with all unique characters + special tokens
 * 2. Count all adjacent token pairs in the corpus
 * 3. Merge most frequent pair into a new token
 * 4. Repeat until reaching max_vocab_size
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "bpe_tokenizer.h"

/* Hash table for pair counting */
#define PAIR_HASH_SIZE 65536

typedef struct PairCount {
    char first[BPE_MAX_TOKEN_LEN];
    char second[BPE_MAX_TOKEN_LEN];
    int count;
    struct PairCount *next;
} PairCount;

/* Simple string hash */
static unsigned int hash_pair(const char *a, const char *b) {
    unsigned int h = 5381;
    for (int i = 0; a[i]; i++) h = ((h << 5) + h) + a[i];
    h = ((h << 5) + h) + 31;  /* Separator */
    for (int i = 0; b[i]; i++) h = ((h << 5) + h) + b[i];
    return h % PAIR_HASH_SIZE;
}

/* Token list for processing */
typedef struct TokenNode {
    char token[BPE_MAX_TOKEN_LEN * 2];
    struct TokenNode *next;
    struct TokenNode *prev;
} TokenNode;

/* Add token to vocabulary */
static int add_to_vocab(BPETokenizer *tok, const char *token) {
    /* Check if already exists */
    for (int i = 0; i < tok->vocab_size; i++) {
        if (strcmp(tok->vocab[i], token) == 0) {
            return i;
        }
    }

    /* Add new token */
    if (tok->vocab_size >= tok->max_vocab_size) {
        return -1;  /* Vocab full */
    }

    tok->vocab[tok->vocab_size] = strdup(token);
    return tok->vocab_size++;
}

/* Create BPE tokenizer */
BPETokenizer* bpe_create(const char *text, int max_vocab_size) {
    BPETokenizer *tok = calloc(1, sizeof(BPETokenizer));
    tok->max_vocab_size = max_vocab_size;
    tok->vocab = calloc(max_vocab_size, sizeof(char*));
    tok->merges = calloc(BPE_MAX_MERGES, sizeof(BPEMerge));
    tok->n_merges = 0;
    tok->vocab_size = 0;

    printf("Building BPE vocabulary (target: %d tokens)...\n", max_vocab_size);

    /* Step 1: Initialize with unique characters */
    int char_seen[256] = {0};
    for (int i = 0; text[i]; i++) {
        unsigned char c = text[i];
        if (!char_seen[c]) {
            char_seen[c] = 1;
            char s[2] = {c, '\0'};
            add_to_vocab(tok, s);
        }
    }
    printf("  Initial vocab: %d characters\n", tok->vocab_size);

    /* Step 2: Convert text to token list */
    int text_len = strlen(text);
    TokenNode *head = NULL;
    TokenNode *tail = NULL;
    int token_count = 0;

    for (int i = 0; i < text_len; i++) {
        TokenNode *node = malloc(sizeof(TokenNode));
        node->token[0] = text[i];
        node->token[1] = '\0';
        node->next = NULL;
        node->prev = tail;

        if (tail) tail->next = node;
        else head = node;
        tail = node;
        token_count++;
    }

    printf("  Initial tokens: %d\n", token_count);

    /* Step 3: Iteratively merge most frequent pairs */
    PairCount **pair_table = calloc(PAIR_HASH_SIZE, sizeof(PairCount*));

    int target_merges = max_vocab_size - tok->vocab_size;
    int merge_count = 0;

    while (tok->vocab_size < max_vocab_size && merge_count < target_merges) {
        /* Clear pair counts */
        for (int i = 0; i < PAIR_HASH_SIZE; i++) {
            PairCount *pc = pair_table[i];
            while (pc) {
                PairCount *next = pc->next;
                free(pc);
                pc = next;
            }
            pair_table[i] = NULL;
        }

        /* Count all pairs */
        TokenNode *node = head;
        while (node && node->next) {
            unsigned int h = hash_pair(node->token, node->next->token);
            PairCount *pc = pair_table[h];

            /* Find or create entry */
            PairCount *found = NULL;
            while (pc) {
                if (strcmp(pc->first, node->token) == 0 &&
                    strcmp(pc->second, node->next->token) == 0) {
                    found = pc;
                    break;
                }
                pc = pc->next;
            }

            if (found) {
                found->count++;
            } else {
                PairCount *new_pc = malloc(sizeof(PairCount));
                strncpy(new_pc->first, node->token, BPE_MAX_TOKEN_LEN - 1);
                strncpy(new_pc->second, node->next->token, BPE_MAX_TOKEN_LEN - 1);
                new_pc->count = 1;
                new_pc->next = pair_table[h];
                pair_table[h] = new_pc;
            }

            node = node->next;
        }

        /* Find most frequent pair */
        char best_first[BPE_MAX_TOKEN_LEN] = "";
        char best_second[BPE_MAX_TOKEN_LEN] = "";
        int best_count = 0;

        for (int i = 0; i < PAIR_HASH_SIZE; i++) {
            PairCount *pc = pair_table[i];
            while (pc) {
                if (pc->count > best_count) {
                    best_count = pc->count;
                    strncpy(best_first, pc->first, BPE_MAX_TOKEN_LEN - 1);
                    strncpy(best_second, pc->second, BPE_MAX_TOKEN_LEN - 1);
                }
                pc = pc->next;
            }
        }

        if (best_count < 2) break;  /* No more useful merges */

        /* Create merged token */
        char merged[BPE_MAX_TOKEN_LEN * 2];
        snprintf(merged, sizeof(merged), "%s%s", best_first, best_second);

        /* Add merge rule */
        strncpy(tok->merges[tok->n_merges].first, best_first, BPE_MAX_TOKEN_LEN - 1);
        strncpy(tok->merges[tok->n_merges].second, best_second, BPE_MAX_TOKEN_LEN - 1);
        strncpy(tok->merges[tok->n_merges].merged, merged, BPE_MAX_TOKEN_LEN * 2 - 1);
        tok->n_merges++;

        /* Add to vocabulary */
        add_to_vocab(tok, merged);

        /* Apply merge to token list */
        node = head;
        int merge_applied = 0;
        while (node && node->next) {
            if (strcmp(node->token, best_first) == 0 &&
                strcmp(node->next->token, best_second) == 0) {
                /* Merge these two nodes */
                strncpy(node->token, merged, BPE_MAX_TOKEN_LEN * 2 - 1);

                TokenNode *to_remove = node->next;
                node->next = to_remove->next;
                if (to_remove->next) {
                    to_remove->next->prev = node;
                } else {
                    tail = node;
                }
                free(to_remove);
                token_count--;
                merge_applied++;
            }
            node = node->next;
        }

        merge_count++;

        /* Progress reporting */
        if (merge_count % 100 == 0) {
            printf("  Merge %d: '%s' + '%s' -> '%s' (count=%d, tokens=%d)\n",
                   merge_count, best_first, best_second, merged, best_count, token_count);
        }
    }

    printf("  Final vocab: %d tokens (%d merges)\n", tok->vocab_size, tok->n_merges);
    printf("  Final token count: %d (%.1fx compression)\n",
           token_count, (float)text_len / token_count);

    /* Cleanup */
    for (int i = 0; i < PAIR_HASH_SIZE; i++) {
        PairCount *pc = pair_table[i];
        while (pc) {
            PairCount *next = pc->next;
            free(pc);
            pc = next;
        }
    }
    free(pair_table);

    /* Free token list */
    TokenNode *node = head;
    while (node) {
        TokenNode *next = node->next;
        free(node);
        node = next;
    }

    return tok;
}

/* Free tokenizer */
void bpe_free(BPETokenizer *tok) {
    if (tok) {
        for (int i = 0; i < tok->vocab_size; i++) {
            free(tok->vocab[i]);
        }
        free(tok->vocab);
        free(tok->merges);
        free(tok);
    }
}

/* Encode text to tokens */
int* bpe_encode(BPETokenizer *tok, const char *text, int *n_tokens) {
    int text_len = strlen(text);
    if (text_len == 0) {
        *n_tokens = 0;
        return NULL;
    }

    /* Start with character tokens */
    char **tokens = malloc(text_len * sizeof(char*));
    int count = 0;

    for (int i = 0; i < text_len; i++) {
        tokens[count] = malloc(BPE_MAX_TOKEN_LEN * 2);
        tokens[count][0] = text[i];
        tokens[count][1] = '\0';
        count++;
    }

    /* Apply merges in order */
    for (int m = 0; m < tok->n_merges; m++) {
        BPEMerge *merge = &tok->merges[m];

        for (int i = 0; i < count - 1; i++) {
            if (strcmp(tokens[i], merge->first) == 0 &&
                strcmp(tokens[i + 1], merge->second) == 0) {
                /* Apply merge */
                strncpy(tokens[i], merge->merged, BPE_MAX_TOKEN_LEN * 2 - 1);

                /* Remove next token */
                free(tokens[i + 1]);
                for (int j = i + 1; j < count - 1; j++) {
                    tokens[j] = tokens[j + 1];
                }
                count--;
                i--;  /* Check this position again */
            }
        }
    }

    /* Convert to token IDs */
    int *ids = malloc(count * sizeof(int));
    for (int i = 0; i < count; i++) {
        ids[i] = bpe_get_id(tok, tokens[i]);
        if (ids[i] < 0) ids[i] = 0;  /* Unknown token */
        free(tokens[i]);
    }
    free(tokens);

    *n_tokens = count;
    return ids;
}

/* Decode tokens to text */
char* bpe_decode(BPETokenizer *tok, const int *tokens, int n_tokens) {
    /* Calculate total length */
    int total_len = 0;
    for (int i = 0; i < n_tokens; i++) {
        if (tokens[i] >= 0 && tokens[i] < tok->vocab_size) {
            total_len += strlen(tok->vocab[tokens[i]]);
        }
    }

    char *result = malloc(total_len + 1);
    result[0] = '\0';

    for (int i = 0; i < n_tokens; i++) {
        if (tokens[i] >= 0 && tokens[i] < tok->vocab_size) {
            strcat(result, tok->vocab[tokens[i]]);
        }
    }

    return result;
}

/* Get token by ID */
const char* bpe_get_token(BPETokenizer *tok, int id) {
    if (id >= 0 && id < tok->vocab_size) {
        return tok->vocab[id];
    }
    return "";
}

/* Get ID by token */
int bpe_get_id(BPETokenizer *tok, const char *token) {
    for (int i = 0; i < tok->vocab_size; i++) {
        if (strcmp(tok->vocab[i], token) == 0) {
            return i;
        }
    }
    return -1;
}

/* Save tokenizer */
int bpe_save(BPETokenizer *tok, const char *filepath) {
    FILE *f = fopen(filepath, "wb");
    if (!f) return -1;

    /* Write vocab size and merge count */
    fwrite(&tok->vocab_size, sizeof(int), 1, f);
    fwrite(&tok->n_merges, sizeof(int), 1, f);

    /* Write vocabulary */
    for (int i = 0; i < tok->vocab_size; i++) {
        int len = strlen(tok->vocab[i]);
        fwrite(&len, sizeof(int), 1, f);
        fwrite(tok->vocab[i], 1, len, f);
    }

    /* Write merges */
    for (int i = 0; i < tok->n_merges; i++) {
        int len1 = strlen(tok->merges[i].first);
        int len2 = strlen(tok->merges[i].second);
        int len3 = strlen(tok->merges[i].merged);

        fwrite(&len1, sizeof(int), 1, f);
        fwrite(tok->merges[i].first, 1, len1, f);
        fwrite(&len2, sizeof(int), 1, f);
        fwrite(tok->merges[i].second, 1, len2, f);
        fwrite(&len3, sizeof(int), 1, f);
        fwrite(tok->merges[i].merged, 1, len3, f);
    }

    fclose(f);
    return 0;
}

/* Load tokenizer */
BPETokenizer* bpe_load(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) return NULL;

    BPETokenizer *tok = calloc(1, sizeof(BPETokenizer));

    /* Read sizes */
    fread(&tok->vocab_size, sizeof(int), 1, f);
    fread(&tok->n_merges, sizeof(int), 1, f);

    tok->max_vocab_size = tok->vocab_size;
    tok->vocab = calloc(tok->vocab_size, sizeof(char*));
    tok->merges = calloc(tok->n_merges + 1, sizeof(BPEMerge));

    /* Read vocabulary */
    for (int i = 0; i < tok->vocab_size; i++) {
        int len;
        fread(&len, sizeof(int), 1, f);
        tok->vocab[i] = malloc(len + 1);
        fread(tok->vocab[i], 1, len, f);
        tok->vocab[i][len] = '\0';
    }

    /* Read merges */
    for (int i = 0; i < tok->n_merges; i++) {
        int len1, len2, len3;

        fread(&len1, sizeof(int), 1, f);
        fread(tok->merges[i].first, 1, len1, f);
        tok->merges[i].first[len1] = '\0';

        fread(&len2, sizeof(int), 1, f);
        fread(tok->merges[i].second, 1, len2, f);
        tok->merges[i].second[len2] = '\0';

        fread(&len3, sizeof(int), 1, f);
        fread(tok->merges[i].merged, 1, len3, f);
        tok->merges[i].merged[len3] = '\0';
    }

    fclose(f);
    return tok;
}
