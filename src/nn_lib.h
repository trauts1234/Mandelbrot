#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <functional>
#include <iterator>
#include <map>
#include <iomanip>
#include "nn_lib_helpers.h"
#include "Util.h"

namespace NeuralNetwork{

template<int output_vector_size>
class EfficientlyUpdatableLayer{
public:

EfficientlyUpdatableLayer(){}

EfficientlyUpdatableLayer(const std::tuple<std::vector<std::vector<float>>, std::vector<float>> weights_biases){
    const auto[weights_vec, biases_vec] = weights_biases;
    assert(biases_vec.size() == output_vector_size);
    assert(weights_vec.size() == output_vector_size);
    assert(weights_vec[0].size() == NN_Helpers::EfficientUpdateInputSize);

    std::copy(biases_vec.begin(), biases_vec.end(), bias);
    for(int output_idx=0;output_idx<output_vector_size;output_idx++){
        for(int input_idx=0; input_idx<NN_Helpers::EfficientUpdateInputSize; input_idx++){
            matrix[output_vector_size*input_idx + output_idx] = weights_vec[output_idx][input_idx];//flip round for better cache in efficient update
        }
    }
}



/**
 * @brief sets the inputs all to 0, suggesting an empty chessboard
 */
void ResetInputs(){
    for(int output_idx=0; output_idx<output_vector_size; output_idx++) {
        output_no_activation[output_idx] = bias[output_idx];//all variables start as the bias, then matrix is added later
    }
}

inline void AddPiece(int input_idx){
    for(int output_idx=0; output_idx<output_vector_size; output_idx++) {
        output_no_activation[output_idx] += matrix[output_vector_size*input_idx + output_idx];
    }
}
inline void RemovePiece(int input_idx){
    for(int output_idx=0; output_idx<output_vector_size; output_idx++) {
        output_no_activation[output_idx] -= matrix[output_vector_size*input_idx + output_idx];
    }
}


/**
 * @returns the matrix multiplication plus bias, without any output activation function applied
 */
float* GetOutput(){return output_no_activation;}

float output_no_activation[output_vector_size] = {0};//current output of the matrix multiplication
float matrix[NN_Helpers::EfficientUpdateInputSize * output_vector_size] = {0};//indexed top to bottom, then L -> R (backwards for better cache in efficient update)
float bias[output_vector_size] = {0};

};

template<int input_size, int output_size>
class FullLayer {
public:

FullLayer(){}


FullLayer(const std::tuple<std::vector<std::vector<float>>, std::vector<float>> weights_biases){
    const auto[weights_vec, biases_vec] = weights_biases;
    assert(biases_vec.size() == output_size);
    assert(weights_vec.size() == output_size);//other way round from efficientlyUpdatable 
    assert(weights_vec[0].size() == input_size);
    
    for(int output_idx=0;output_idx<output_size;output_idx++){
        bias[output_idx] = biases_vec[output_idx];
        for(int input_idx=0; input_idx<input_size; input_idx++){
            matrix[input_size * output_idx + input_idx] = weights_vec[output_idx][input_idx];//flip round for better cache in efficient update
        }
    }
}

float* ForwardPass(float* previous_layer){
    for(int output_idx=0; output_idx<output_size; output_idx++){
        float weighted_sum = bias[output_idx];

        for(int input_idx=0; input_idx < input_size; input_idx++){
            weighted_sum += NN_Helpers::ReLU(previous_layer[input_idx]) * matrix[input_size*output_idx + input_idx];
        }

        output_no_activation[output_idx] = weighted_sum;
    }

    return output_no_activation;
}

float output_no_activation[output_size] = {0};//current output of the matrix multiplication without the activation function applied
float matrix[output_size * input_size] = {0};//WARNING: indexed opposite way round from EfficientlyUpdatableLayer
float bias[output_size] = {0};

};

template<int hidden_layer_size_a, int hidden_layer_size_b>
class NeuralNetwork{
public:

NeuralNetwork(std::string nn_file_data){
    std::istringstream nn_data_stream(nn_file_data);
    std::string line;

    // Read layer count
    getline(nn_data_stream, line);
    std::istringstream layercount_data(line);
    std::string label;
    layercount_data >> label;
    assert(label == "LayerCount:");
    int layer_count;
    layercount_data >> layer_count;

    // Read layer sizes
    getline(nn_data_stream, line);
    std::istringstream layersize_data(line);
    layersize_data >> label;
    assert(label == "LayerSizes:");
    std::vector<int> layer_sizes;
    int size;
    while (layersize_data >> size) {
        layer_sizes.push_back(size);
    }
    assert(layer_sizes == std::vector<int>({64*12, hidden_layer_size_a, hidden_layer_size_b, 1}));

    // Read activation functions
    getline(nn_data_stream, line);
    std::istringstream activation_data(line);
    activation_data >> label;
    assert(label == "Activations:");
    std::string activation;
    while (activation_data >> activation) {}//do nothing. assume the activations are relu, relu, ... (tanh or sigmoid)

    // Build layers
    std::vector<std::tuple<
    std::vector<std::vector<float>>, std::vector<float>//weight matrix, biases
    >> per_layer_weights_biases;
    for (size_t i = 0; i < layer_sizes.size() - 1; ++i) {
        int inputs_size = layer_sizes[i];

        // Read weights
        getline(nn_data_stream, line);
        std::istringstream weights_data(line);
        weights_data >> label;
        assert(label == "Weights:");
        std::vector<std::string> weights_str;
        std::string weight;
        while (weights_data >> weight) {
            weights_str.push_back(weight);
        }

        // Read biases
        getline(nn_data_stream, line);
        std::istringstream biases_data(line);
        biases_data >> label;
        assert(label == "Biases:");
        std::vector<std::string> biases_str;
        std::string bias;
        while (biases_data >> bias) {
            biases_str.push_back(bias);
        }

        std::vector<std::vector<std::string>> weights_per_neuron = NN_Helpers::chunks(weights_str, inputs_size);
        std::vector<std::vector<float>> weights;
        std::vector<float> biases;
        for (const auto& neuron_weights : weights_per_neuron) {
            std::vector<float> weights_float;
            for (const std::string& w : neuron_weights) {
                weights_float.push_back(std::stof(w));
            }
            weights.push_back(weights_float);
        }
        for (const std::string& b : biases_str) {
            biases.push_back(std::stof(b));
        }

        per_layer_weights_biases.push_back(std::make_tuple(weights, biases));
    }

    first_layer = EfficientlyUpdatableLayer<hidden_layer_size_a>(per_layer_weights_biases[0]);
    hidden_layer = FullLayer<hidden_layer_size_a,hidden_layer_size_b>(per_layer_weights_biases[1]);
    final_layer = FullLayer<hidden_layer_size_b,1>(per_layer_weights_biases[2]);
}

float FastForwardPass(){
    float* hidden_a_activations = first_layer.GetOutput();//efficiently updated, no work to do here
    float* hidden_b_activations = hidden_layer.ForwardPass(hidden_a_activations);
    float* output_layer_activations = final_layer.ForwardPass(hidden_b_activations);
    return output_layer_activations[0] * 200;//200 is the magic number in python training code
}

void ResetEfficientUpdate(){first_layer.ResetInputs();}

void AddPiece(Square sq, Piece p){
    first_layer.AddPiece(NN_Helpers::CalculateEfficientUpdateIndex(sq, p));
}
void RemovePiece(Square sq, Piece p){
    first_layer.RemovePiece(NN_Helpers::CalculateEfficientUpdateIndex(sq, p));
}

bool AmEqualToOtherNet(const NeuralNetwork<hidden_layer_size_a, hidden_layer_size_b>& other) const {
    if(!NN_Helpers::CompareCArrays(other.first_layer.bias, first_layer.bias, hidden_layer_size_a)){
        return false;//wrong bias in input layer
    }
    if(!NN_Helpers::CompareCArrays(other.hidden_layer.bias, hidden_layer.bias, hidden_layer_size_b)){
        return false;//wrong bias in hidden layer
    }
    if(!NN_Helpers::FloatsEqual(other.final_layer.bias[0], final_layer.bias[0])){
        return false;//wrong bias in output layer
    }

    if(!NN_Helpers::CompareCArrays(other.first_layer.output_no_activation, first_layer.output_no_activation, hidden_layer_size_a)){
        return false;//wrong bias in input layer
    }
    if(!NN_Helpers::CompareCArrays(other.hidden_layer.output_no_activation, hidden_layer.output_no_activation, hidden_layer_size_b)){
        return false;//wrong bias in hidden layer
    }
    if(!NN_Helpers::FloatsEqual(other.final_layer.output_no_activation[0], final_layer.output_no_activation[0])){
        return false;//wrong bias in output layer
    }

    //todo FIX these tests since i moved to 1d array

    /*for(int efficient_output_idx=0;efficient_output_idx<hidden_layer_size_a;efficient_output_idx++){
        //loop outputs from efficient update layer
        for(int input_idx=0;input_idx<NN_Helpers::EfficientUpdateInputSize;input_idx++){
            //loop inputs to efficient update layer
            if(!NN_Helpers::FloatsEqual(
                other.first_layer.matrix[input_idx][efficient_output_idx],
                first_layer.matrix[input_idx][efficient_output_idx])){
                return false;//matrix used is different
            }
        }
    }

    for(int hidden_output_idx=0;hidden_output_idx<hidden_layer_size_b;hidden_output_idx++){
        //loop outputs from hidden
        for(int input_idx=0;input_idx<hidden_layer_size_a;input_idx++){
            //loop inputs to hidden
            if(!NN_Helpers::FloatsEqual(
                other.hidden_layer.matrix[hidden_output_idx][input_idx],
                hidden_layer.matrix[hidden_output_idx][input_idx])){
                return false;//matrix used is different
            }
        }
    }*/

    //net only final outputs 1 value, so no 2d loops here
    for(int input_idx=0;input_idx<hidden_layer_size_b;input_idx++){
        if(!NN_Helpers::FloatsEqual(other.final_layer.matrix[input_idx], final_layer.matrix[input_idx])){
            return false;//matrix used is different
        }
    }

    return true;
}


private:
//each layer refers to the nodes and outgoing connections from that layer, so there are are actually n+1 layers, as the output layer is 1 node with no connections
EfficientlyUpdatableLayer<hidden_layer_size_a> first_layer;
FullLayer<hidden_layer_size_a,hidden_layer_size_b> hidden_layer;
FullLayer<hidden_layer_size_b,1> final_layer;
};

}