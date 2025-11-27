/*
 * bpe_tokenizer.h - Byte Pair Encoding tokenizer (like GPT)
 *
 * BPE learns subword units from data:
 * - Starts with characters
 * - Iteratively merges most frequent pairs
 * - Results in common words as single tokens, rare words as subwords
 */

#ifndef BPE_TOKENIZER_H
#define BPE_TOKENIZER_H

#define BPE_MAX_TOKEN_LEN 64
#define BPE_MAX_MERGES 10000

/* A single merge rule: "ab" -> "ab" (pair becomes single token) */
typedef struct {
    char first[BPE_MAX_TOKEN_LEN];
    char second[BPE_MAX_TOKEN_LEN];
    char merged[BPE_MAX_TOKEN_LEN * 2];
} BPEMerge;

/* BPE Tokenizer */
typedef struct {
    char **vocab;           /* Token strings, indexed by token ID */
    int vocab_size;         /* Current vocabulary size */
    int max_vocab_size;     /* Maximum vocabulary size */

    BPEMerge *merges;       /* Merge rules in order */
    int n_merges;           /* Number of merge rules */
} BPETokenizer;

/* Create BPE tokenizer from text */
BPETokenizer* bpe_create(const char *text, int max_vocab_size);

/* Free tokenizer */
void bpe_free(BPETokenizer *tok);

/* Encode text to token IDs */
int* bpe_encode(BPETokenizer *tok, const char *text, int *n_tokens);

/* Decode token IDs to text */
char* bpe_decode(BPETokenizer *tok, const int *tokens, int n_tokens);

/* Get token string by ID */
const char* bpe_get_token(BPETokenizer *tok, int id);

/* Get token ID by string (or -1 if not found) */
int bpe_get_id(BPETokenizer *tok, const char *token);

/* Save/load tokenizer */
int bpe_save(BPETokenizer *tok, const char *filepath);
BPETokenizer* bpe_load(const char *filepath);

#endif /* BPE_TOKENIZER_H */
