#include <string>
#include <vector>

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/layer.hpp"
#include "caffe/sequence_layers.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/io.hpp"

namespace caffe {

template <typename Dtype>
string BP_RecurrentLayer<Dtype>::int_to_str(const int t) const {
  ostringstream num;
  num << t;
  return num.str();
}

template <typename Dtype>
void BP_RecurrentLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  CHECK_GE(bottom[0]->num_axes(), 2)
      << "bottom[0] must have at least 2 axes -- (#timesteps, #streams, ...)";

  T_ = this->layer_param_.bp_recurrent_param().num_iteration(); 
  K_ = this->layer_param_.bp_recurrent_param().num_frame();
  N_ = bottom[0]->shape(0);   //  initialized messages

  LOG(INFO) << "Initializing BP_recurrent layer: assuming input messages need "
            << T_ << " iterations for " << N_ << " batches";

  CHECK_EQ(bottom[0]->shape(0),bottom[1]->shape(0))              
      << "bottom[0] and bottom[1] must have same batchsize";// batchsize of messages and unary input should be the same

  // Create a NetParameter; setup the inputs that aren't unique to particular
  // recurrent architectures.
  NetParameter net_param;
  net_param.set_force_backward(true);

  net_param.add_input("MessageIn0");
  BlobShape input_shape;
  for (int i = 0; i < bottom[0]->num_axes(); ++i) {
    input_shape.add_dim(bottom[0]->shape(i));
  }
  net_param.add_input_shape()->CopyFrom(input_shape);

  input_shape.Clear();
  //net_param.add_input("UnaryInput");
  net_param.add_input("Initial_Messages");
  for (int i = 0; i < bottom[1]->num_axes(); ++i) {
    input_shape.add_dim(bottom[1]->shape(i));
  }
  net_param.add_input_shape()->CopyFrom(input_shape);

  input_shape.Clear();
  net_param.add_input("label_action");
  for (int i = 0; i < bottom[2]->num_axes(); ++i) {
    input_shape.add_dim(bottom[2]->shape(i));
  }
  net_param.add_input_shape()->CopyFrom(input_shape);

  input_shape.Clear();
  net_param.add_input("fc8_context");
  for (int i = 0; i < bottom[3]->num_axes(); ++i) {
    input_shape.add_dim(bottom[3]->shape(i));
  }
  net_param.add_input_shape()->CopyFrom(input_shape);

  input_shape.Clear();
  net_param.add_input("concat_all");
  for (int i = 0; i < bottom[4]->num_axes(); ++i) {
    input_shape.add_dim(bottom[4]->shape(i));
  }
  net_param.add_input_shape()->CopyFrom(input_shape);
  
  this->BP_FillUnrolledNet(&net_param);

  // Prepend this layer's name to the names of each layer in the unrolled net.
  const string& layer_name = this->layer_param_.name();
  if (layer_name.size() > 0) {
    for (int i = 0; i < net_param.layer_size(); ++i) {
      LayerParameter* layer = net_param.mutable_layer(i);
      layer->set_name(layer_name + "_" + layer->name());
    }
  }

  WriteProtoToTextFile(net_param, string("/cs/vml2/zhiweid/CVPR16_NEW/bp-rnn/experiments/print_proto/fragment.txt"));

  // Create the unrolled net.
  unrolled_net_.reset(new Net<Dtype>(net_param));
  unrolled_net_->set_debug_info(
      this->layer_param_.recurrent_param().debug_info());

  // Setup pointers to the inputs.
  MessageIn0_blob_ = CHECK_NOTNULL(unrolled_net_->blob_by_name("MessageIn0").get());
  UnaryInput_blob_ = CHECK_NOTNULL(unrolled_net_->blob_by_name("UnaryInput").get());
  label_blob_ = CHECK_NOTNULL(unrolled_net_->blob_by_name("label_action").get());
  fc8_context_ = CHECK_NOTNULL(unrolled_net_->blob_by_name("fc8_context").get());
  concat_all_ = CHECK_NOTNULL(unrolled_net_->blob_by_name("concat_all").get());
  
  // Setup pointers to paired recurrent inputs/outputs.
  /*vector<string> recur_input_names;
  RecurrentInputBlobNames(&recur_input_names);
  vector<string> recur_output_names;
  RecurrentOutputBlobNames(&recur_output_names);
  const int num_recur_blobs = recur_input_names.size();
  CHECK_EQ(num_recur_blobs, recur_output_names.size());
  recur_input_blobs_.resize(num_recur_blobs);
  recur_output_blobs_.resize(num_recur_blobs);
  for (int i = 0; i < recur_input_names.size(); ++i) {
    recur_input_blobs_[i] =
        CHECK_NOTNULL(unrolled_net_->blob_by_name(recur_input_names[i]).get());
    recur_output_blobs_[i] =
        CHECK_NOTNULL(unrolled_net_->blob_by_name(recur_output_names[i]).get());
  }*/

  // Setup pointers to outputs.
  vector<string> output_names;
  BP_OutputBlobNames(&output_names);
  CHECK_EQ(top.size(), output_names.size())
      << "OutputBlobNames must provide an output blob name for each top.";
  output_blobs_.resize(output_names.size());
  for (int i = 0; i < output_names.size(); ++i) {
    output_blobs_[i] =
        CHECK_NOTNULL(unrolled_net_->blob_by_name(output_names[i]).get());
  }

