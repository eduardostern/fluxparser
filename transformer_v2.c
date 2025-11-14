/*
 * transformer_v2.c - GPT-style transformer using autograd_v2
 * Memory-safe implementation with arena allocation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "transformer_v2.h"

/* ============ Layer Normalization ============ */

LayerNormV2* layer_norm_create(int size) {
    LayerNormV2 *ln = calloc(1, sizeof(LayerNormV2));
    ln->size = size;
    ln->eps = 1e-5;

    /* Create persistent parameters */
    int shape[] = {size};
    ln->gamma = var_create_parameter(tensor_create_persistent(shape, 1));
    ln->beta = var_create_parameter(tensor_create_persistent(shape, 1));

    /* Initialize gamma to 1, beta to 0 */
    for (int i = 0; i < size; i++) {
        ln->gamma->data->data[i] = 1.0;
        ln->beta->data->data[i] = 0.0;
    }

    ln->gamma->requires_grad = 1;
    ln->beta->requires_grad = 1;

    return ln;
}

void layer_norm_free(LayerNormV2 *ln) {
    if (ln) {
        var_free_persistent(ln->gamma);
        var_free_persistent(ln->beta);
        free(ln);
    }
}

VariableV2* layer_norm_forward(LayerNormV2 *ln, VariableV2 *input) {
    /* Expect input shape: [seq_len, d_model] or [batch_size, seq_len, d_model] */
    int batch_dim = (input->data->rank == 3) ? input->data->shape[0] : 1;
    int seq_len = (input->data->rank == 3) ? input->data->shape[1] : input->data->shape[0];
    int d_model = (input->data->rank == 3) ? input->data->shape[2] : input->data->shape[1];

    assert(d_model == ln->size);

    /* Create output with same shape as input */
    TensorV2 *output_tensor = tensor_create_temp(input->data->shape, input->data->rank);
    VariableV2 *output = var_create_temp(output_tensor, input->requires_grad);

    /* Allocate arrays to store all means and variances */
    int n_positions = batch_dim * seq_len;
    double *all_means = (double*)malloc(n_positions * sizeof(double));
    double *all_vars = (double*)malloc(n_positions * sizeof(double));

    int pos_idx = 0;
    for (int b = 0; b < batch_dim; b++) {
        for (int t = 0; t < seq_len; t++) {
            /* Calculate mean and variance for this position */
            double mean = 0.0;
            double var = 0.0;

            int offset = pos_idx * d_model;

            /* Mean */
            for (int i = 0; i < d_model; i++) {
                mean += input->data->data[offset + i];
            }
            mean /= d_model;

            /* Variance */
            for (int i = 0; i < d_model; i++) {
                double diff = input->data->data[offset + i] - mean;
                var += diff * diff;
            }
            var /= d_model;

            /* Store mean and variance */
            all_means[pos_idx] = mean;
            all_vars[pos_idx] = var;

            /* Normalize and apply affine transform */
            double std = sqrt(var + ln->eps);
            for (int i = 0; i < d_model; i++) {
                double normalized = (input->data->data[offset + i] - mean) / std;
                output_tensor->data[offset + i] =
                    ln->gamma->data->data[i] * normalized + ln->beta->data->data[i];
            }

            pos_idx++;
        }
    }

    /* Record operation for backward pass with all means and variances */
    tape_record_layer_norm_v2(output, input, ln->gamma, ln->beta, all_means, all_vars, n_positions);

    /* Free temporary arrays */
    free(all_means);
    free(all_vars);

    return output;
}

/* ============ Multi-Head Attention ============ */

MultiHeadAttention* mha_create(int d_model, int n_heads) {
    assert(d_model % n_heads == 0);

    MultiHeadAttention *mha = calloc(1, sizeof(MultiHeadAttention));
    mha->d_model = d_model;
    mha->n_heads = n_heads;
    mha->d_head = d_model / n_heads;
    mha->scale = 1.0 / sqrt(mha->d_head);

    /* Create projection layers */
    mha->q_proj = linear_create(d_model, d_model);
    mha->k_proj = linear_create(d_model, d_model);
    mha->v_proj = linear_create(d_model, d_model);
    mha->out_proj = linear_create(d_model, d_model);

    return mha;
}

void mha_free(MultiHeadAttention *mha) {
    if (mha) {
        linear_free(mha->q_proj);
        linear_free(mha->k_proj);
        linear_free(mha->v_proj);
        linear_free(mha->out_proj);
        free(mha);
    }
}

