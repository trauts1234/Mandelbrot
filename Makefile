#this is all written by GPT
#makefile is difficult :(

# Compiler and flags
CXX = g++#clang++

# Core flags
CXXFLAGS = -std=c++20 -march=native -Wall -Wextra -pedantic# -Werror

# Dependency generation flags
DEPFLAGS = -MMD -MF $(@:.o=.d)

# Debug and release flags
#debug level 3, no optimisation, keep frame pointer, crash if signed int overflow
DEBUG_FLAGS = -g3 -O0 -fno-omit-frame-pointer -fsanitize=signed-integer-overflow -fsanitize=address

FAST_DEBUG_FLAGS = -g3 -Ofast -ffast-math -fno-omit-frame-pointer -DNDEBUG

#max optimisation, remove calls to assert, link time optimisation, this program is a single app, not a library
RELEASE_FLAGS = -Ofast -ffast-math -DNDEBUG# -flto -fwhole-program # last two flags make program not work for some reason

ENGINE_FLAG = -DRUN_UCI
TRAINING_DATA_FLAG = -DQUIET_GEN

# Source and build directories
SRC_DIR = src
BUILD_DIR = build
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))
NN_TEXT = weights_biases.txt
NN_HEADER_GEN = src/network_data.txt # Path to the generated file

# Dependency files
DEPS = $(OBJ_FILES:.o=.d)

# Default target
all: release

#function to convert neural network into a header file, then convert the header file from c style array to std::vector<>
#sed commands:
#1. replace unsigned char with std::vector<unsigned char>
#2. remove []
#3. swap the length variable from unsigned int to const size_t
$(NN_HEADER_GEN): $(NN_TEXT)
	xxd -i $< | sed \
		-e 's/unsigned char/static const std::vector<unsigned char>/' \
		-e 's/\[\]//' \
		-e 's/unsigned int /const size_t /' \
		> $@

# Debug build
debug: CXXFLAGS += $(ENGINE_FLAG) $(DEBUG_FLAGS)
debug: $(NN_HEADER_GEN) $(BUILD_DIR) $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(OBJ_FILES) -o main

# Release build
release: CXXFLAGS += $(ENGINE_FLAG) $(RELEASE_FLAGS)
release: $(NN_HEADER_GEN) $(BUILD_DIR) $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(OBJ_FILES) -o main

fast_debug: CXXFLAGS += $(ENGINE_FLAG) $(FAST_DEBUG_FLAGS)
fast_debug: $(NN_HEADER_GEN) $(BUILD_DIR) $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(OBJ_FILES) -o main

training_json_create: CXXFLAGS += $(RELEASE_FLAGS) $(TRAINING_DATA_FLAG) -I/opt/vcpkg/installed/x64-linux/include/ #define the quiet generation flags, and link in the json parse library
training_json_create: $(NN_HEADER_GEN) $(BUILD_DIR) $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(OBJ_FILES) -o main


# Build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Rule to build object files and dependency files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# Include generated dependency files
-include $(DEPS)

# Clean build files
clean:
	rm -rf $(BUILD_DIR) main $(NN_HEADER_GEN)
