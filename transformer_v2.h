/*
 * transformer_v2.h - GPT-style transformer using autograd_v2
 * Memory-safe implementation with arena allocation
 */
#ifndef TRANSFORMER_V2_H
#define TRANSFORMER_V2_H

#include "autograd_v2.h"

/* Layer Normalization */
typedef struct {
    VariableV2 *gamma;  /* scale parameter */
    VariableV2 *beta;   /* shift parameter */
    int size;
    double eps;
} LayerNormV2;

LayerNormV2* layer_norm_create(int size);
void layer_norm_free(LayerNormV2 *ln);
VariableV2* layer_norm_forward(LayerNormV2 *ln, VariableV2 *input);

/* Multi-Head Attention */
typedef struct {
    Linear *q_proj;  /* query projection */
    Linear *k_proj;  /* key projection */
    Linear *v_proj;  /* value projection */
    Linear *out_proj; /* output projection */
    int n_heads;
    int d_model;
    int d_head;
    double scale;
} MultiHeadAttention;

MultiHeadAttention* mha_create(int d_model, int n_heads);
void mha_free(MultiHeadAttention *mha);
VariableV2* mha_forward(MultiHeadAttention *mha, VariableV2 *x);

/* Feed-Forward Network */
typedef struct {
    Linear *fc1;
    Linear *fc2;
    int d_model;
    int d_ff;
} FeedForward;

FeedForward* ff_create(int d_model, int d_ff);
void ff_free(FeedForward *ff);
VariableV2* ff_forward(FeedForward *ff, VariableV2 *x);

/* Transformer Block */
typedef struct {
    MultiHeadAttention *attn;
    LayerNormV2 *ln1;
    LayerNormV2 *ln2;
    FeedForward *ff;
} TransformerBlock;

TransformerBlock* block_create(int d_model, int n_heads, int d_ff);
void block_free(TransformerBlock *block);
VariableV2* block_forward(TransformerBlock *block, VariableV2 *x);

/* Full Transformer Model */
typedef struct {
    /* Token and position embeddings */
    VariableV2 *token_embed;  /* vocab_size x d_model */
    VariableV2 *pos_embed;    /* max_seq_len x d_model */

    /* Transformer blocks */
    TransformerBlock **blocks;
    int n_layers;

    /* Output head */
    LayerNormV2 *ln_final;
    Linear *lm_head;

    /* Model dimensions */
    int vocab_size;
    int d_model;
    int n_heads;
    int d_ff;
    int max_seq_len;
} TransformerV2;

/* Model creation and management */
TransformerV2* transformer_create(
    int vocab_size,
    int d_model,
    int n_heads,
    int n_layers,
    int d_ff,
    int max_seq_len
);
void transformer_free(TransformerV2 *model);

/* Forward pass - returns logits */
VariableV2* transformer_forward(TransformerV2 *model, int *tokens, int seq_len);

/* Training utilities */
VariableV2* compute_cross_entropy_loss(VariableV2 *logits, int *targets, int seq_len);

/* Optimizer */
typedef struct {
    VariableV2 **params;
    int n_params;
    double learning_rate;
} AdamOptimizerV2;

AdamOptimizerV2* adam_create(double learning_rate);
void adam_add_param(AdamOptimizerV2 *opt, VariableV2 *param);
void adam_step(AdamOptimizerV2 *opt);
void adam_free(AdamOptimizerV2 *opt);

/* Helper to collect all model parameters */
void transformer_get_params(TransformerV2 *model, VariableV2 ***params, int *n_params);

#endif /* TRANSFORMER_V2_H */