# HLS Limit Order Book

This repository contains a portfolio-oriented, synthesizable prototype of a single-instrument electronic limit order book written in C++ for Vitis HLS style flows.

The design is intentionally hardware-oriented rather than software-optimized:

- no dynamic memory
- no STL containers
- no recursion
- fixed-size arrays and bounded loops
- explicit state and command structs

The project is being built in stages. This revision implements **Stage 2**:

- separate bid and ask books
- add limit orders
- price-priority insertion
- bounded FIFO order storage inside each price level
- aggressive-order matching against the opposite side
- partial fills with residual quantity optionally resting in-book
- simple top-level function for HLS integration
- C++ testbench with insertion and matching scenarios

## Repo Layout

- `README.md`
- `src/lob.hpp`
- `src/lob.cpp`
- `src/top.cpp`
- `tb/tb_lob.cpp`

## Current Architecture

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

Current implementation supports:

- `CMD_RESET`
- `CMD_ADD`

`CMD_ADD` behaves as a limit order submission:

- if it does not cross the opposite side, it rests in-book
- if it crosses, it matches from the best opposite price level first
- within one price level, resting orders are consumed FIFO from the queue head
- if the incoming order is only partially filled, the residual quantity rests on its own side at its limit price

The implementation does **not** support yet:

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

For Stage 2, pragmas are still intentionally kept out of the code. The priority remains a clean baseline model before applying directive-level tuning.

## Build The Testbench

Example local build:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic src/lob.cpp src/top.cpp tb/tb_lob.cpp -I./src -o tb_lob
./tb_lob
```

## Roadmap

### Stage 2

- completed in the current revision

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
- no cancel path yet
- duplicate order IDs are not rejected yet
- no bounded order-ID lookup yet, so cancel support is deferred to Stage 3
- trade output is intentionally compact: aggregate execution fields are returned rather than a variable-length list of fill records

## Future Work

- multi-instrument support
- deeper queue handling
- market orders
- risk checks
- AXI-stream interface adaptation
- latency/resource benchmarking in Vitis HLS
