#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h> 

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
    uint32_t sf = (instruction >> 31) & 0x1;     // Size flag (bit 31)
    uint32_t rd = (instruction >> 0) & 0x1F;     // Destination register (bits 0-4)
    uint32_t rn = (instruction >> 5) & 0x1F;     // First operand register (bits 5-9)
    uint32_t imm12 = (instruction >> 10) & 0xFFF; // Immediate value (bits 10-21)
    uint32_t sh = (instruction >> 22) & 1;       // Shift flag (bit 22)
    uint32_t opc = (instruction >> 29) & 0x3;    // Operation code (bits 29-30)

    uint64_t operand1 = cpu->regs[rn];
    uint64_t operand2 = (sh == 1) ? (imm12 << 12) : imm12;
    uint64_t result;

    if (sf == 0) { // 32-bit mode
        operand1 &= 0xFFFFFFFF;
        operand2 &= 0xFFFFFFFF;
    }

    switch (opc) {
        case 0x0: // ADD
            result = operand1 + operand2;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            cpu->regs[rd] = result;
            break;
        case 0x1: // ADDS
            result = operand1 + operand2;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            cpu->regs[rd] = result;
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (sf == 0) ? (int32_t)result < 0 : (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0); // Zero flag
            set_flag(cpu, C_FLAG, operand1 > UINT64_MAX - operand2); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)operand1 > 0 && (int64_t)operand2 > 0 && (int64_t)result < 0) ||
                                 ((int64_t)operand1 < 0 && (int64_t)operand2 < 0 && (int64_t)result > 0)); // Overflow flag
            break;
        case 0x2: // SUB
            result = operand1 - operand2;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            cpu->regs[rd] = result;
            break;
        case 0x3: // SUBS
            result = operand1 - operand2;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            cpu->regs[rd] = result;
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (sf == 0) ? (int32_t)result < 0 : (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0); // Zero flag
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

void apply_shift(uint64_t *value, uint32_t shift_type, uint32_t shift_amount, uint32_t sf) {
    if (sf == 0) { // 32-bit mode
        *value &= 0xFFFFFFFF; // Mask to 32 bits
        switch (shift_type) {
            case 0: // LSL - Logical Shift Left
                *value <<= shift_amount;
                *value &= 0xFFFFFFFF; // Ensure result is 32-bit
                break;
            case 1: // LSR - Logical Shift Right
                *value >>= shift_amount;
                *value &= 0xFFFFFFFF; // Ensure result is 32-bit
                break;
            case 2: // ASR - Arithmetic Shift Right
                *value = (int32_t)(*value) >> shift_amount;
                *value &= 0xFFFFFFFF; // Ensure result is 32-bit
                break;
            case 3: // ROR - Rotate Right
                *value = (*value >> shift_amount) | (*value << (32 - shift_amount));
                *value &= 0xFFFFFFFF; // Ensure result is 32-bit
                break;
            default:
                break;
        }
    } else { // 64-bit mode
        switch (shift_type) {
            case 0: // LSL - Logical Shift Left
                *value <<= shift_amount;
                break;
            case 1: // LSR - Logical Shift Right
                *value >>= shift_amount;
                break;
            case 2: // ASR - Arithmetic Shift Right
                *value = (int64_t)(*value) >> shift_amount;
                break;
            case 3: // ROR - Rotate Right
                *value = (*value >> shift_amount) | (*value << (64 - shift_amount));
                break;
            default:
                break;
        }
    }
}

