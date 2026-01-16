#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <limits>
#include <iomanip>
#include <regex>
using namespace std;

/*
Notes:
A cache has a maximum of two levels, known as L1 and L2
Each level has a certain number of rows
Each row has a certain amount of blocks
*/

// Some helpful constant values to use
size_t const static NUM_REGS = 8;
size_t const static MEM_SIZE = 1<<13;
size_t const static REG_SIZE = 1<<16;

struct Block {
    uint16_t tag = -1;
};

struct Row {
    vector<Block> blocks_vec; // stores all the blocks of any given row, this .size() also can never be more than 4; .size() will give you the current number of blocks in the row
    int associativity; // the max size of the blocks vector; aka how many blocks can be stored in one row; you can keep on adding a block into the blocks vector as long as the blocks_vec.size() < associativity
};

struct Level {
    Level(int cache_size, int num_of_rows, int associativity, int blocksize) : blocksize(blocksize) { 
        for (int i = 0; i < num_of_rows; i++) {
            Row new_row;
            new_row.blocks_vec = vector<Block>(associativity);
            new_row.associativity = associativity;
            rows_vec.push_back(new_row);
        }
    }
    vector<Row> rows_vec; // each level object has a set number of rows, with each row containing a vector of blocks of size associativity
    int blocksize; // blocksize is how many values you can store in one block (all these values will have the same tag)
};

struct Cache {
    vector<Level> levels_vec;
};

/*
    Prints out the correctly-formatted configuration of a cache.
    @param cache_name The name of the cache. "L1" or "L2"
    @param size The total size of the cache, measured in memory cells. Excludes metadata
    @param assoc The associativity of the cache. One of [1,2,4,8,16]
    @param blocksize The blocksize of the cache. One of [1,2,4,8,16,32,64])
    @param num_rows The number of rows in the given cache.
*/
void print_cache_config(const string &cache_name, int size, int assoc, int blocksize, int num_rows);
/*
    Prints out a correctly-formatted log entry.
    @param cache_name The name of the cache where the event occurred. "L1" or "L2"
    @param status The kind of cache event. "SW", "HIT", or "MISS"
    @param pc The program counter of the memory access instruction
    @param addr The memory address being accessed.
    @param row The cache row or set number where the data is stored.
*/
void print_log_entry(const string& cache_name, const string& status, uint16_t pc, uint16_t addr, int row);
/*
    Loads an E20 machine code file into the list provided by mem. Assume that mem is large enough to hold the values in the machine code file.
    @param f Open file to read from
    @param mem Array represetnting memory into which to read program
*/
void load_machine_code(ifstream &f, uint16_t mem[]);
/*
    Prints the current state of the simulator, including the current program counter, the current register values, and the first memquantity elements of memory.
    @param pc The final value of the program counter
    @param regs Final value of all registers
    @param memory Final value of memory
    @param memquantity How many words of memory to dump
*/
void print_state(uint16_t pc, uint16_t regs[], uint16_t memory[], size_t memquantity);
void sign_extend7_func(uint16_t& imm7);
void simulate_cache_access(uint16_t address, uint16_t index, Cache& a_cache, vector<int>& parts, bool is_store_word);
int run_e20_simulator(uint16_t regs_arr[], uint16_t pc, uint16_t* memory_arr, Cache& a_cache, vector<int>& parts);

void print_cache_config(const string &cache_name, int size, int assoc, int blocksize, int num_rows) {
    cout << "Cache " << cache_name << " has size " << size << ", associativity " << assoc << ", blocksize " << blocksize << ", rows " << num_rows << endl;
}

void print_log_entry(const string& cache_name, const string& status, uint16_t pc, uint16_t addr, int row) {
    cout << left << setw(8) << cache_name + " " + status <<  right << " pc:" << setw(5) << pc << "\taddr:" << setw(5) << addr << "\trow:" << setw(4) << row << endl;
}

void load_machine_code(ifstream &f, uint16_t mem[]) {
    regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
    size_t expectedaddr = 0;
    string line;
    while (getline(f, line)) {
        smatch sm;
        if (!regex_match(line, sm, machine_code_re)) {
            cerr << "Can't parse line: " << line << endl;
            exit(1);
        }
        size_t addr = stoi(sm[1], nullptr, 10);
        unsigned instr = stoi(sm[2], nullptr, 2);
        if (addr != expectedaddr) {
            cerr << "Memory addresses encountered out of sequence: " << addr << endl;
            exit(1);
        }
        if (addr >= MEM_SIZE) {
            cerr << "Program too big for memory" << endl;
            exit(1);
        }
        expectedaddr ++;
        mem[addr] = instr;
    }
}

