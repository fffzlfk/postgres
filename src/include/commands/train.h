#ifndef TRAIN_H
#define TRAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef void *ModelHandle;

typedef enum ModelType
{
	LINEAR_REG_MODEL_TYPE,
	LSTM_MODEL_TYPE,
	UNKNOWN_MODEL_TYPE,
}			ModelType;

typedef enum OptimizerType
{
	SGD_TYPE,
	ADAM_TYPE,
	RMS_TYPE,
}			OptimizerType;

typedef struct
{
	float		lr;
	OptimizerType opt_type;
	int			batch_size;
	int			input_size;
	int			label_size;
}			HyperParams;

typedef struct
{
}			LinearRegressionParams;

ModelHandle create_model(ModelType modeltype);

ModelHandle create_linear_reg_model(HyperParams * hyper_params, LinearRegressionParams * params);

float		train_model(ModelHandle handle, float *data, float *labels, int batch_size);

void		save_model_bin(ModelHandle handle, char **buf, int *size);

void		destroy_model(ModelHandle handle);

#ifdef __cplusplus
}
#endif
#endif
