#include <string>
#include <vector>

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/layer.hpp"
#include "caffe/sequence_layers.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void BP_RNNLayer<Dtype>::BP_RecurrentInputBlobNames(vector<string>* names) const {
  names->resize(1);
  (*names)[0] = "MessageIn0";
}

template <typename Dtype>
void BP_RNNLayer<Dtype>::BP_RecurrentOutputBlobNames(vector<string>* names) const {
  names->resize(1);
  (*names)[0] = "MessageIn" + this->int_to_str(this->T_);
}

template <typename Dtype>
void BP_RNNLayer<Dtype>::BP_OutputBlobNames(vector<string>* names) const {
  names->resize(2);
  (*names)[0] = "o_action";
  (*names)[1] = "o_scene";
}

template <typename Dtype>
void BP_RNNLayer<Dtype>::BP_FillUnrolledNet(NetParameter* net_param) const {
  const int num_output = this->layer_param_.bp_recurrent_param().num_output();
  CHECK_GT(num_output, 0) << "num_output must be positive";
  const FillerParameter& weight_filler =
      this->layer_param_.bp_recurrent_param().weight_filler();
  const FillerParameter& bias_filler =
      this->layer_param_.bp_recurrent_param().bias_filler();

  // Add generic LayerParameter's (without bottoms/tops) of layer types we'll
  // use to save redundant code.

  //int num_people = this->layer_param_.bp_recurrent_param().num_people();
  int scene_class = this->layer_param_.bp_recurrent_param().scene_class();
  int action_class = this->layer_param_.bp_recurrent_param().action_class();
  
  
/* temporarily not use,  these flags are for controlling connections between nodes
  int spp = this->layer_param_.bp_recurrent_param().spp();
  int tpp = this->layer_param_.bp_recurrent_param().tpp();
  int ps = this->layer_param_.bp_recurrent_param().ps();

  int temp_node_connection_start, temp_node_connection_mid, spat_node_connection, scene_node_connection;
  if (spp == 1){
      spat_node_connection = num_people + 1 + 1;  //spatial_connection = number of people + 1 temporal + 1 scene
  }
  else{
      spat_node_connection = 1 + 1;  //spatial_connection = 1 temporal + 1 scene
  }
  if (tpp == 1){
      temp_node_connection_start = 2;
      temp_node_connection_mid = 3;
  }
  else{
      temp_node_connection_start = 1; // only by itself, unary factor node
      temp_node_connection_mid = 1;
      std::cout << "without temporal connection!" << std::endl;
  }
  if (ps == 1){
      scene_node_connection = num_people + 1; //scene_connection = number of people + 1 scene unary factor node
  }
  else{
      scene_node_connection = 1;  // without person-scene interaction, scene_node_connection = 1 scene unary factor node itself
      std::cout << "without scene-person interaction!" << std::endl;
  }
  */
  // message classifier definition: **************************************************************************  


  int bp_num_output = 0;
  // temporal node message classifier, 1 for leaf nodes, 2 for mid nodes
  bp_num_output = action_class;  //TA->TB
  LayerParameter temporal1;
  temporal1.set_type("InnerProduct");
  temporal1.mutable_inner_product_param()->set_num_output(bp_num_output);
  temporal1.mutable_inner_product_param()->set_bias_term(true);
  temporal1.mutable_inner_product_param()->set_axis(2);     // batchsize*num_related_messages_all*inputsize
  temporal1.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  temporal1.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);

  LayerParameter temporal2;   //TB->TA
  temporal2.set_type("InnerProduct");
  temporal2.mutable_inner_product_param()->set_num_output(bp_num_output);
  temporal2.mutable_inner_product_param()->set_bias_term(true);
  temporal2.mutable_inner_product_param()->set_axis(2);     // batchsize*num_related_messages_all*inputsize
  temporal2.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  temporal2.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);

  // spatial node message classifier, 1 for output toward temporal nodes, 2 for output toward spatial nodes, 3 for output toward scene nodes
  bp_num_output = action_class;      // A->A
  LayerParameter spatial1;
  spatial1.set_type("InnerProduct");
  spatial1.mutable_inner_product_param()->set_num_output(bp_num_output);
  spatial1.mutable_inner_product_param()->set_bias_term(true);
  spatial1.mutable_inner_product_param()->set_axis(2);     // batchsize*num_related_messages_all*inputsize
  spatial1.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  spatial1.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);
  
  LayerParameter spatial2;
  spatial2.set_type("InnerProduct");
  spatial2.mutable_inner_product_param()->set_num_output(bp_num_output);
  spatial2.mutable_inner_product_param()->set_bias_term(true);
  spatial2.mutable_inner_product_param()->set_axis(2);     // batchsize*num_related_messages_all*inputsize
  spatial2.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  spatial2.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);

  bp_num_output = scene_class;      //A->S
  LayerParameter spatial3;
  spatial3.set_type("InnerProduct");
  spatial3.mutable_inner_product_param()->set_num_output(bp_num_output);
  spatial3.mutable_inner_product_param()->set_bias_term(true);
  spatial3.mutable_inner_product_param()->set_axis(2);     // batchsize*num_related_messages_all*inputsize
  spatial3.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  spatial3.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);

  // scene node message classifier
  bp_num_output = action_class;      //S->A
  LayerParameter scene;
  scene.set_type("InnerProduct");
  scene.mutable_inner_product_param()->set_num_output(bp_num_output);
  scene.mutable_inner_product_param()->set_bias_term(true);
  scene.mutable_inner_product_param()->set_axis(2);     // batchsize*num_related_messages_all*inputsize
  scene.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  scene.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);
  
  // scene node output
  bp_num_output = scene_class;      //S->A
  LayerParameter scene_lb;
  scene_lb.set_type("InnerProduct");
  scene_lb.mutable_inner_product_param()->set_num_output(bp_num_output);
  scene_lb.mutable_inner_product_param()->set_bias_term(true);
  scene_lb.mutable_inner_product_param()->set_axis(1);     // batchsize*num_related_messages_all*inputsize
  scene_lb.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  scene_lb.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);

  // spatial node message classifier, 1 for output toward temporal nodes, 2 for output toward spatial nodes, 3 for output toward scene nodes
  bp_num_output = action_class;      // A->A
  LayerParameter spatial1_lb;
  spatial1_lb.set_type("InnerProduct");
  spatial1_lb.mutable_inner_product_param()->set_num_output(bp_num_output);
  spatial1_lb.mutable_inner_product_param()->set_bias_term(true);
  spatial1_lb.mutable_inner_product_param()->set_axis(2);     // batchsize*num_related_messages_all*inputsize
  spatial1_lb.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  spatial1_lb.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);

  // structured gate:
  bp_num_output = 1;      // A->A
  LayerParameter structured_gate;
  structured_gate.set_type("InnerProduct");
  structured_gate.mutable_inner_product_param()->set_num_output(bp_num_output);
  structured_gate.mutable_inner_product_param()->set_bias_term(true);
  structured_gate.mutable_inner_product_param()->set_axis(1);     // batchsize*num_related_messages_all*inputsize
  structured_gate.mutable_inner_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);
  structured_gate.mutable_inner_product_param()->
      mutable_bias_filler()->CopyFrom(bias_filler);

  LayerParameter eliwise_weight;
  eliwise_weight.set_type("EliwiseProduct");
  //eliwise_weight.mutable_eliwise_product_param()->set_num_output(bp_num_output);
  //eliwise_weight.mutable_eliwise_product_param()->set_bias_term(true);
  eliwise_weight.mutable_eliwise_product_param()->
      mutable_weight_filler()->CopyFrom(weight_filler);

  // message classifier definition end. *******************************************************************************

  // prediction output layer
  LayerParameter scene_output_concat_layer;
  scene_output_concat_layer.set_name("o_concat_scene");
  scene_output_concat_layer.set_type("Concat");
  scene_output_concat_layer.add_top("o_scene");
  scene_output_concat_layer.mutable_concat_param()->set_axis(0);
  
  LayerParameter action_output_concat_layer;
  action_output_concat_layer.set_name("o_concat_action");
  action_output_concat_layer.set_type("Concat");
  action_output_concat_layer.add_top("o_action_forcheck");
  action_output_concat_layer.mutable_concat_param()->set_axis(0);

  LayerParameter tanh_param;
  tanh_param.set_type("ReLU");


  LayerParameter sum_param;
  sum_param.set_type("Eltwise");
  sum_param.mutable_eltwise_param()->set_operation(
      EltwiseParameter_EltwiseOp_SUM);
 
  //bool lb = true;
  //int timecontrol = 0;
  //bool context = false;
  //bool separate_initial_weight = true;
  float w_lr_mult = 10,b_lr_mult = 20; 
  float w_decay_mult = 10,b_decay_mult = 0;
  //bool ifgate = false;
  bool ifforprint = true;
  bool ifneuron = false;
  //bool if_ensemble_message = false;
  //bool if_ensemble_previous = false;
  

  // BP recurrent steps start
  // split frame scores and action scores
  {
	  LayerParameter* split_action_frame = net_param->add_layer();
      split_action_frame->set_name("slice_action_frame_0");
	  split_action_frame->set_type("Slice");
      split_action_frame->add_bottom("concat_all");
      split_action_frame->add_top("scene_score_normalized0");
      split_action_frame->add_top("cur_action_score_normalized_reshaped0");
      split_action_frame->mutable_slice_param()->add_slice_point(5);
  }
  

  LayerParameter action_output_python;
  action_output_python.set_name("output_python_concat_action");
  action_output_python.set_type("Python");
  action_output_python.mutable_python_param()->set_module("MyConcat");
  action_output_python.mutable_python_param()->set_layer("MyConcat");
  if (ifforprint){
      action_output_python.add_top("action_pred");
  }
  else{
      action_output_python.add_top("o_action_forcheck");
  }

  LayerParameter scene_output_python;
  scene_output_python.set_name("output_python_concat_scene");
  scene_output_python.set_type("Python");
  scene_output_python.mutable_python_param()->set_module("MyConcat");
  scene_output_python.mutable_python_param()->set_layer("MyConcat");
  if (ifforprint){
      scene_output_python.add_top("scene_pred");
  }else{
      scene_output_python.add_top("o_scene");
  }  
  

  for (int t = 1; t <= this->T_; ++t) {
      string tm1s = this->int_to_str(t - 1);
      string ts = this->int_to_str(t);
      
      // Add layer to re-arrange all messages to each innerproduct component
      {
		  LayerParameter* python_message_in = net_param->add_layer();
          python_message_in->set_name("ArrangeMessageIn" + ts);
		  python_message_in->set_type("Python");
		  python_message_in->mutable_python_param()->set_module("simplified_message_in");
		  python_message_in->mutable_python_param()->set_layer("simplified_message_in");
          if (ifforprint){
		      python_message_in->add_bottom("concat_all" + ts);
          }else{
              python_message_in->add_bottom("MessageIn" + tm1s);
          }
          python_message_in->add_bottom("label_action");
		  python_message_in->add_top("A_A_MessageIn" + ts);
		  python_message_in->add_top("A_S_MessageIn" + ts);
		  python_message_in->add_top("S_A_MessageIn" + ts);
      }

      // Add layer to predict messages toward action node under current frame
      {
          LayerParameter* action_message_pred = net_param->add_layer();
          action_message_pred->CopyFrom(spatial1);
          action_message_pred->set_name("A_A_Message" + ts);
          caffe::ParamSpec* weight = action_message_pred->add_param();
          caffe::ParamSpec* weight_bias = action_message_pred->add_param();
          weight->set_name("W_hh_1");
          weight->set_lr_mult(w_lr_mult);
          weight->set_decay_mult(w_decay_mult);
          weight_bias->set_lr_mult(b_lr_mult);
          weight_bias->set_decay_mult(b_decay_mult);

          action_message_pred->add_bottom("A_A_MessageIn" + ts);
          if (ifneuron){
              action_message_pred->add_top("A_A_MessageOut_neuron_input_" + ts);  
          }else{
              action_message_pred->add_top("A_A_MessageOut" + ts); 
          }        
          action_message_pred->mutable_inner_product_param()->set_axis(1); // batchsize*num_people*num_people --->
      }

      // Add layer to predict messages toward scene node from action
      {
          LayerParameter* scene_message_pred_as = net_param->add_layer();
          scene_message_pred_as->CopyFrom(spatial3);
          scene_message_pred_as->set_name("A_S_Message" + ts); 
          caffe::ParamSpec* weight = scene_message_pred_as->add_param();
          caffe::ParamSpec* weight_bias = scene_message_pred_as->add_param();
          weight->set_name("W_hh_2");
          weight->set_lr_mult(w_lr_mult);
          weight->set_decay_mult(w_decay_mult);
          weight_bias->set_lr_mult(b_lr_mult);
          weight_bias->set_decay_mult(b_decay_mult);
          scene_message_pred_as->add_bottom("A_S_MessageIn" + ts);
          if (ifneuron){
              scene_message_pred_as->add_top("A_S_MessageOut_neuron_input_" + ts);  
          }else{
              scene_message_pred_as->add_top("A_S_MessageOut" + ts);
          }        
          scene_message_pred_as->mutable_inner_product_param()->set_axis(1);  // batchsize*num_people
      }      

      // Add layer to predict messages toward action node from scene
      {
          LayerParameter* scene_message_pred = net_param->add_layer();
          scene_message_pred->CopyFrom(scene);
          scene_message_pred->set_name("S_A_Message" + ts);
          caffe::ParamSpec* weight = scene_message_pred->add_param();
          caffe::ParamSpec* weight_bias = scene_message_pred->add_param();
          weight->set_name("W_hh_3");
          weight->set_lr_mult(w_lr_mult);
          weight->set_decay_mult(w_decay_mult);
          weight_bias->set_lr_mult(b_lr_mult);
          weight_bias->set_decay_mult(b_decay_mult);
          scene_message_pred->add_bottom("S_A_MessageIn" + ts);
          if (ifneuron){
              scene_message_pred->add_top("S_A_MessageOut_neuron_input_" + ts); 
          }else{
              scene_message_pred->add_top("S_A_MessageOut" + ts); 
          }         
          scene_message_pred->mutable_inner_product_param()->set_axis(1);  // batchsize*num_people
      }

      // neuron layers:
      if (ifneuron){
      {
          {
		    LayerParameter* h_neuron_param = net_param->add_layer();
		    h_neuron_param->CopyFrom(tanh_param);
		    h_neuron_param->set_name("h_neuron1_" + ts);
		    h_neuron_param->add_bottom("S_A_MessageOut_neuron_input_" + ts);
		    h_neuron_param->add_top("S_A_MessageOut" + ts);
		  }
          {
		    LayerParameter* h_neuron_param = net_param->add_layer();
		    h_neuron_param->CopyFrom(tanh_param);
		    h_neuron_param->set_name("h_neuron2_" + ts);
		    h_neuron_param->add_bottom("A_S_MessageOut_neuron_input_" + ts);
		    h_neuron_param->add_top("A_S_MessageOut" + ts);
		  }
          {
		    LayerParameter* h_neuron_param = net_param->add_layer();
		    h_neuron_param->CopyFrom(tanh_param);
		    h_neuron_param->set_name("h_neuron3_" + ts);
		    h_neuron_param->add_bottom("A_A_MessageOut_neuron_input_" + ts);
		    h_neuron_param->add_top("A_A_MessageOut" + ts);
		  }
      }
      }
      
      // Add layer to re-arrange messages from each component
      {
          LayerParameter* python_message_out = net_param->add_layer();
          python_message_out->set_name("ArrangeMessageOut" + ts);
          python_message_out->set_type("Python");
          python_message_out->mutable_python_param()->set_module("Message_Out");
          python_message_out->mutable_python_param()->set_layer("Message_Out");
          python_message_out->add_bottom("A_A_MessageOut" + ts);
          python_message_out->add_bottom("A_S_MessageOut" + ts);
          python_message_out->add_bottom("S_A_MessageOut" + ts);
          if (ifforprint){
              python_message_out->add_bottom("concat_all");
          }else{
              python_message_out->add_bottom("UnaryInput");
          }
          python_message_out->add_bottom("label_action" + ts);
          python_message_out->add_top("scene_" + ts);
          python_message_out->add_top("action_" + ts);
      }

      // Add layer to normalize message
      {
          LayerParameter* softmax_layer_normalize1 = net_param->add_layer();
          softmax_layer_normalize1->set_type("Softmax");
          softmax_layer_normalize1->set_name("Normalize_Scene_" + ts);
          softmax_layer_normalize1->add_bottom("scene_" + ts);
          softmax_layer_normalize1->add_top("scene_normalized_" + ts);
      }

      // Add layer to normalize message
      {
          LayerParameter* softmax_layer_normalize2 = net_param->add_layer();
          softmax_layer_normalize2->set_type("Softmax");
          softmax_layer_normalize2->set_name("Normalize_Action_" + ts);
          softmax_layer_normalize2->add_bottom("action_" + ts);
          softmax_layer_normalize2->add_top("action_normalized_" + ts);
      }

      // Add layer to re-arrange action predictions
      {
          LayerParameter* python_action_arrange = net_param->add_layer();
          python_action_arrange->set_name("Data_arrange_layer_filter" + ts);
          python_action_arrange->set_type("Python");
          python_action_arrange->mutable_python_param()->set_module("Data_arrange");
          python_action_arrange->mutable_python_param()->set_layer("Data_Arrange_Layer");
          python_action_arrange->add_bottom("action_normalized_" + ts);
          python_action_arrange->add_bottom("label_action");
          python_action_arrange->add_top("fc9_filtered" + ts);
      }

      {  
         LayerParameter* concat_layer_all = net_param->add_layer();
         concat_layer_all->set_name("concatenation_s_a" + ts);
         concat_layer_all->set_type("Concat");
         concat_layer_all->add_bottom("scene_normalized_" + ts);
         concat_layer_all->add_bottom("fc9_filtered" + ts);
         concat_layer_all->add_top("concat_all" + ts);
         concat_layer_all->mutable_concat_param()->set_concat_dim(1);
      }

      scene_output_python.add_bottom("scene_"+ts); 
      action_output_python.add_bottom("action_" + ts);
  }
}

INSTANTIATE_CLASS(BP_RNNLayer);
REGISTER_LAYER_CLASS(BP_RNN);

}  // namespace caffe