void print_state(uint16_t pc, uint16_t regs[], uint16_t memory[], size_t memquantity) {
    cout << setfill(' ');
    cout << "Final state:" << endl;
    cout << "\tpc=" <<setw(5)<< pc << endl;

    for (size_t reg=0; reg<NUM_REGS; reg++) {
        cout << "\t$" << reg << "="<<setw(5)<<regs[reg]<<endl;
    }
    cout << setfill('0');
    bool cr = false;
    for (size_t count=0; count<memquantity; count++) {
        cout << hex << setw(4) << memory[count] << " ";
        cr = true;
        if (count % 8 == 7) {
            cout << endl;
            cr = false;
        }
    }
    if (cr) {
        cout << endl;
    }
}

void sign_extend7_func(uint16_t& imm7) {
    if ((imm7 >> 6) == 1) {
        imm7 = imm7 | 65408;
    }
}

void simulate_cache_access(uint16_t address, uint16_t index, Cache& a_cache, vector<int>& parts, bool is_store_word) {
    uint16_t l1size = 0;
    uint16_t l1assoc = 0;
    uint16_t l1blocksize = 0;
    uint16_t l1rows = 0;
    uint16_t l1blockid = 0;
    uint16_t l1row_num = 0;
    uint16_t l1tag = 0;

    uint16_t l2size = 0;
    uint16_t l2assoc = 0;
    uint16_t l2blocksize = 0;
    uint16_t l2rows = 0;
    uint16_t l2blockid = 0;
    uint16_t l2row_num = 0;
    uint16_t l2tag = 0;

    // L1 cache info
    l1size = parts[0];
    l1assoc = parts[1];
    l1blocksize = parts[2];

    l1rows = l1size / (l1assoc * l1blocksize);
    l1blockid = address / l1blocksize;
    l1row_num = l1blockid % l1rows;
    l1tag = l1blockid / l1rows;

    if (parts.size() == 6) { // l2 cache info 
        l2size = parts[3];
        l2assoc = parts[4];
        l2blocksize = parts[5];

        l2rows = l2size / (l2assoc * l2blocksize);
        l2blockid = address / l2blocksize;
        l2row_num = l2blockid % l2rows;
        l2tag = l2blockid / l2rows;
    }

    bool l1tag_was_found = false;
    size_t l1tag_block_index;
    for (size_t i = 0; i < a_cache.levels_vec[0].rows_vec[l1row_num].blocks_vec.size(); i++) {
        if (a_cache.levels_vec[0].rows_vec[l1row_num].blocks_vec[i].tag == l1tag) {
                l1tag_was_found = true;
                l1tag_block_index = i;
                break;
            }
    }
    if (is_store_word) {
        print_log_entry("L1", "SW", index, address, l1row_num);
    }
    else {
        if (l1tag_was_found) {
            print_log_entry("L1", "HIT", index, address, l1row_num);
        }
        else {
            print_log_entry("L1", "MISS", index, address, l1row_num);
        }
    }

    // Update L1 cache
    if (l1tag_was_found) { // HIT
        a_cache.levels_vec[0].rows_vec[l1row_num].blocks_vec.erase(a_cache.levels_vec[0].rows_vec[l1row_num].blocks_vec.begin() + l1tag_block_index); //erase the first element
        Block a_block;
        a_block.tag = l1tag;
        a_cache.levels_vec[0].rows_vec[l1row_num].blocks_vec.push_back(a_block);
    }
    else { // MISSS
        a_cache.levels_vec[0].rows_vec[l1row_num].blocks_vec.erase(a_cache.levels_vec[0].rows_vec[l1row_num].blocks_vec.begin());
        Block a_block;
        a_block.tag = l1tag;
        a_cache.levels_vec[0].rows_vec[l1row_num].blocks_vec.push_back(a_block);
    }

    if (a_cache.levels_vec.size() == 2 && (!l1tag_was_found || is_store_word)) { // if L1 and L2 cache is available and if l1 tag was not found
        bool l2tag_was_found = false;
        size_t l2tag_block_index;
        for (size_t i = 0; i < a_cache.levels_vec[1].rows_vec[l2row_num].blocks_vec.size(); i++) {
            if (a_cache.levels_vec[1].rows_vec[l2row_num].blocks_vec[i].tag == l2tag) {
                    l2tag_was_found = true;
                    l2tag_block_index = i;
                    break;
                }
        }

        if (is_store_word) {
            print_log_entry("L2", "SW", index, address, l2row_num);
        }
        else {
            if (l2tag_was_found) {
                print_log_entry("L2", "HIT", index, address, l2row_num);
            }
            else {
                print_log_entry("L2", "MISS", index, address, l2row_num);
            }
        }
        // update L2 cache
        if (l2tag_was_found) { // HIT
            a_cache.levels_vec[1].rows_vec[l2row_num].blocks_vec.erase(a_cache.levels_vec[1].rows_vec[l2row_num].blocks_vec.begin() + l2tag_block_index);
            Block a_block;
            a_block.tag = l2tag;
            a_cache.levels_vec[1].rows_vec[l2row_num].blocks_vec.push_back(a_block);
        }
        else { // MISS
            a_cache.levels_vec[1].rows_vec[l2row_num].blocks_vec.erase(a_cache.levels_vec[1].rows_vec[l2row_num].blocks_vec.begin());
            Block a_block;
            a_block.tag = l2tag;
            a_cache.levels_vec[1].rows_vec[l2row_num].blocks_vec.push_back(a_block);
        }
    }
}

