#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MEMORY_SIZE (2 * 1024 * 1024) // 2MB of memory
#define HALT 0x8A000000

#define N_FLAG 3 // Negative
#define Z_FLAG 2 // Zero
#define C_FLAG 1 // Carry
#define V_FLAG 0 // Overflow

typedef struct {
    uint64_t regs[31]; // General-purpose registers X0-X30
    uint64_t pc;       // Program Counter
    uint32_t pstate;   // Processor state (NZCV)
} CPUState;

uint32_t memory[MEMORY_SIZE / sizeof(uint32_t)];

void load_binary(const char *filename, uint32_t *memory, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_END);
    *size = ftell(file) / sizeof(uint32_t);
    fseek(file, 0, SEEK_SET);
    fread(memory, sizeof(uint32_t), *size, file);
    fclose(file);
}

void init_cpu(CPUState *cpu) {
    memset(cpu->regs, 0, sizeof(cpu->regs));
    cpu->pc = 0;
    cpu->pstate = 0x4; // Z flag set
}

void set_flag(CPUState *cpu, int flag_pos, int condition) {
    if (condition) {
        cpu->pstate |= (1 << flag_pos); // Set the flag bit
    } else {
        cpu->pstate &= ~(1 << flag_pos); // Clear the flag bit
    }
}

void arth_immediate(CPUState *cpu, uint32_t instruction) {
    uint32_t rd = (instruction >> 0) & 0x1F;     // Destination register (bits 0-4)
    uint32_t rn = (instruction >> 5) & 0x1F;     // First operand register (bits 5-9)
    uint32_t imm12 = (instruction >> 10) & 0xFFF; // Immediate value (bits 10-21)
    uint32_t sh = (instruction >> 22) & 1;       // Shift flag (bit 22)
    uint32_t opc = (instruction >> 29) & 0x3;    // Operation code (bits 29-30)

    uint64_t operand1 = cpu->regs[rn];
    uint64_t operand2 = (sh == 1) ? (imm12 << 12) : imm12;
    uint64_t result;

    switch (opc) {
        case 0x0: // ADD
            result = operand1 + operand2;
            cpu->regs[rd] = result;
            break;
        case 0x1: // ADDS
            result = operand1 + operand2;
            cpu->regs[rd] = result;
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0);         // Zero flag
            set_flag(cpu, C_FLAG, operand1 > UINT64_MAX - operand2); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)operand1 > 0 && (int64_t)operand2 > 0 && (int64_t)result < 0) ||
                                 ((int64_t)operand1 < 0 && (int64_t)operand2 < 0 && (int64_t)result > 0)); // Overflow flag
            break;
        case 0x2: // SUB
            result = operand1 - operand2;
            cpu->regs[rd] = result;
            break;
        case 0x3: // SUBS
            result = operand1 - operand2;
            cpu->regs[rd] = result;
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0);         // Zero flag
            set_flag(cpu, C_FLAG, operand1 >= operand2); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)operand1 > 0 && (int64_t)operand2 < 0 && (int64_t)result < 0) ||
                                 ((int64_t)operand1 < 0 && (int64_t)operand2 > 0 && (int64_t)result > 0)); // Overflow flag
            break;
        default:
            printf("Unknown arithmetic immediate opcode: 0x%x\n", opc);
            return;
    }
    printf("arth_immediate: X%d = X%d %s %llu (result: %llu)\n", rd, rn, (opc & 0x2) ? "-" : "+", operand2, cpu->regs[rd]);
}

void format_pstate(uint8_t pstate, char *buffer) {
    buffer[0] = (pstate & (1 << N_FLAG)) ? 'N' : '-'; // N flag (bit 3)
    buffer[1] = (pstate & (1 << Z_FLAG)) ? 'Z' : '-'; // Z flag (bit 2)
    buffer[2] = (pstate & (1 << C_FLAG)) ? 'C' : '-'; // C flag (bit 1)
    buffer[3] = (pstate & (1 << V_FLAG)) ? 'V' : '-'; // V flag (bit 0)
    buffer[4] = '\0'; // Null-terminate the string
}

void and_register(CPUState *cpu, uint32_t instruction) {
    uint32_t rd = (instruction >> 0) & 0x1F;
    uint32_t rn = (instruction >> 5) & 0x1F;
    uint32_t rm = (instruction >> 16) & 0x1F;
    cpu->regs[rd] = cpu->regs[rn] & cpu->regs[rm];
    printf("AND_REGISTER: X%d = X%d & X%d (result: %llu)\n", rd, rn, rm, cpu->regs[rd]);
}