void arithmetic_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 31) & 0x1;      // Size flag (bit 31)
    uint32_t rd = (instruction >> 0) & 0x1F;      // Destination register (bits 0-4)
    uint32_t rn = (instruction >> 5) & 0x1F;      // First operand register (bits 5-9)
    uint32_t rm = (instruction >> 16) & 0x1F;     // Second operand register (bits 16-20)
    uint32_t operand = (instruction >> 10) & 0x3F; // Shift amount (bits 10-15)
    uint32_t shift = (instruction >> 22) & 0x3;   // Shift type (bits 22-23)
    uint32_t opc = (instruction >> 29) & 0x3;     // Operation code (bits 29-30)

    uint64_t operand_value = cpu->regs[rm];
    apply_shift(&operand_value, shift, operand, sf);

    uint64_t result;
    uint64_t operand1 = cpu->regs[rn];

    if (sf == 0) { // 32-bit mode
        operand1 &= 0xFFFFFFFF;
    }

    switch (opc) {
        case 0x0: // ADD
            result = operand1 + operand_value;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            cpu->regs[rd] = result;
            break;
        case 0x1: // ADDS
            result = operand1 + operand_value;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            cpu->regs[rd] = result;
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (sf == 0) ? (int32_t)result < 0 : (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0); // Zero flag
            set_flag(cpu, C_FLAG, operand1 > UINT64_MAX - operand_value); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)operand1 > 0 && (int64_t)operand_value > 0 && (int64_t)result < 0) ||
                                  ((int64_t)operand1 < 0 && (int64_t)operand_value < 0 && (int64_t)result > 0)); // Overflow flag
            break;
        case 0x2: // SUB
            result = operand1 - operand_value;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            cpu->regs[rd] = result;
            break;
        case 0x3: // SUBS
            result = operand1 - operand_value;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            cpu->regs[rd] = result;
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (sf == 0) ? (int32_t)result < 0 : (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0); // Zero flag
            set_flag(cpu, C_FLAG, operand1 >= operand_value); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)operand1 > 0 && (int64_t)operand_value < 0 && (int64_t)result < 0) ||
                                  ((int64_t)operand1 < 0 && (int64_t)operand_value > 0 && (int64_t)result > 0)); // Overflow flag
            break;
        default:
            printf("Unknown arithmetic immediate opcode: 0x%x\n", opc);
            return;
    }
    printf("arithmetic_instruction: X%d = X%d %s X%d (result: %llu)\n", rd, rn, (opc & 0x2) ? "-" : "+", rm, result);
}

void logical_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 31) & 0x1;      // Size flag (bit 31)
    uint32_t rd = (instruction >> 0) & 0x1F;      // Destination register (bits 0-4)
    uint32_t rn = (instruction >> 5) & 0x1F;      // First operand register (bits 5-9)
    uint32_t rm = (instruction >> 16) & 0x1F;     // Second operand register (bits 16-20)
    uint32_t operand = (instruction >> 10) & 0x3F; // Shift amount (bits 10-15)
    uint32_t shift = (instruction >> 22) & 0x3;   // Shift type (bits 22-23)
    uint32_t N = (instruction >> 21) & 0x1;       // Bitwise negation flag
    uint32_t opc = (instruction >> 29) & 0x3;     // Operation code (bits 29-30)

    uint64_t operand_value = cpu->regs[rm];
    apply_shift(&operand_value, shift, operand, sf);

    if (N) {
        operand_value = ~operand_value;
    }

    uint64_t result;
    uint64_t operand1 = cpu->regs[rn];

    if (sf == 0) { // 32-bit mode
        operand1 &= 0xFFFFFFFF;
    }

    switch (opc) {
        case 0x0: // AND/BIC
            result = operand1 & operand_value;
            if (N) {
                result = operand1 & ~operand_value;
            }
            break;
        case 0x1: // ORR/ORN
            result = operand1 | operand_value;
            if (N) {
                result = operand1 | ~operand_value;
            }
            break;
        case 0x2: // EOR/EON
            result = operand1 ^ operand_value;
            if (N) {
                result = operand1 ^ ~operand_value;
            }
            break;
        case 0x3: // ANDS/BICS
            result = operand1 & operand_value;
            if (N) {
                result = operand1 & ~operand_value;
            }
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (sf == 0) ? (int32_t)result < 0 : (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0); // Zero flag
            set_flag(cpu, C_FLAG, 0); // Carry flag (logical operations set C to 0)
            set_flag(cpu, V_FLAG, 0); // Overflow flag (logical operations set V to 0)
            break;
        default:
            printf("Unknown logical instruction opcode: 0x%x\n", opc);
            return;
    }

    if (sf == 0) { // 32-bit mode
        result &= 0xFFFFFFFF; // Ensure result is 32-bit
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

    if (sf == 0) { // 32-bit mode
        operand1 &= 0xFFFFFFFF;
        operand2 &= 0xFFFFFFFF;
        product &= 0xFFFFFFFF;
        accumulate &= 0xFFFFFFFF;
    }

    if (x == 0) { // MADD: Rd := Ra + (Rn * Rm)
        result = accumulate + product;
    } else {     // MSUB: Rd := Ra - (Rn * Rm)
        result = accumulate - product;
    }

    if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
    cpu->regs[rd] = result;
    printf("multiply_instruction: X%d = X%d %c (X%d * X%d) (result: %llu)\n", rd, ra, (x == 0 ? '+' : '-'), rn, rm, result);
}


