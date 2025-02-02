#pragma once
#include <vector>
#include <cassert>
#include "Util.h"

namespace NN_Helpers
{
constexpr int EfficientUpdateInputSize = 64*12;//12 times 64 possible squares

/**
 * @note this function is applied to the layer *receiving* the path, such that the output of each layer doesn't have the activation function applied
 */
inline float ReLU(float input){
    return std::max(0.0f,input);
}

// Helper function to split vector into chunks
template<typename T>
inline std::vector<std::vector<T>> chunks(const std::vector<T>& list, size_t chunk_size) {
    std::vector<std::vector<T>> result;
    for (size_t i = 0; i < list.size(); i += chunk_size) {
        std::vector<T> chunk(list.begin() + i, list.begin() + std::min(i + chunk_size, list.size()));
        result.push_back(chunk);
    }
    return result;
}

/**
 * @brief calculates the index in the input vector that this piece-square combination refers to
 * @param sq the square that the new or removed piece is on
 * @param piece the piece that has been moved
 */
inline int CalculateEfficientUpdateIndex(Square sq, Piece piece){
    assert(SquareUtils::IsValid(sq));
    assert(!PieceUtils::IsEmpty(piece));

    int result = sq + piece*64;
    assert(result < EfficientUpdateInputSize);
    return result;
}

inline bool FloatsEqual(float a, float b){
    float difference = std::abs(a - b);
    return difference < 0.01f;
}

template<typename T>
bool CompareCArrays(const T* first, const T* other, int size){
    for(int i=0;i<size;i++){
        T first_val = first[i];
        T other_val = other[i];
        if(!FloatsEqual(first_val, other_val)){
            return false;
        }
    }
    return true;
}

} // namespace NN_Helpers
