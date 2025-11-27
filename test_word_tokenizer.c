/*
 * test_word_tokenizer.c - Test word-level tokenization
 */

#include <stdio.h>
#include <stdlib.h>
#include "dataset.h"

int main() {
    printf("=== Word-Level Tokenizer Test ===\n\n");

    const char *test_text = "To be or not to be, that is the question. "
                           "Whether 'tis nobler in the mind to suffer "
                           "the slings and arrows of outrageous fortune.";

    printf("Input text:\n%s\n\n", test_text);

    /* Create word tokenizer */
    WordTokenizer *tokenizer = create_word_tokenizer(test_text, 100);

    printf("\nVocabulary (top 20 words):\n");
    int display_count = (tokenizer->vocab_size < 20) ? tokenizer->vocab_size : 20;
    for (int i = 0; i < display_count; i++) {
        printf("%3d: %s\n", i, tokenizer->words[i]);
    }

    /* Tokenize a sentence */
    const char *test_sentence = "to be or not to be";
    printf("\nTokenizing: \"%s\"\n", test_sentence);
    printf("Tokens: ");

    WordTokenizer *temp_tok = create_word_tokenizer(test_sentence, 100);
    char word_buf[256];
    int pos = 0;

    while (test_sentence[pos]) {
        /* Skip whitespace/punctuation */
        while (test_sentence[pos] && (test_sentence[pos] == ' ' || test_sentence[pos] == ',' || test_sentence[pos] == '.')) {
            pos++;
        }

        /* Extract word */
        int len = 0;
        while (test_sentence[pos] && test_sentence[pos] != ' ' && test_sentence[pos] != ',' && test_sentence[pos] != '.' && len < 255) {
            word_buf[len++] = test_sentence[pos++];
        }
        word_buf[len] = '\0';

        if (len > 0) {
            /* Convert to lowercase for matching */
            for (int i = 0; i < len; i++) {
                if (word_buf[i] >= 'A' && word_buf[i] <= 'Z') {
                    word_buf[i] += 32;
                }
            }

            int token_id = word_to_token(word_buf, tokenizer);
            printf("%d ", token_id);
        }
    }
    printf("\n");

    /* Detokenize */
    int tokens[] = {1, 2, 3, 4, 1, 2};  /* "to be or not to be" */
    printf("\nDetokenizing tokens [");
    for (size_t i = 0; i < sizeof(tokens)/sizeof(tokens[0]); i++) {
        printf("%d ", tokens[i]);
    }
    printf("]: ");
    for (size_t i = 0; i < sizeof(tokens)/sizeof(tokens[0]); i++) {
        printf("%s ", token_to_word(tokens[i], tokenizer));
    }
    printf("\n");

    /* Test with Shakespeare */
    printf("\n=== Loading Shakespeare Dataset ===\n");
    WordTokenizer *shakespeare_tok;
    Dataset *shakespeare = load_shakespeare_words(&shakespeare_tok);

    if (shakespeare) {
        printf("\nShakespeare Dataset:\n");
        printf("  Total word tokens: %d\n", shakespeare->length);
        printf("  Vocabulary size: %d\n", shakespeare_tok->vocab_size);
        printf("  vs Character-level: ~66 tokens\n");
        printf("  Compression ratio: %.1fx fewer tokens than characters\n",
               1115394.0 / shakespeare->length);

        printf("\nTop 20 most frequent words:\n");
        for (int i = 1; i < 21 && i < shakespeare_tok->vocab_size; i++) {
            printf("%3d: %s\n", i, shakespeare_tok->words[i]);
        }

        free_dataset(shakespeare);
        free_word_tokenizer(shakespeare_tok);
    }

    free_word_tokenizer(tokenizer);
    free_word_tokenizer(temp_tok);

    return 0;
}