VariableV2* mha_forward(MultiHeadAttention *mha, VariableV2 *x) {
    /* x shape: [seq_len, d_model] */
    int seq_len = x->data->shape[0];
    int d_model = x->data->shape[1];

    assert(d_model == mha->d_model);

    /* Project to Q, K, V */
    VariableV2 *q = linear_forward(mha->q_proj, x);  /* [seq_len, d_model] */
    VariableV2 *k = linear_forward(mha->k_proj, x);
    VariableV2 *v = linear_forward(mha->v_proj, x);

    /* Reshape for multi-head attention: [seq_len, n_heads, d_head] */
    int new_shape[] = {seq_len, mha->n_heads, mha->d_head};
    q = var_reshape(q, new_shape, 3);
    k = var_reshape(k, new_shape, 3);
    v = var_reshape(v, new_shape, 3);

    /* Compute attention scores for each head */
    int scores_shape[] = {mha->n_heads, seq_len, seq_len};
    TensorV2 *scores_tensor = tensor_create_temp(scores_shape, 3);
    VariableV2 *scores = var_create_temp(scores_tensor, x->requires_grad);

    for (int h = 0; h < mha->n_heads; h++) {
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < seq_len; j++) {
                double score = 0.0;
                for (int d = 0; d < mha->d_head; d++) {
                    int q_idx = i * mha->n_heads * mha->d_head + h * mha->d_head + d;
                    int k_idx = j * mha->n_heads * mha->d_head + h * mha->d_head + d;
                    score += q->data->data[q_idx] * k->data->data[k_idx];
                }
                scores_tensor->data[h * seq_len * seq_len + i * seq_len + j] =
                    score * mha->scale;
            }
        }
    }

    /* Apply softmax to scores */
    VariableV2 *attn_weights = var_softmax_2d(scores);  /* softmax over last dim */

    /* Apply attention weights to values */
    int out_shape[] = {seq_len, mha->n_heads, mha->d_head};
    TensorV2 *attn_output_tensor = tensor_create_temp(out_shape, 3);
    VariableV2 *attn_output = var_create_temp(attn_output_tensor, x->requires_grad);

    for (int h = 0; h < mha->n_heads; h++) {
        for (int i = 0; i < seq_len; i++) {
            for (int d = 0; d < mha->d_head; d++) {
                double sum = 0.0;
                for (int j = 0; j < seq_len; j++) {
                    int w_idx = h * seq_len * seq_len + i * seq_len + j;
                    int v_idx = j * mha->n_heads * mha->d_head + h * mha->d_head + d;
                    sum += attn_weights->data->data[w_idx] * v->data->data[v_idx];
                }
                int out_idx = i * mha->n_heads * mha->d_head + h * mha->d_head + d;
                attn_output_tensor->data[out_idx] = sum;
            }
        }
    }

    /* Reshape back to [seq_len, d_model] */
    int final_shape[] = {seq_len, d_model};
    attn_output = var_reshape(attn_output, final_shape, 2);

    /* Final output projection */
    VariableV2 *output = linear_forward(mha->out_proj, attn_output);

    return output;
}

/* ============ Feed-Forward Network ============ */

FeedForward* ff_create(int d_model, int d_ff) {
    FeedForward *ff = calloc(1, sizeof(FeedForward));
    ff->d_model = d_model;
    ff->d_ff = d_ff;
    ff->fc1 = linear_create(d_model, d_ff);
    ff->fc2 = linear_create(d_ff, d_model);
    return ff;
}

void ff_free(FeedForward *ff) {
    if (ff) {
        linear_free(ff->fc1);
        linear_free(ff->fc2);
        free(ff);
    }
}

VariableV2* ff_forward(FeedForward *ff, VariableV2 *x) {
    VariableV2 *h = linear_forward(ff->fc1, x);
    h = var_relu(h);  /* ReLU activation */
    VariableV2 *output = linear_forward(ff->fc2, h);
    return output;
}

/* ============ Transformer Block ============ */

TransformerBlock* block_create(int d_model, int n_heads, int d_ff) {
    TransformerBlock *block = calloc(1, sizeof(TransformerBlock));
    block->attn = mha_create(d_model, n_heads);
    block->ln1 = layer_norm_create(d_model);
    block->ln2 = layer_norm_create(d_model);
    block->ff = ff_create(d_model, d_ff);
    return block;
}

void block_free(TransformerBlock *block) {
    if (block) {
        mha_free(block->attn);
        layer_norm_free(block->ln1);
        layer_norm_free(block->ln2);
        ff_free(block->ff);
        free(block);
    }
}

