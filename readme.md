# Chess Engine

Welcome to the repository for **Mandelbrot**, an approximately 2500 ELO chess engine supporting UCI.

## Features

Below is a list of key features currently supported by the engine:

- **UCI support**
- **Bitboards**
- **NNUE-like neural network**
- **Aspiration windows**
- **Late move reductions**
- **Null move pruning**
- **Search Extensions**
- **Transposition table**
- **Killer moves + History heuristic**
- **Legal move generator:** runs at ~210M nps

## Installation

To get started with the Chess Engine, follow these steps:

1. **Clone the repository**:
   ```bash
   git clone https://github.com/trauts1234/Mandelbrot.git
   cd Mandelbrot
   ```

2. **Build the binary**
    ```bash
    make
    ```
3. **Use the generated binary**
    ```bash
    ./main
    ```