  CHECK_EQ(3+2,
           unrolled_net_->input_blobs().size());

  // This layer's parameters are any parameters in the layers of the unrolled
  // net. We only want one copy of each parameter, so check that the parameter
  // is "owned" by the layer, rather than shared with another.
  this->blobs_.clear();
  for (int i = 0; i < unrolled_net_->params().size(); ++i) {
    if (unrolled_net_->param_owners()[i] == -1) {
      LOG(INFO) << "Adding parameter " << i << ": "
                << unrolled_net_->param_display_names()[i];
      this->blobs_.push_back(unrolled_net_->params()[i]);
    }
  }
  // Check that param_propagate_down is set for all of the parameters in the
  // unrolled net; set param_propagate_down to true in this layer.
  for (int i = 0; i < unrolled_net_->layers().size(); ++i) {
    for (int j = 0; j < unrolled_net_->layers()[i]->blobs().size(); ++j) {
      CHECK(unrolled_net_->layers()[i]->param_propagate_down(j))
          << "param_propagate_down not set for layer " << i << ", param " << j;
    }
  }
  this->param_propagate_down_.clear();
  this->param_propagate_down_.resize(this->blobs_.size(), true);
  //CHECK(0==1);

  // Set the diffs of recurrent outputs to 0 -- we can't backpropagate across
  // batches.
  /*for (int i = 0; i < recur_output_blobs_.size(); ++i) {
    caffe_set(recur_output_blobs_[i]->count(), Dtype(0),
              recur_output_blobs_[i]->mutable_cpu_diff());
  }*/

}

template <typename Dtype>
void BP_RecurrentLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  CHECK_EQ(top.size(), output_blobs_.size());
  // std::cout << "start" << std::endl;
  for (int i = 0; i < top.size(); ++i) {
    top[i]->ReshapeLike(*output_blobs_[i]);
    output_blobs_[i]->ShareData(*top[i]);
    output_blobs_[i]->ShareDiff(*top[i]);
  }
  // std::cout << "pass" << std::endl;
  MessageIn0_blob_->ShareData(*bottom[0]);
  MessageIn0_blob_->ShareDiff(*bottom[0]);
  // std::cout << "pass" << std::endl;
  UnaryInput_blob_->ShareData(*bottom[1]);
  UnaryInput_blob_->ShareDiff(*bottom[1]);
  // std::cout << "pass" << std::endl;
  label_blob_->ShareData(*bottom[2]);
  fc8_context_->ShareData(*bottom[3]);
  fc8_context_->ShareDiff(*bottom[3]);
  concat_all_->ShareData(*bottom[4]);
  concat_all_->ShareDiff(*bottom[4]);
  // std::cout << "pass" << std::endl;
  //x_input_blob_->ShareData(*bottom[0]);
  //x_input_blob_->ShareDiff(*bottom[0]);
  //cont_input_blob_->ShareData(*bottom[1]);
  /*if (static_input_) {
    x_static_input_blob_->ShareData(*bottom[2]);
    x_static_input_blob_->ShareDiff(*bottom[2]);
  }*/
}

template <typename Dtype>
void BP_RecurrentLayer<Dtype>::Reset() {
  // "Reset" the hidden state of the net by zeroing out all recurrent outputs.
  /*for (int i = 0; i < recur_output_blobs_.size(); ++i) {
    caffe_set(recur_output_blobs_[i]->count(), Dtype(0),
              recur_output_blobs_[i]->mutable_cpu_data());
  }*/
}

template <typename Dtype>
void BP_RecurrentLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  // Hacky fix for test time... reshare all the shared blobs.
  // TODO: somehow make this work non-hackily.
  if (this->phase_ == TEST) {
    unrolled_net_->ShareWeightData();
  }

  /*DCHECK_EQ(recur_input_blobs_.size(), recur_output_blobs_.size());
  for (int i = 0; i < recur_input_blobs_.size(); ++i) {
    const int count = recur_input_blobs_[i]->count();
    DCHECK_EQ(count, recur_output_blobs_[i]->count());
    const Dtype* timestep_T_data = recur_output_blobs_[i]->cpu_data();
    Dtype* timestep_0_data = recur_input_blobs_[i]->mutable_cpu_data();
    caffe_copy(count, timestep_T_data, timestep_0_data);
  }*/

  unrolled_net_->ForwardPrefilled();
}

template <typename Dtype>
void BP_RecurrentLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  CHECK(!propagate_down[2]) << "Cannot backpropagate to sequence indicators.";

  // TODO: skip backpropagation to inputs and parameters inside the unrolled
  // net according to propagate_down[0] and propagate_down[2]. For now just
  // backprop to inputs and parameters unconditionally, as either the inputs or
  // the parameters do need backward (or Net would have set
  // layer_needs_backward_[i] == false for this layer).
  unrolled_net_->Backward();
}

#ifdef CPU_ONLY
STUB_GPU_FORWARD(BP_RecurrentLayer, Forward);
#endif

INSTANTIATE_CLASS(BP_RecurrentLayer);

}  // namespace caffe
