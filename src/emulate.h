#include <stdint.h>

#define MEMORY_SIZE (2 * 1024 * 1024) // 2MB of memory
#define MEMORY_OFFSET 0 // Offsets by MEMORY_OFFSET * 4 bits
#define HALT 0x8A000000

#define N_FLAG 3 // Negative
#define Z_FLAG 2 // Zero
#define C_FLAG 1 // Carry
#define V_FLAG 0 // Overflow

typedef struct {
    uint64_t regs[31]; // General purpose registers X0-X30
    uint64_t zr;       // Zero register
    uint64_t pc;       // Program Counter
    uint32_t pstate;   // Processor state (NZCV)
} CPUState;

extern uint32_t memory[MEMORY_SIZE / sizeof(uint32_t)];

void load_binary(const char *filename, uint32_t *memory, size_t *size);
void init_cpu(CPUState *cpu);
void set_flag(CPUState *cpu, int flag_pos, int condition);
int check_condition(CPUState *cpu, uint32_t cond);
void format_pstate(uint8_t pstate, char *buffer);
void arth_immediate(CPUState *cpu, uint32_t instruction);
void and_register(CPUState *cpu, uint32_t instruction);
void move_immediate(CPUState *cpu, uint32_t instruction);
void data_processing_immediate(CPUState *cpu, uint32_t instruction);
void apply_shift(uint64_t *value, uint32_t shift_type, uint32_t shift_amount, uint32_t sf);
void arithmetic_instruction(CPUState *cpu, uint32_t instruction);
void logical_instruction(CPUState *cpu, uint32_t instruction);
void multiply_instruction(CPUState *cpu, uint32_t instruction);
void data_processing_register(CPUState *cpu, uint32_t instruction);
void single_data_transfer(CPUState *cpu, uint32_t instruction);
void branch_instruction(CPUState *cpu, uint32_t instruction);
void decode_and_execute(CPUState *cpu, uint32_t *memory, uint32_t instruction);
void emulate(CPUState *cpu, uint32_t *memory, size_t size);
void output_state(CPUState *cpu, uint32_t *memory, size_t size);