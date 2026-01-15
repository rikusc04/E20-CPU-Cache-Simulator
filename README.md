# E20-CPU-Cache-Simulator

## Overview
This project implements a simulator for the E20 processor, a teaching ISA commonly used in university Computer Architecture courses. The simulator executes E20 machine code by modeling registers, memory, and control flow exactly as defined in the E20 manual.

The base simulator (`e20_sim.cpp`) loads a machine-code program into memory, initializes the program counter and registers, and executes instructions sequentially until a halt condition is reached. Instructions are decoded using bitwise operations to extract opcodes, registers, and immediates, and execution behavior is selected based on the decoded opcode and function bits.

The cache-enabled simulator (`e20_sim_cache.cpp`) extends this functionality by adding support for configurable cache hierarchies. It simulates one or two cache levels (L1 and optional L2), logs cache hits, misses, and store operations, and updates cache state on each memory access.

## Building & Running the E20 Simulator
- Navigate to the `/tests` directory to find the respective tests for validating the behavior of e20_sim (found in `/tests/e20_sim_tests`) and e20_sim_cache (found in `/tests/e20_sim_cache_tests`)
1. **To build both of the E20 Simulators, compile the .cpp files:**
    ```bash
    g++ -std=c++14 -O2 e20_sim.cpp -o e20_sim
    ```
    ```bash
    g++ -std=c++14 -O2 e20_sim_cache.cpp -o e20_sim_cache 
    ```
    - Note: This project was creating using C++14, but can compile with later versions and some earlier versions
2. **To run the e20_sim, use command:**
    ```bash
    ./e20_sim ./tests/e20_sim_tests/loop3.bin
    ```
    - More generally:
         ```bash
         ./e20_sim ./tests/e20_sim_tests/{test_name}.bin
         ```
    - Note that the test must be a .bin file, not a .s file
3. **To run the e20_sim_cache using 1 cache (L1 cache), use command:**
    ```bash
    ./e20_sim_cache ./tests/e20_sim_cache_tests/test.bin --cache 2,1,1
    ```
    - This program must always use at least 1 cache (L1 cache); this is the standard way to run the program
    - More generally, the command is given by:
         ```bash
         ./e20_sim_cache ./tests/e20_sim_cache_tests/{test_name}.bin --cache size,associativity,blocksize
         ```
4. **To run the e20_sim_cache using 2 caches (L1 cache and L2 cache), use command:**
    ```bash
    ./e20_sim_cache ./tests/e20_sim_cache_tests/test.bin --cache 2,1,1,8,4,2
    ```
    - More generally, the command is given by:
         ```bash
         ./e20_sim_cache ./tests/e20_sim_cache_tests/{test_name}.bin --cache size1,associativity1,blocksize1,size2,associativity2,blocksize2
         ```

## What is a valid cache configuration?
1. For each cache level, the following must hold true:
    ```bash
    size > 0
    associativity > 0
    blocksize > 0
    ```
3. Cache size must be divisible by (associativity × blocksize)
   - Formula:
     ```bash
     number_of_rows = size / (associativity × blocksize)
     ```
   - number_of_rows must be an integer where number_of_rows ≥ 1
   - If this is not true, the cache cannot be laid out into rows/sets
4. Associativity cannot exceed the number of blocks
   - The total number of blocks is given by:
         ```bash
         blocks = size / blocksize
         ```
   - So, you must have:
         ```bash
         associativity ≤ blocks
         ```
5. Although the simulators themselves do not have any check to validate whether a valid cache configuration was used, the simulators are desgined to work with the three rules listed above.
6. In addiiton, the L1 and L2 caches are validated independently. And although there is no requirement that L2 be larger than L1, the test cases provided generally assume it is

## FAQ about E20:

**Q**: What are the initial values of the registers, the program counter, and the memory cells?  
**A**: The intital values of the registers is 0, the initial value of the program counter is 0, and the initial value of all the memory cells is 0.

**Q**: What should happen if a program sets a register to a value outside of the range expressible as a 16-bit unsigned number? Consider both positive and negative numbers that cannot be expressed in 16 bits.  
**A**: The register will wrap around and take the value that comes next. (If it goes past the maximum number it should wrap around to the minimum number, and if it goes past the minimum number, it should wrap around to the maximum number).

**Q**: What should happen if a program tries to change the value of $0?  
**A**: The program shouldn't be able to change the vlaue of $0, but it is still valid to write code that attempts to change the value of $0.

**Q**: What should happen if a program uses slt to compare a negative number to a positive number?  
**A**: The program will compare the values as unsigned 16 bit integers, not as 2's complement numbers. This means that the negative number would just be a positive number with an msb of 1.

**Q**: What range of memory address are valid? What should happen if a program tries to read or write a memory cell whose address is outside of the range of valid addresses?  
**A**: The valid range of memory addresses are: 0 - 8191. If a program tries to read or write to a memory cell whose address is outside of the range of valid addresses, it will not be allowed.

**Q**: What should happen if a program sets the program counter to a value outside of the range of valid addresses?  
**A**: The program counter should take into account the overflow and wrap around to the correct value.

**Q**: What should happen if a program uses a negative immediate value in addi or jeq?  
**A**: The program will still run as intended. The negative immidiate value in addi and jeq is sign extended, so negative numbers are accounted for by the program.

**Q**: What should happen if a program uses a negative immediate value in lw or sw?  
**A**: The program will still run as intended. The negative immidiate value in lw and sw is sign extended, so negative numbers are accounted for by the program.

**Q**: What should happen if a program modifies a memory cell containing machine code?  
**A**: The program should allow this to happen. Although this is very uncommon to do becuase it is overwritting a pre-existing instruction, there is no rule stating it cannot be done.

**Q**: What should happen if the program counter reaches the address of the last memory cell?  
**A**: Once the program counter reaches the address of the last memory cell, it should wrap around to the first memory cell for the next instruction (assuming the last memory cell is not a instruction that halts the prgram).

**Q**: When should your simulator stop?  
**A**: The simulator should stop if there is an instruction that forces the pc to jump to its current position, also known as the halt instruciton.
