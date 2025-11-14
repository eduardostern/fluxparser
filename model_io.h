/*
 * FluxParser - Model Serialization
 * Save and load trained transformer models
 */

#ifndef MODEL_IO_H
#define MODEL_IO_H

#include "transformer.h"

/* Save model weights to file */
int model_save(const TransformerModel *model, const char *filepath);

/* Load model weights from file */
int model_load(TransformerModel *model, const char *filepath);

#endif /* MODEL_IO_H */