void data_processing_register(CPUState *cpu, uint32_t instruction) {
    uint32_t op = (instruction >> 24) & 0x1; // Arithmetic or Logical
    uint32_t M = (instruction >> 28) & 0x1; // Arith/Logical or Multiply
    if (M == 1) {
        multiply_instruction(cpu, instruction);
    } else if (op == 0) {
        logical_instruction(cpu, instruction);
    } else {
        arithmetic_instruction(cpu, instruction);
    }
}

void single_data_transfer(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 30) & 0x1;      // Size flag (bit 30)
    uint32_t L = (instruction >> 22) & 0x1;       // Load/Store flag (bit 22)
    uint32_t opc = (instruction >> 23) & 0x3;     // Option (bits 23-24)
    uint32_t offset = (instruction >> 10) & 0xFFF; // Offset (bits 10-21)
    int32_t simm9 = (instruction >> 12) & 0x1FF;  // Signed 9-bit offset (bits 12-20)
    uint32_t Xn = (instruction >> 5) & 0x1F;      // Base register (bits 5-9)
    uint32_t Rt = instruction & 0x1F;             // Target register (bits 0-4)
    uint64_t address = cpu->regs[Xn];
    uint64_t data;

    switch (opc) {
        case 0: // Unsigned Offset
            address += (sf == 0) ? (offset * 4) : (offset * 8);
            break;
        case 1: // Pre-Indexed
            address += (int64_t)(simm9 << 55) >> 55; // Sign-extend 9-bit offset
            cpu->regs[Xn] = address; // Write-back the updated address to the base register
            break;
        case 2: // Post-Indexed
            // Address remains unchanged initially
            break;
        default:
            printf("Unknown addressing mode: %u\n", opc);
            return;
    }

    if (L) { // Load
        if (sf == 0) { // 32-bit load
            data = memory[address / sizeof(uint32_t)] & 0xFFFFFFFF;
            cpu->regs[Rt] = data;
        } else { // 64-bit load
            data = *(uint64_t *)(memory + address / sizeof(uint32_t));
            cpu->regs[Rt] = data;
        }
        printf("LOAD: X%d = [X%d + %d] (address: 0x%llx, data: %llu)\n", Rt, Xn, (sf == 0) ? (offset * 4) : (offset * 8), address, data);
    } else { // Store
        if (sf == 0) { // 32-bit store
            data = cpu->regs[Rt] & 0xFFFFFFFF;
            memory[address / sizeof(uint32_t)] = data;
        } else { // 64-bit store
            data = cpu->regs[Rt];
            *(uint64_t *)(memory + address / sizeof(uint32_t)) = data;
        }
        printf("STORE: [X%d + %d] = X%d (address: 0x%llx, data: %llu)\n", Xn, (sf == 0) ? (offset * 4) : (offset * 8), Rt, address, data);
    }

    // Handle post-index addressing mode
    if (opc == 2) {
        cpu->regs[Xn] += (int64_t)(simm9 << 55) >> 55; // Sign-extend 9-bit offset and update base register
    }
}