VariableV2* block_forward(TransformerBlock *block, VariableV2 *x) {
    /* Pre-norm architecture */
    VariableV2 *ln1_out = layer_norm_forward(block->ln1, x);
    VariableV2 *attn_out = mha_forward(block->attn, ln1_out);
    VariableV2 *x_attn = var_add(x, attn_out);  /* residual connection */

    VariableV2 *ln2_out = layer_norm_forward(block->ln2, x_attn);
    VariableV2 *ff_out = ff_forward(block->ff, ln2_out);
    VariableV2 *output = var_add(x_attn, ff_out);  /* residual connection */

    return output;
}

/* ============ Full Transformer Model ============ */

TransformerV2* transformer_create(
    int vocab_size,
    int d_model,
    int n_heads,
    int n_layers,
    int d_ff,
    int max_seq_len
) {
    TransformerV2 *model = calloc(1, sizeof(TransformerV2));

    model->vocab_size = vocab_size;
    model->d_model = d_model;
    model->n_heads = n_heads;
    model->n_layers = n_layers;
    model->d_ff = d_ff;
    model->max_seq_len = max_seq_len;

    /* Create embeddings */
    int tok_shape[] = {vocab_size, d_model};
    TensorV2 *tok_tensor = tensor_create_persistent(tok_shape, 2);
    model->token_embed = var_create_parameter(tok_tensor);

    int pos_shape[] = {max_seq_len, d_model};
    TensorV2 *pos_tensor = tensor_create_persistent(pos_shape, 2);
    model->pos_embed = var_create_parameter(pos_tensor);

    /* Initialize embeddings */
    double scale = sqrt(1.0 / d_model);
    for (int i = 0; i < vocab_size * d_model; i++) {
        model->token_embed->data->data[i] = ((double)rand() / RAND_MAX - 0.5) * 2 * scale;
    }
    for (int i = 0; i < max_seq_len * d_model; i++) {
        model->pos_embed->data->data[i] = ((double)rand() / RAND_MAX - 0.5) * 2 * scale;
    }

    /* Create transformer blocks */
    model->blocks = calloc(n_layers, sizeof(TransformerBlock*));
    for (int i = 0; i < n_layers; i++) {
        model->blocks[i] = block_create(d_model, n_heads, d_ff);
    }

    /* Output head */
    model->ln_final = layer_norm_create(d_model);
    model->lm_head = linear_create(d_model, vocab_size);

    return model;
}

void transformer_free(TransformerV2 *model) {
    if (model) {
        var_free_persistent(model->token_embed);
        var_free_persistent(model->pos_embed);

        for (int i = 0; i < model->n_layers; i++) {
            block_free(model->blocks[i]);
        }
        free(model->blocks);

        layer_norm_free(model->ln_final);
        linear_free(model->lm_head);
        free(model);
    }
}

VariableV2* transformer_forward(TransformerV2 *model, int *tokens, int seq_len) {
    assert(seq_len <= model->max_seq_len);

    /* Get token embeddings */
    int emb_shape[] = {seq_len, model->d_model};
    TensorV2 *x_tensor = tensor_create_temp(emb_shape, 2);
    VariableV2 *x = var_create_temp(x_tensor, true);

    for (int t = 0; t < seq_len; t++) {
        int token = tokens[t];
        assert(token >= 0 && token < model->vocab_size);
        for (int d = 0; d < model->d_model; d++) {
            int tok_idx = token * model->d_model + d;
            int pos_idx = t * model->d_model + d;
            x_tensor->data[t * model->d_model + d] =
                model->token_embed->data->data[tok_idx] +
                model->pos_embed->data->data[pos_idx];
        }
    }

    /* Pass through transformer blocks */
    for (int i = 0; i < model->n_layers; i++) {
        x = block_forward(model->blocks[i], x);
    }

    /* Final layer norm */
    x = layer_norm_forward(model->ln_final, x);

    /* Project to vocabulary */
    VariableV2 *logits = linear_forward(model->lm_head, x);  /* [seq_len, vocab_size] */

    return logits;
}

/* ============ Training Utilities ============ */

