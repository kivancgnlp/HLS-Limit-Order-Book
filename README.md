# HLS Limit Order Book

This repository contains a portfolio-oriented, synthesizable prototype of a single-instrument electronic limit order book written in C++ for Vitis HLS style flows.

The design is intentionally hardware-oriented rather than software-optimized:

- no dynamic memory
- no STL containers
- no recursion
- fixed-size arrays and bounded loops
- explicit state and command structs

The project is being built in stages. This commit implements **Stage 1**:

- separate bid and ask books
- add limit order only
- price-priority insertion
- bounded FIFO order storage inside each price level
- simple top-level function for HLS integration
- basic C++ testbench

## Repo Layout

- `README.md`
- `src/lob.hpp`
- `src/lob.cpp`
- `src/top.cpp`
- `tb/tb_lob.cpp`

## Stage 1 Architecture

The book is modeled as two fixed-capacity arrays of price levels:

- bids are kept in descending price order
- asks are kept in ascending price order

Each `PriceLevel` contains:

- a fixed price
- aggregate quantity
- a bounded FIFO queue of orders at that price

This is a deliberate FPGA-friendly compromise. Instead of a tree or hash-map based software book, the implementation uses:

- bounded linear searches
- bounded array shifts when inserting a new price level
- static storage sized by compile-time constants

That is less flexible than a production software matching engine, but much more aligned with synthesis and predictable hardware resource usage.

## Data Structures

The core compile-time limits live in [`src/lob.hpp`](/Users/kivanc/GitHub/HLS-Limit-Order-Book/src/lob.hpp):

- `MAX_ORDERS`
- `MAX_PRICE_LEVELS`
- `MAX_ORDERS_PER_LEVEL`
- `INVALID_PRICE`

Primary structs:

- `Order`: one resting order
- `PriceLevel`: one bounded FIFO queue at a single price
- `Command`: one input event
- `CommandResult`: one output status plus a compact book summary
- `LimitOrderBook`: full single-instrument book state

## Supported Operations

Stage 1 supports:

- `CMD_RESET`
- `CMD_ADD`

Stage 1 does **not** support yet:

- matching
- partial fills
- cancel by order ID
- market orders
- multiple instruments

## Why This Differs From A Normal Software LOB

A normal software limit order book often uses:

- balanced trees
- linked lists
- hash tables
- dynamic allocation

Those designs are convenient on CPUs but are not ideal starting points for FPGA/HLS work. This project instead favors:

- static memory layout
- explicit capacities
- deterministic control flow
- small, analyzable loops

That makes the code less feature-rich, but more realistic as a starting point for synthesis experiments.

## HLS Considerations

Current code is structured so later Vitis HLS work can focus on:

- pipelining bounded search loops
- partitioning small arrays where useful
- separating hot-path state from debug-oriented outputs
- evaluating the cost of price-level shifts versus more specialized indexing

For Stage 1, pragmas are intentionally kept out of the code. The priority is a clean baseline model first.

## Build The Testbench

Example local build:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic src/lob.cpp src/top.cpp tb/tb_lob.cpp -I./src -o tb_lob
./tb_lob
```

## Roadmap

### Stage 2

- add matching against the opposite side
- support partial fills
- maintain book quantities after fills

### Stage 3

- add cancel by order ID
- introduce a bounded lookup structure for cancels

### Stage 4

- HLS-oriented cleanup
- comments for suggested pragmas
- loop and interface review for synthesis

### Stage 5

- portfolio-quality README polish
- clearer architecture diagrams and tradeoff notes

## Current Limitations

- single instrument only
- bounded number of price levels
- bounded queue depth per price level
- insertion-only behavior in this stage
- duplicate order IDs are not rejected yet
- no matching logic yet, so crossed books can exist in Stage 1

## Future Work

- multi-instrument support
- deeper queue handling
- market orders
- risk checks
- AXI-stream interface adaptation
- latency/resource benchmarking in Vitis HLS