void load_literal(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 30) & 0x1;      // Size flag (bit 30)
    int32_t simm19 = (instruction >> 5) & 0x7FFFF; // Signed immediate (bits 5-23)
    uint32_t Rt = instruction & 0x1F;             // Target register (bits 0-4)

    // Sign-extend the 19-bit immediate
    int64_t offset = (simm19 << 45) >> 45;
    uint64_t address = cpu->pc + (offset * 4);
    uint64_t data;

    if (sf == 0) { // 32-bit load
        data = memory[address / sizeof(uint32_t)] & 0xFFFFFFFF;
        cpu->regs[Rt] = data;
    } else { // 64-bit load
        data = *(uint64_t *)(memory + address / sizeof(uint32_t));
        cpu->regs[Rt] = data;
    }
    printf("LOAD_LITERAL: X%d = [PC + %lld] (address: 0x%llx, data: %llu)\n", Rt, offset, address, data);
}

void branch_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t op = (instruction >> 26) & 0x3F; // Bits 31-26
    int32_t simm26, simm19;
    int64_t offset;

    switch (op) {
        case 0x05: // Unconditional branch
            simm26 = (instruction & 0x3FFFFFF); // Bits 25-0
            offset = (int64_t)simm26 << 2;   
            cpu->pc += offset - 4; // Set the PC to the target address
            printf("Unconditional branch1 to PC=0x%llx\n", cpu->pc);
            break; // Do not increment PC after the branch
        case 0x35: // Register branch
            {
                uint32_t Xn = (instruction >> 5) & 0x1F; // Bits 9-5
                printf("we here?");
                cpu->pc = cpu->regs[Xn];
                printf("Register branch to PC=0x%llx\n", cpu->pc);
            }
            break;
        case 0x15: // Conditional branch
            simm19 = (instruction >> 5) & 0x7FFFF; // Bits 23-5
            uint32_t cond = instruction & 0xF;     // Bits 3-0
            offset = (int64_t)simm19 << 2;
            if (check_condition(cpu, cond)) {
                printf("hi");
                printf("offset=%llx\n",offset);
                cpu->pc += offset - 4; // Set the PC to the target address
                printf("Conditional branch to PC=0x%llx on condition %x\n", cpu->pc, cond);
                return; // Do not increment PC after the branch
            } else {
                printf("Condition %x not met, no branch taken\n", cond);
            }
            break;
        default:
            printf("Unknown branch instruction: 0x%08x\n", instruction);
            break;
    }
}

int check_condition(CPUState *cpu, uint32_t cond) {
    uint32_t Z = (cpu->pstate >> Z_FLAG) & 1;
    uint32_t N = (cpu->pstate >> N_FLAG) & 1;
    uint32_t V = (cpu->pstate >> V_FLAG) & 1;

    switch (cond) {
        case 0x0: return Z == 1;                        // EQ
        case 0x1: return Z == 0;                        // NE
        case 0xA: return N == V;                        // GE
        case 0xB: return N != V;                        // LT
        case 0xC: return Z == 0 && N == V;              // GT
        case 0xD: return Z == 1 || N != V;              // LE
        case 0xE: return 1;                             // AL (always)
        default: return 0;                              // Unknown condition
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
        case 0xd: // Multiply (Register)
            multiply_instruction(cpu, instruction);
            break;
        case 0x6: // Loads and Stores
        case 0x7:
            single_data_transfer(cpu, instruction);
            break;
        case 0xC: // Load Literal
            load_literal(cpu, instruction);
            break;
        case 0xA: // Branches
        case 0xB:
            branch_instruction(cpu, instruction);
            printf("Branch instruction: 0x%08x\n", instruction);
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