VariableV2* compute_cross_entropy_loss(VariableV2 *logits, int *targets, int seq_len) {
    /* logits shape: [seq_len, vocab_size] */
    assert(logits->data->rank == 2);
    assert(logits->data->shape[0] == seq_len);

    int vocab_size = logits->data->shape[1];
    double total_loss = 0.0;

    for (int t = 0; t < seq_len; t++) {
        /* Compute softmax for this position */
        double max_logit = -INFINITY;
        for (int v = 0; v < vocab_size; v++) {
            double val = logits->data->data[t * vocab_size + v];
            if (val > max_logit) max_logit = val;
        }

        double sum_exp = 0.0;
        for (int v = 0; v < vocab_size; v++) {
            sum_exp += exp(logits->data->data[t * vocab_size + v] - max_logit);
        }

        /* Cross-entropy loss for this position */
        int target = targets[t];
        assert(target >= 0 && target < vocab_size);
        double log_prob = (logits->data->data[t * vocab_size + target] - max_logit) - log(sum_exp);
        total_loss -= log_prob;
    }

    /* Return average loss */
    int loss_shape[] = {1};
    TensorV2 *loss_tensor = tensor_create_temp(loss_shape, 1);
    loss_tensor->data[0] = total_loss / seq_len;
    VariableV2 *loss = var_create_temp(loss_tensor, true);

    /* Record for backward pass */
    tape_record_cross_entropy(loss, logits, targets, seq_len);

    return loss;
}

/* ============ Optimizer ============ */

AdamOptimizerV2* adam_create(double learning_rate) {
    AdamOptimizerV2 *opt = calloc(1, sizeof(AdamOptimizerV2));
    opt->learning_rate = learning_rate;
    opt->params = NULL;
    opt->n_params = 0;
    return opt;
}

void adam_add_param(AdamOptimizerV2 *opt, VariableV2 *param) {
    opt->n_params++;
    opt->params = realloc(opt->params, opt->n_params * sizeof(VariableV2*));
    opt->params[opt->n_params - 1] = param;
}

void adam_step(AdamOptimizerV2 *opt) {
    /* Simple SGD for now - can extend to full Adam later */
    for (int i = 0; i < opt->n_params; i++) {
        VariableV2 *param = opt->params[i];
        if (param->grad) {
            for (int j = 0; j < param->data->size; j++) {
                param->data->data[j] -= opt->learning_rate * param->grad->data[j];
            }
        }
    }
}

void adam_free(AdamOptimizerV2 *opt) {
    if (opt) {
        free(opt->params);
        free(opt);
    }
}

/* Helper to collect all model parameters */
void transformer_get_params(TransformerV2 *model, VariableV2 ***params, int *n_params) {
    /* Count parameters */
    int count = 2;  /* token_embed, pos_embed */
    count += 2;     /* ln_final gamma & beta */
    count += 2;     /* lm_head weight & bias */

    /* Each block has: 2 layer norms (2 params each) + 4 attention linear layers (2 params each) + 2 ff linear layers (2 params each) */
    count += model->n_layers * (2 * 2 + 4 * 2 + 2 * 2);  /* 16 params per block */

    *n_params = count;
    *params = calloc(count, sizeof(VariableV2*));

    int idx = 0;
    (*params)[idx++] = model->token_embed;
    (*params)[idx++] = model->pos_embed;

    /* Add block parameters */
    for (int i = 0; i < model->n_layers; i++) {
        TransformerBlock *block = model->blocks[i];

        /* Layer norm parameters */
        (*params)[idx++] = block->ln1->gamma;
        (*params)[idx++] = block->ln1->beta;
        (*params)[idx++] = block->ln2->gamma;
        (*params)[idx++] = block->ln2->beta;

        /* Attention parameters */
        (*params)[idx++] = block->attn->q_proj->weight;
        (*params)[idx++] = block->attn->q_proj->bias;
        (*params)[idx++] = block->attn->k_proj->weight;
        (*params)[idx++] = block->attn->k_proj->bias;
        (*params)[idx++] = block->attn->v_proj->weight;
        (*params)[idx++] = block->attn->v_proj->bias;
        (*params)[idx++] = block->attn->out_proj->weight;
        (*params)[idx++] = block->attn->out_proj->bias;

        /* Feed-forward parameters */
        (*params)[idx++] = block->ff->fc1->weight;
        (*params)[idx++] = block->ff->fc1->bias;
        (*params)[idx++] = block->ff->fc2->weight;
        (*params)[idx++] = block->ff->fc2->bias;
    }

    /* Final layer parameters */
    (*params)[idx++] = model->ln_final->gamma;
    (*params)[idx++] = model->ln_final->beta;
    (*params)[idx++] = model->lm_head->weight;
    (*params)[idx++] = model->lm_head->bias;

    assert(idx == count);
}