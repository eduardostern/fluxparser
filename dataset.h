/*
 * dataset.h - Dataset loading and tokenization
 */

#ifndef DATASET_H
#define DATASET_H

/* Character-level tokenizer */
typedef struct {
    int vocab_size;
    char *char_to_id;  /* Maps token index to character */
} CharTokenizer;

/* Dataset */
typedef struct {
    int *tokens;
    int length;
} Dataset;

/* Tokenizer functions */
int char_to_token(char c, CharTokenizer *tokenizer);
char token_to_char(int token, CharTokenizer *tokenizer);
CharTokenizer* create_char_tokenizer(const char *text);
void free_tokenizer(CharTokenizer *tokenizer);

/* Dataset loading */
Dataset* load_text_file(const char *filepath, CharTokenizer **tokenizer_out);
Dataset* load_shakespeare(CharTokenizer **tokenizer_out);
void free_dataset(Dataset *dataset);

/* Batch creation */
void get_batch(Dataset *dataset, int batch_size, int seq_len,
               int *batch_inputs, int *batch_targets);

/* Tokenizer persistence */
int save_tokenizer(CharTokenizer *tokenizer, const char *filepath);
CharTokenizer* load_tokenizer(const char *filepath);

/* ============================================================================
 * WORD-LEVEL TOKENIZER (like GPT)
 * ============================================================================ */

/* Word-level tokenizer - much better than character-level for LLMs */
typedef struct {
    char **words;        /* Array of words (vocabulary) */
    int vocab_size;      /* Number of unique words */
    int max_vocab_size;  /* Maximum vocabulary size */
} WordTokenizer;

/* Word tokenizer functions */
WordTokenizer* create_word_tokenizer(const char *text, int max_vocab);
void free_word_tokenizer(WordTokenizer *tokenizer);
int word_to_token(const char *word, WordTokenizer *tokenizer);
const char* token_to_word(int token, WordTokenizer *tokenizer);

/* Word-level dataset loading */
Dataset* load_shakespeare_words(WordTokenizer **tokenizer_out);
Dataset* tokenize_text_words(const char *text, WordTokenizer *tokenizer);

/* Word tokenizer persistence */
int save_word_tokenizer(WordTokenizer *tokenizer, const char *filepath);
WordTokenizer* load_word_tokenizer(const char *filepath);

#endif /* DATASET_H */