int run_e20_simulator(uint16_t regs_arr[], uint16_t pc, uint16_t* memory_arr, Cache& a_cache, vector<int>& parts) {
    bool halt = false;

    while (!halt)
    {
        uint16_t index = pc % MEM_SIZE;
        uint16_t instruction = memory_arr[index];

        // Extract all possible combinations
        uint16_t opcode = instruction >> 13;
        uint16_t bits10_12 = (instruction >> 10) & 7;
        uint16_t bits7_9 = (instruction >> 7) & 7;
        uint16_t bits4_6 = (instruction >> 4) & 7;
        uint16_t bits0_3 = instruction & 15;
        uint16_t bits0_6 = instruction & 127;
        uint16_t bits0_12 = instruction & 8191;
        sign_extend7_func(bits0_6);

        if (opcode == 0) { //add, sub, or, and, slt, jr, 
            if (bits0_3 == 0) { // add
                regs_arr[bits4_6] = regs_arr[bits10_12] + regs_arr[bits7_9];
                pc+=1;
            }
            else if (bits0_3 == 1) { // sub
                regs_arr[bits4_6] = regs_arr[bits10_12] - regs_arr[bits7_9];
                pc+=1;
            }
            else if (bits0_3 == 2) { // or
                regs_arr[bits4_6] = regs_arr[bits10_12] | regs_arr[bits7_9];
                pc+=1;
            }
            else if (bits0_3 == 3) { // and
                regs_arr[bits4_6] = regs_arr[bits10_12] & regs_arr[bits7_9];
                pc+=1;
            }
            else if (bits0_3 == 4) { // slt
                if (regs_arr[bits10_12] < regs_arr[bits7_9]) {
                    regs_arr[bits4_6] = 1;
                }
                else {
                    regs_arr[bits4_6] = 0;
                }
                pc+=1;
            }
            else if (bits0_3 == 8) { // jr
                pc = regs_arr[bits10_12];
            }
            regs_arr[0] = 0;
        }
        else if (opcode == 1) { // addi
            regs_arr[bits7_9] = regs_arr[bits10_12] + bits0_6;
            regs_arr[0] = 0;
            pc+=1;
        }
        else if (opcode == 2) { // j
            if (pc == bits0_12) {
                halt = true;
            }
            pc = bits0_12;
        }
        else if (opcode == 3) { // jal
            regs_arr[7] = pc + 1;
            pc = bits0_12;
        }
        else if (opcode == 4) { // lw
            uint16_t address = (regs_arr[bits10_12] + bits0_6) % MEM_SIZE; 
            simulate_cache_access(address, index, a_cache, parts, false);
            regs_arr[bits7_9] = memory_arr[address];
            regs_arr[0] = 0;
            pc+=1;
        }
        else if (opcode == 5) { // sw
            uint16_t address = (regs_arr[bits10_12] + bits0_6) % MEM_SIZE;
            simulate_cache_access(address, index, a_cache, parts, true);
            memory_arr[address] = regs_arr[bits7_9];
            pc+=1;
        }
        else if (opcode == 6) { // jeq
            if (regs_arr[bits10_12] == regs_arr[bits7_9]) {
                pc = pc + 1 + bits0_6;
            }
            else {
                pc+=1;
            }
        }
        else if (opcode == 7) { // slti
            if (regs_arr[bits10_12] < bits0_6) {
                regs_arr[bits7_9] = 1;
            }
            else {
                regs_arr[bits7_9] = 0;
            }
            regs_arr[0] = 0;
            pc+=1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // Parse the command-line arguments
    char *filename = nullptr;
    bool do_help = false;
    bool arg_error = false;
    string cache_config;
    for (int i=1; i<argc; i++) {
        string arg(argv[i]);
        if (arg.rfind("-",0)==0) {
            if (arg== "-h" || arg == "--help") {
                do_help = true;
            }
            else if (arg=="--cache") {
                i++;
                if (i>=argc) {
                    arg_error = true;
                }
                else {
                    cache_config = argv[i];
                }
            }
            else
                arg_error = true;
        } 
        else {
            if (filename == nullptr) {
                filename = argv[i];
            }
            else {
                arg_error = true;
            }
        }
    }
    /* Display error message if appropriate */
    if (arg_error || do_help || filename == nullptr) {
        cerr << "usage " << argv[0] << " [-h] [--cache CACHE] filename" << endl << endl;
        cerr << "Simulate E20 cache" << endl << endl;
        cerr << "positional arguments:" << endl;
        cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl<<endl;
        cerr << "optional arguments:"<<endl;
        cerr << "  -h, --help  show this help message and exit"<<endl;
        cerr << "  --cache CACHE  Cache configuration: size,associativity,blocksize (for one"<<endl;
        cerr << "                 cache) or"<<endl;
        cerr << "                 size,associativity,blocksize,size,associativity,blocksize"<<endl;
        cerr << "                 (for two caches)"<<endl;
        return 1;
    }

    ifstream f(filename);
    if (!f.is_open()) {
        cerr << "Can't open file "<<filename<<endl;
        return 1;
    }

    // Initialize pc, registers, and memory
    uint16_t pc = 0;
    uint16_t regs_arr[NUM_REGS];
    for (size_t i = 0; i < NUM_REGS; i++) {
        regs_arr[i] = 0;
    }

    uint16_t memory_arr[MEM_SIZE];
    for (size_t i = 0; i < MEM_SIZE; i++) {
        memory_arr[i] = 0;
    }

    // Load f and parse using load_machine_code
    load_machine_code(f, memory_arr);

    // parse cache config
    if (cache_config.size() > 0) {
        vector<int> parts;
        size_t pos;
        size_t lastpos = 0;
        while ((pos = cache_config.find(",", lastpos)) != string::npos) {
            parts.push_back(stoi(cache_config.substr(lastpos,pos)));
            lastpos = pos + 1;
        }
        parts.push_back(stoi(cache_config.substr(lastpos)));
        if (parts.size() == 3) {
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];
            int l1_rows = L1size / (L1assoc * L1blocksize);
            
            Level l1 = Level(L1size, l1_rows, L1assoc, L1blocksize);
            Cache a_cache;
            a_cache.levels_vec.push_back(l1);

            print_cache_config("L1", L1size, L1assoc, L1blocksize, l1_rows);
            run_e20_simulator(regs_arr, pc, memory_arr, a_cache, parts);
        } 
        else if (parts.size() == 6) {
            int L1size = parts[0];
            int L1assoc = parts[1];
            int L1blocksize = parts[2];
            int L2size = parts[3];
            int L2assoc = parts[4];
            int L2blocksize = parts[5];

            int l1_rows = L1size / (L1assoc * L1blocksize);
            int l2_rows = L2size / (L2assoc * L2blocksize);

            Level l1 = Level(L1size, l1_rows, L1assoc, L1blocksize);
            Level l2 = Level(L2size, l2_rows, L2assoc, L2blocksize);
            Cache a_cache;
            a_cache.levels_vec.push_back(l1);
            a_cache.levels_vec.push_back(l2);

            print_cache_config("L1", L1size, L1assoc, L1blocksize, l1_rows);
            print_cache_config("L2", L2size, L2assoc, L2blocksize, l2_rows);

            run_e20_simulator(regs_arr, pc, memory_arr, a_cache, parts);

        } 
        else {
            cerr << "Invalid cache config"  << endl;
            return 1;
        }
    }
    return 0;
}