/*
 * model_io_v2.h - Model serialization for Autograd V2
 */

#ifndef MODEL_IO_V2_H
#define MODEL_IO_V2_H

#include "transformer_v2.h"

/* Save/load model weights */
int transformer_save(TransformerV2 *model, const char *filepath);
int transformer_load(TransformerV2 *model, const char *filepath);

/* Save/load training checkpoints (includes optimizer state) */
int checkpoint_save(TransformerV2 *model, AdamOptimizerV2 *optimizer,
                   int iteration, double loss, const char *filepath);
int checkpoint_load(TransformerV2 **model, AdamOptimizerV2 **optimizer,
                   int *iteration, double *loss, const char *filepath);

#endif /* MODEL_IO_V2_H */