void move_immediate(CPUState *cpu, uint32_t instruction) {
    uint32_t rd = (instruction >> 0) & 0x1F;     // Destination register (bits 0-4)
    uint32_t imm16 = (instruction >> 5) & 0xFFFF; // Immediate value (bits 5-20)
    uint32_t hw = (instruction >> 21) & 0x3;     // Logical shift value (bits 21-22)
    uint32_t opc = (instruction >> 29) & 0x3;    // Opcode (bits 29-30)
    uint64_t shifted_imm16 = imm16 << (hw * 16); // Shift the immediate value by hw * 16 bits

    switch (opc) {
        case 0x0: // movn: Move wide with NOT
            cpu->regs[rd] = ~shifted_imm16;
            break;
        case 0x2: // movz: Move wide with zero
            cpu->regs[rd] = shifted_imm16;
            break;
        case 0x3: // movk: Move wide with keep
            cpu->regs[rd] = (cpu->regs[rd] & ~(0xFFFF << (hw * 16))) | shifted_imm16;
            break;
        default:
            printf("Unknown Data Processing Immediate opcode: 0x%x\n", opc);
            return;
    }
    printf("MOVE_IMMEDIATE: X%d = %llu\n", rd, cpu->regs[rd]);
}

void data_processing_immediate(CPUState *cpu, uint32_t instruction) {
    uint32_t opcode = (instruction >> 23) & 0x7; // Bits 23-25
    switch (opcode) {
        case 0x2: // ADD (immediate)
            arth_immediate(cpu, instruction);
            break;
        case 0x5: // MOV (immediate)
            move_immediate(cpu, instruction);
            break;
        default:
            printf("Unknown Data Processing Immediate opcode: 0x%x\n", opcode);
            break;
    }
}

void apply_shift(uint64_t *value, uint32_t shift_type, uint32_t shift_amount) {
    switch (shift_type) {
        case 0: // LSL
            *value <<= shift_amount;
            break;
        case 1: // LSR
            *value >>= shift_amount;
            break;
        case 2: // ASR
            *value = (int64_t)(*value) >> shift_amount;
            break;
        case 3: // ROR
            *value = (*value >> shift_amount) | (*value << (64 - shift_amount));
            break;
        default:
            break;
    }
}

void arithmetic_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t rd = (instruction >> 0) & 0x1F;
    uint32_t rn = (instruction >> 5) & 0x1F;
    uint32_t rm = (instruction >> 16) & 0x1F;
    uint32_t operand = (instruction >> 10) & 0x3F;
    uint32_t shift = (instruction >> 22) & 0x3;
    uint32_t opc = (instruction >> 29) & 0x3;

    uint64_t operand_value = cpu->regs[rm];
    apply_shift(&operand_value, shift, operand);

    uint64_t result;
    switch (opc) {
        case 0x0: // ADD
            result = cpu->regs[rn] + operand_value;
            cpu->regs[rd] = result;
            break;
        case 0x1: // ADDS
            result = cpu->regs[rn] + operand_value;
            cpu->regs[rd] = result;
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0);         // Zero flag
            set_flag(cpu, C_FLAG, cpu->regs[rn] > UINT64_MAX - operand_value); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)cpu->regs[rn] > 0 && (int64_t)operand_value > 0 && (int64_t)result < 0) ||
                                 ((int64_t)cpu->regs[rn] < 0 && (int64_t)operand_value < 0 && (int64_t)result > 0)); // Overflow flag
            break;
        case 0x2: // SUB
            result = cpu->regs[rn] - operand_value;
            cpu->regs[rd] = result;
            break;
        case 0x3: // SUBS
            result = cpu->regs[rn] - operand_value;
            cpu->regs[rd] = result;
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0);         // Zero flag
            set_flag(cpu, C_FLAG, cpu->regs[rn] >= operand_value); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)cpu->regs[rn] > 0 && (int64_t)operand_value < 0 && (int64_t)result < 0) ||
                                 ((int64_t)cpu->regs[rn] < 0 && (int64_t)operand_value > 0 && (int64_t)result > 0)); // Overflow flag
            break;
        default:
            printf("Unknown arithmetic immediate opcode: 0x%x\n", opc);
            return;
    }
    printf("arithmetic_instruction: X%d = X%d + X%d (result: %llu)\n", rd, rn, rm, result);
}

void logical_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t rd = (instruction >> 0) & 0x1F;
    uint32_t rn = (instruction >> 5) & 0x1F;
    uint32_t rm = (instruction >> 16) & 0x1F;
    uint32_t operand = (instruction >> 10) & 0x3F;
    uint32_t shift = (instruction >> 22) & 0x3;
    uint32_t N = (instruction >> 21) & 0x1;
    uint32_t opc = (instruction >> 29) & 0x3;

    uint64_t operand_value = cpu->regs[rm];
    apply_shift(&operand_value, shift, operand);
    if (N) {
        operand_value = ~operand_value;
    }

    uint64_t result;
    switch (opc) {
        case 0x0: // AND/BIC
            result = cpu->regs[rn] & operand_value;
            if (N) {
                result = cpu->regs[rn] & ~operand_value;
            }
            break;
        case 0x1: // ORR/ORN
            result = cpu->regs[rn] | operand_value;
            if (N) {
                result = cpu->regs[rn] | ~operand_value;
            }
            break;
        case 0x2: // EOR/EON
            result = cpu->regs[rn] ^ operand_value;
            if (N) {
                result = cpu->regs[rn] ^ ~operand_value;
            }
            break;
        case 0x3: // ANDS/BICS
            result = cpu->regs[rn] & operand_value;
            if (N) {
                result = cpu->regs[rn] & ~operand_value;
            }
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0);         // Zero flag
            set_flag(cpu, C_FLAG, 0);                   // Carry flag (logical operations set C to 0)
            set_flag(cpu, V_FLAG, 0);                   // Overflow flag (logical operations set V to 0)
            break;
        default:
            printf("Unknown logical instruction opcode: 0x%x\n", opc);
            return;
    }
    cpu->regs[rd] = result;
    printf("logical_instruction: X%d = X%d <logical_op> X%d (result: %llu)\n", rd, rn, rm, result);
}

