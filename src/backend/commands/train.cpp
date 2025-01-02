#include "commands/train.h"

#include <sstream>
#include <torch/torch.h>
#include <iostream>
#include <memory>

// struct LSTMModel: torch::nn::Module
// {
// 	torch::nn::LSTM lstm{ nullptr };
// 	torch::nn::Linear linear{ nullptr };

// 	LSTMModel(int input_dim, int hidden_dim, int output_dim, int num_layers)
// 											: lstm(torch::nn::LSTMOptions(input_dim, hidden_dim)
// 													   .num_layers(num_layers)
// 													   .batch_first(true)),
// 											  linear(hidden_dim, output_dim)
// 	{
// 		// 注册模块
// 		register_module("lstm", lstm);
// 		register_module("linear", linear);
// 	}

// 	// 前向传播
// 	torch::Tensor
// 	forward(torch::Tensor x)
// 	{
// 		// LSTM 输出
// 		auto lstm_out = lstm->forward(x);
// 		auto hidden_state = std::get<0>(lstm_out); // 获取 LSTM 的输出

// 		// 取最后时间步的输出
// 		auto last_hidden_state = hidden_state.slice(1, -1, hidden_state.size(1));

// 		// 通过全连接层映射到输出
// 		return linear(last_hidden_state.squeeze(1));
// 	}
// };

class IModel
{
  public:
	IModel(int input_size, int label_size, OptimizerType opt_type, float lr)
											: input_size(input_size), label_size(label_size),
											  opt_type(opt_type), lr(lr)
	{
	}
	virtual ~IModel() {}
	virtual torch::Tensor forward(torch::Tensor input) = 0;
	virtual float train_step(torch::Tensor input, torch::Tensor target) = 0;
	virtual void save_model(char **buf, int *size) = 0;
	int
	get_input_size()
	{
		return input_size;
	};
	int
	get_label_size()
	{
		return label_size;
	};

  private:
	int input_size;
	int label_size;

	OptimizerType opt_type;
	float lr;

  protected:
	std::shared_ptr<torch::optim::Optimizer> optimizer;
};

class LinearRegressionModel: public IModel
{
  public:
	LinearRegressionModel(int input_size, int label_size, OptimizerType opt_type, float lr)
											: IModel(input_size, label_size, opt_type, lr),
											  model(torch::nn::Linear(input_size, label_size))
	{
		switch (opt_type)
		{
			case OptimizerType::SGD_TYPE:
				optimizer =
					std::make_unique<torch::optim::SGD>(model->parameters(),
														torch::optim::SGDOptions(lr).momentum(0.9));
				break;
			case OptimizerType::ADAM_TYPE:
				optimizer =
					std::make_unique<torch::optim::Adam>(model->parameters(),
														 torch::optim::AdamOptions(lr).betas(
															 { 0.9, 0.999 }));
				break;
			case OptimizerType::RMS_TYPE:
				optimizer = std::make_unique<torch::optim::RMSprop>(model->parameters(),
																	torch::optim::RMSpropOptions(lr)
																		.momentum(0.9));
				break;
			default:
				std::cerr << "不支持的优化器类型!\n";
				exit(1);
		}
	}

	torch::Tensor
	forward(torch::Tensor x)
	{
		return model->forward(x);
	}

	float
	train_step(torch::Tensor inputs, torch::Tensor targets)
	{
		auto loss_fn = torch::nn::MSELoss();
		optimizer->zero_grad();
		torch::Tensor output = forward(inputs);
		torch::Tensor loss = loss_fn(output, targets);
		loss.backward();
		optimizer->step();

		return loss.item<float>();
	}

	void
	save_model(char **buf, int *size)
	{
		torch::serialize::OutputArchive archive;
		std::ostringstream buffer;

		model->save(archive);
		archive.save_to(buffer);
		*buf = (char *) malloc(sizeof(char) * buffer.tellp());
		*size = buffer.tellp();
		std::memcpy(*buf, buffer.str().c_str(), buffer.tellp());
	}

  private:
	torch::nn::Sequential model;
};

ModelHandle
create_linear_reg_model(HyperParams *hp_params, LinearRegressionParams *lr_params)
{
	return new LinearRegressionModel(hp_params->input_size,
									 hp_params->label_size,
									 hp_params->opt_type,
									 hp_params->lr);
}

float
train_model(ModelHandle handle, float *data, float *labels, int batch_size)
{
	IModel *model = (IModel *) handle;
	int input_size = model->get_input_size();
	int label_size = model->get_label_size();
	torch::Tensor input = torch::from_blob(data,
										   {
											   batch_size,
											   input_size,
										   },
										   torch::kFloat32);
	torch::Tensor target = torch::from_blob(labels, { batch_size, label_size }, torch::kFloat32);

	return model->train_step(input, target);
}

void
save_model_bin(ModelHandle handle, char **buf, int *size)
{
	IModel *model = (IModel *) handle;
	model->save_model(buf, size);
}

void
destroy_model(ModelHandle handle)
{
	IModel *model = (IModel *) handle;
	delete model;
}

// extern "C" void
// train_model(PTensor inputs, PTensor labels)
// {
// 	std::vector<int64_t> inputs_shape(inputs.dim_len);
// 	std::vector<int64_t> labels_shape(labels.dim_len);
// 	// 数据转换为 torch::Tensor
// 	torch::Tensor input_data = torch::from_blob(inputs.data, inputs_shape, torch::kFloat);
// 	torch::Tensor label_data = torch::from_blob(labels.data, labels_shape, torch::kFloat);

// 	int input_size = input_data.size(1);
// 	int output_size = label_data.size(1);

// 	// 初始化模型
// 	auto model = std::make_shared<LinearModel>(input_size, output_size);
// 	torch::optim::SGD optimizer(model->parameters(), torch::optim::SGDOptions(0.01));

// 	// 模型训练
// 	for (size_t epoch = 0; epoch < 100; ++epoch)
// 	{
// 		optimizer.zero_grad();
// 		torch::Tensor output = model->forward(input_data);
// 		torch::Tensor loss = torch::binary_cross_entropy(output, label_data);
// 		loss.backward();
// 		optimizer.step();

// 		std::cout << "Epoch " << epoch << ": Loss = " << loss.item<float>() << std::endl;
// 	}

// 	// 保存模型
// 	torch::save(model, "/tmp/torch_model.pt");
// 	// std::cout << "Model saved to /tmp/torch_model.pt" << std::endl;
// }