void multiply_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 31) & 0x1;      // Data size (ignored for now)
    uint32_t rm = (instruction >> 16) & 0x1F;     // Second operand register
    uint32_t ra = (instruction >> 10) & 0x1F;     // Accumulate register
    uint32_t x = (instruction >> 15) & 0x1;       // Multiply-Add or Multiply-Sub flag
    uint32_t rn = (instruction >> 5) & 0x1F;      // First operand register
    uint32_t rd = instruction & 0x1F;             // Destination register

    uint64_t operand1 = cpu->regs[rn];
    uint64_t operand2 = cpu->regs[rm];
    uint64_t accumulate =  (ra == 31) ? 0 : cpu->regs[ra]; // Use zero if ra is 11111 (31)
    uint64_t product = operand1 * operand2;
    uint64_t result;

    if (x == 0) { // MADD: Rd := Ra + (Rn * Rm)
        result = accumulate + product;
    } else {     // MSUB: Rd := Ra - (Rn * Rm)
        result = accumulate - product;
    }

    cpu->regs[rd] = result;
    printf("multiply_instruction: X%d = X%d %c (X%d * X%d) (result: %llu)\n", rd, ra, (x == 0 ? '+' : '-'), rn, rm, result);
}


void data_processing_register(CPUState *cpu, uint32_t instruction) {
    uint32_t op = (instruction >> 24) & 0x1; // Arithmetic or Logical
    uint32_t M = (instruction >> 28) & 0x1; // Arith/Logical or Multiply
    if (op == 0) {
        logical_instruction(cpu, instruction);
    } else {
        arithmetic_instruction(cpu, instruction);
    }
}

void decode_and_execute(CPUState *cpu, uint32_t *memory, uint32_t instruction) {
    printf("Decoding instruction at PC=0x%llx: 0x%08x\n", cpu->pc, instruction);
    if (instruction == HALT) {
        printf("HALT instruction executed at PC=0x%llx\n", cpu->pc);
        return;
    }

    uint32_t op0 = (instruction >> 25) & 0xF; // Bits 28-25
    switch (op0) {
        case 0x8: // Data Processing (Immediate)
        case 0x9:
            data_processing_immediate(cpu, instruction);
            break;
        case 0x5: // Data Processing (Register)
            data_processing_register(cpu, instruction);
            break;
        case 0x6: // Loads and Stores
        case 0x7:
            printf("Loads and Stores instruction: 0x%08x\n", instruction);
            break;
        case 0xA: // Branches
        case 0xB:
            printf("Branch instruction: 0x%08x\n", instruction);
            break;
            case 0xd: // Multiply (Register)
            multiply_instruction(cpu, instruction);
            break;
        default:
            printf("Unknown instruction group: op0=0x%x\n", op0);
            break;
    }
}

void emulate(CPUState *cpu, uint32_t *memory, size_t size) {
    while (cpu->pc < size * 4) {
        uint32_t instruction = memory[cpu->pc / 4];
        decode_and_execute(cpu, memory, instruction);
        cpu->pc += 4; // Increment PC by 4 (size of an instruction)
        if (instruction == HALT) break; // Stop execution on HALT
    }
}

void output_state(CPUState *cpu, uint32_t *memory, size_t size) {
    char pstate_str[5];
    format_pstate(cpu->pstate, pstate_str);
    printf("Registers:\n");
    for (int i = 0; i < 31; i++) {
        printf("X%02d = %016llx\n", i, cpu->regs[i]);
    }
    printf("PC = %016llx\n\n", cpu->pc-4);
    printf("PSTATE : %s\n", pstate_str); // Use formatted PSTATE string
    printf("Non-Zero Memory:\n");
    for (size_t i = 0; i < size; i++) {
        if (memory[i] != 0) {
            printf("0x%08lx: 0x%08x\n", i * 4, memory[i]);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary file> [output file]\n", argv[0]);
        return EXIT_FAILURE;
    }

    CPUState cpu;
    init_cpu(&cpu);

    size_t size;
    load_binary(argv[1], memory, &size);

    emulate(&cpu, memory, size);

    if (argc == 3) {
        freopen(argv[2], "w", stdout);
    }
    output_state(&cpu, memory, size);

    return EXIT_SUCCESS;
}