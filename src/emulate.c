#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emulate.h"

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
    cpu->zr = 0;
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

int check_condition(CPUState *cpu, uint32_t cond) {
    uint32_t N = (cpu->pstate >> N_FLAG) & 1;
    uint32_t Z = (cpu->pstate >> Z_FLAG) & 1;
    uint32_t C = (cpu->pstate >> C_FLAG) & 1;
    uint32_t V = (cpu->pstate >> V_FLAG) & 1; 

    switch (cond) {
        case 0x0: return Z == 1;                        // EQ: Equal
        case 0x1: return Z == 0;                        // NE: Not equal
        case 0x2: return C == 1;                        // CS/HS: Carry set / unsigned higher or same
        case 0x3: return C == 0;                        // CC/LO: Carry clear / unsigned lower
        case 0x4: return N == 1;                        // MI: Minus / negative
        case 0x5: return N == 0;                        // PL: Plus / positive or zero
        case 0x6: return V == 1;                        // VS: Overflow
        case 0x7: return V == 0;                        // VC: No overflow
        case 0x8: return (C == 1 && Z == 0);            // HI: Unsigned higher
        case 0x9: return (C == 0 || Z == 1);            // LS: Unsigned lower or same
        case 0xA: return N == V;                        // GE: Signed greater than or equal
        case 0xB: return N != V;                        // LT: Signed less than
        case 0xC: return (Z == 0 && N == V);            // GT: Signed greater than
        case 0xD: return (Z == 1 || N != V);            // LE: Signed less than or equal
        case 0xE: return 1;                             // AL: Always (unconditional)
        default: return 0;                              // NV: Never (shouldn't happen)
    }
}

void format_pstate(uint8_t pstate, char *buffer) {
    buffer[0] = (pstate & (1 << N_FLAG)) ? 'N' : '-'; // N flag (bit 3)
    buffer[1] = (pstate & (1 << Z_FLAG)) ? 'Z' : '-'; // Z flag (bit 2)
    buffer[2] = (pstate & (1 << C_FLAG)) ? 'C' : '-'; // C flag (bit 1)
    buffer[3] = (pstate & (1 << V_FLAG)) ? 'V' : '-'; // V flag (bit 0)
    buffer[4] = '\0'; // Null-terminate the string
}

void arithmetic_immediate(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 31) & 0x1;     // Size flag (bit 31)
    uint32_t rd = (instruction >> 0) & 0x1F;     // Destination register (bits 4-0)
    uint32_t rn = (instruction >> 5) & 0x1F;     // First operand register (bits 9-5)
    uint32_t imm12 = (instruction >> 10) & 0xFFF; // Immediate value (bits 21-10)
    uint32_t sh = (instruction >> 22) & 1;       // Shift flag (bit 22)
    uint32_t opc = (instruction >> 29) & 0x3;    // Operation code (bits 30-29)

    uint64_t operand1 = cpu->regs[rn];
    uint64_t operand2 = (sh == 1) ? (imm12 << 12) : imm12;
    uint64_t result;

    switch (opc) {
        case 0x0: // ADD
            result = operand1 + operand2;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            break;
        case 0x1: // ADDS
            result = operand1 + operand2;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
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
            break;
        case 0x3: // SUBS
            result = operand1 - operand2;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
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
    if (rd != 31) {
        cpu->regs[rd] = result;
    }

    printf("arithmetic_immediate: X%d = X%d %s %lu (result: %lu)\n", rd, rn, (opc & 0x2) ? "-" : "+", operand2, cpu->regs[rd]);
}

void and_register(CPUState *cpu, uint32_t instruction) {
    uint32_t rd = (instruction >> 0) & 0x1F;
    uint32_t rn = (instruction >> 5) & 0x1F;
    uint32_t rm = (instruction >> 16) & 0x1F;
    cpu->regs[rd] = cpu->regs[rn] & cpu->regs[rm];
    printf("and_register: X%d = X%d & X%d (result: %lu)\n", rd, rn, rm, cpu->regs[rd]);
}

void move_immediate(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 31) & 0x1;      // Size flag (bit 31)
    uint32_t rd = (instruction >> 0) & 0x1F;      // Destination register (bits 4-0)
    uint32_t imm16 = (instruction >> 5) & 0xFFFF; // Immediate value (bits 20-5)
    uint32_t hw = (instruction >> 21) & 0x3;      // Logical shift value (bits 22-21)
    uint32_t opc = (instruction >> 29) & 0x3;     // Opcode (bits 30-29)
    uint64_t shifted_imm16 = (uint64_t)imm16 << (hw * 16); // Shift the immediate value by hw * 16 bits
    if (rd != 31) {
        switch (opc) {
            case 0x0: // movn: Move wide with NOT
                if (sf == 0) {
                    cpu->regs[rd] = ~shifted_imm16 & 0xFFFFFFFF; // 32-bit result
                } else {
                    cpu->regs[rd] = ~shifted_imm16; // 64-bit result
                }
                break;
            case 0x2: // movz: Move wide with zero
                if (sf == 0) {
                    cpu->regs[rd] = shifted_imm16 & 0xFFFFFFFF; // 32-bit result
                } else {
                    cpu->regs[rd] = shifted_imm16; // 64-bit result
                }
                break;
            case 0x3: // movk: Move wide with keep
                if (sf == 0) {
                    cpu->regs[rd] = (cpu->regs[rd] & ~(0xFFFF << (hw * 16))) | (shifted_imm16 & 0xFFFFFFFF); // 32-bit result
                } else {
                    cpu->regs[rd] = (cpu->regs[rd] & ~(0xFFFFULL << (hw * 16))) | shifted_imm16; // 64-bit result
                }
                break;
            default:
                printf("Unknown Data Processing Immediate opcode: 0x%x\n", opc);
                return;
        }
    }
    // Ensure the result is in the correct bit-width
    if (sf == 0) {
        cpu->regs[rd] &= 0xFFFFFFFF; // Ensure 32-bit result
    }

    printf("move_immediate: X%d = %lu\n", rd, cpu->regs[rd]);
}


void data_processing_immediate(CPUState *cpu, uint32_t instruction) {
    uint32_t opcode = (instruction >> 23) & 0x7; // Bits 25-23
    switch (opcode) {
        case 0x2: // ADD/SUB (immediate)
            arithmetic_immediate(cpu, instruction);
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
                break;
            case 1: // LSR - Logical Shift Right
                *value >>= shift_amount;
                break;
            case 2: // ASR - Arithmetic Shift Right
                *value = (int32_t)(*value) >> shift_amount;
                break;
            case 3: // ROR - Rotate Right
                *value = (*value >> shift_amount) | (*value << (32 - shift_amount));
                break;
            default:
                break;
        }
        *value &= 0xFFFFFFFF; // Ensure result is 32-bit
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

void arithmetic_register(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 31) & 0x1;      // Size flag (bit 31)
    uint32_t rd = (instruction >> 0) & 0x1F;      // Destination register (bits 4-0)
    uint32_t rn = (instruction >> 5) & 0x1F;      // First operand register (bits 9-5)
    uint32_t rm = (instruction >> 16) & 0x1F;     // Second operand register (bits 20-16)
    uint32_t operand = (instruction >> 10) & 0x3F; // Shift amount (bits 15-10)
    uint32_t shift = (instruction >> 22) & 0x3;   // Shift type (bits 23-22)
    uint32_t opc = (instruction >> 29) & 0x3;     // Operation code (bits 30-29)

    uint64_t operand_value = cpu->regs[rm];
    apply_shift(&operand_value, shift, operand, sf);

    uint64_t result;
    uint64_t operand1 = cpu->regs[rn];

    if (sf == 0) { // 32-bit mode
        operand1 &= 0xFFFFFFFF;
    }

    printf("arithmetic_register: PC=0x%lx, instruction=0x%08x, opc=0x%x, rd=%d, rn=%d, rm=%d\n",
           cpu->pc, instruction, opc, rd, rn, rm);

    switch (opc) {
        case 0x0: // ADD
            result = operand1 + operand_value;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            break;
        case 0x1: // ADDS
            result = operand1 + operand_value;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (sf == 0) ? (int32_t)result < 0 : (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0); // Zero flag
            set_flag(cpu, C_FLAG, (result < operand1)); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)operand1 > 0 && (int64_t)operand_value > 0 && (int64_t)result < 0) ||
                                  ((int64_t)operand1 < 0 && (int64_t)operand_value < 0 && (int64_t)result > 0)); // Overflow flag
            break;
        case 0x2: // SUB
            result = operand1 - operand_value;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            break;
        case 0x3: // SUBS
            result = operand1 - operand_value;
            if (sf == 0) result &= 0xFFFFFFFF; // 32-bit result
            // Update PSTATE flags
            set_flag(cpu, N_FLAG, (sf == 0) ? (int32_t)result < 0 : (int64_t)result < 0); // Negative flag
            set_flag(cpu, Z_FLAG, result == 0); // Zero flag
            set_flag(cpu, C_FLAG, operand1 >= operand_value); // Carry flag
            set_flag(cpu, V_FLAG, ((int64_t)operand1 > 0 && (int64_t)operand_value < 0 && (int64_t)result < 0) ||
                                  ((int64_t)operand1 < 0 && (int64_t)operand_value > 0 && (int64_t)result > 0)); // Overflow flag
            break;
        default:
            printf("Unknown arithmetic instruction opcode: 0x%x\n", opc);
            return;
    }
    if (rd != 31) { //ZR Register Case
        cpu->regs[rd] = result;
    } else {
        printf("Attempt to write to ZR prevented. Result: 0x%lx\n", result);
    }

    printf("arithmetic_register: X%d = X%d %s X%d (result: %lu)\n", rd, rn, (opc & 0x2) ? "-" : "+", rm, result);
}

void logical_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 31) & 0x1;      // Size flag (bit 31)
    uint32_t rd = (instruction >> 0) & 0x1F;      // Destination register (bits 4-0)
    uint32_t rn = (instruction >> 5) & 0x1F;      // First operand register (bits 9-5)
    uint32_t rm = (instruction >> 16) & 0x1F;     // Second operand register (bits 20-16)
    uint32_t operand = (instruction >> 10) & 0x3F; // Shift amount (bits 15-10)
    uint32_t shift = (instruction >> 22) & 0x3;   // Shift type (bits 23-22)
    uint32_t N = (instruction >> 21) & 0x1;       // Bitwise negation flag
    uint32_t opc = (instruction >> 29) & 0x3;     // Operation code (bits 30-29)

    uint64_t operand_value = cpu->regs[rm];
    apply_shift(&operand_value, shift, operand, sf);

    if (N) {
        operand_value = ~operand_value;
    }

    uint64_t result;
    uint64_t operand1 = cpu->regs[rn];
    const char *operation = "";

    if (sf == 0) { // 32-bit mode
        operand1 &= 0xFFFFFFFF;
    }

    switch (opc) {
        case 0x0: // AND/BIC
            result = operand1 & operand_value;
            operation = N ? "BIC" : "AND";
            break;
        case 0x1: // ORR/ORN
            result = operand1 | operand_value;
            printf("1) %ld 2) %ld", operand1, operand_value);
            operation = N ? "ORN" : "ORR";
            break;
        case 0x2: // EOR/EON
            result = operand1 ^ operand_value;
            operation = N ? "EON" : "EOR";
            break;
        case 0x3: // ANDS/BICS
            result = operand1 & operand_value;
            operation = N ? "BICS" : "ANDS";
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
    if (rd != 31) {
        cpu->regs[rd] = result;
    }
    printf("logical_instruction: X%d = X%d %s X%d (result: %lu)\n", rd, rn, operation, rm, result);
}


void multiply_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 31) & 0x1;      // Data size (ignored for now)
    uint32_t rm = (instruction >> 16) & 0x1F;     // Second operand register
    uint32_t ra = (instruction >> 10) & 0x1F;     // Accumulate register
    uint32_t x = (instruction >> 15) & 0x1;       // Multiply-Add or Multiply-Sub flag
    uint32_t rn = (instruction >> 5) & 0x1F;      // First operand register
    uint32_t rd = instruction & 0x1F;             // Destination register
    if (rd == 31) { return; }                     // if rd is ZR register, abort

    uint64_t operand1 = cpu->regs[rn];
    uint64_t operand2 = cpu->regs[rm];
    uint64_t accumulate = cpu->regs[ra];
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
    printf("multiply_instruction: X%d = X%d %c (X%d * X%d) (result: %lu)\n", rd, ra, (x == 0 ? '+' : '-'), rn, rm, result);
}


void data_processing_register(CPUState *cpu, uint32_t instruction) {
    uint32_t op = (instruction >> 24) & 0x1; // Arithmetic or Logical
    uint32_t M = (instruction >> 28) & 0x1; // Arith/Logical or Multiply
    if (M == 1) {
        multiply_instruction(cpu, instruction);
    } else if (op == 0) {
        logical_instruction(cpu, instruction);
    } else {
        arithmetic_register(cpu, instruction);
    }
}

void single_data_transfer(CPUState *cpu, uint32_t instruction) {
    uint32_t sf = (instruction >> 30) & 0x1;      // Size flag (bit 30)
    uint32_t literal = !((instruction >> 29) & 0x1); // Literal flag (bit 29)
    uint32_t L = (instruction >> 22) & 0x1;       // Load/Store flag (bit 22)
    uint32_t U = (instruction >> 24) & 0x1;       // Option (bit 24)
    uint32_t R = (instruction >> 21) & 0x1; // R flag
    uint32_t offset = (instruction >> 10) & 0xFFF; // Offset (bits 21-10)
    int32_t simm9 = (instruction >> 12) & 0x1FF;  // Signed 9-bit offset (bits 20-12)
    uint32_t Xn = (instruction >> 5) & 0x1F;      // Base register (bits 9-5)
    uint32_t Rt = instruction & 0x1F;             // Target register (bits 4-0)
    uint32_t I = (instruction >> 11) & 0x1; // Index flag (bit 11)
    uint8_t *byte_memory = (uint8_t *)memory;
    uint64_t address;
    uint64_t data;

    // Sign-extend 9-bit offset
    if (simm9 & 0x100) {
        simm9 |= ~0x1FF;
    }
    printf("Instruction: 0x%08x\n", instruction);
    printf("sf: %u, literal: %u, L: %u, U: %u, offset: %u, simm9: %d, Xn: %u, Rt: %u\n",
           sf, literal, L, U, offset, simm9, Xn, Rt);

    if (literal) {
        // Handle literal load
        int32_t simm19 = (instruction >> 5) & 0x7FFFF; // Signed immediate (bits 5-23)
        int64_t offset = ((int64_t)simm19 << 45) >> 45; // Sign-extend the 19-bit immediate
        address = cpu->pc + (offset * 4);
        printf("Literal load: simm19: %d, offset: %ld, address: 0x%lx\n", simm19, offset, address);
        if (sf == 0) { // 32-bit load
            data = *(uint32_t *)(byte_memory + address);
            cpu->regs[Rt] = data;
            printf("32-bit LOAD: X%d = [0x%lx] (data: 0x%x)\n", Rt, address, (uint32_t)data);
        } else { // 64-bit load
            data = *(uint64_t *)(byte_memory + address);
            cpu->regs[Rt] = data;
            printf("64-bit LOAD: X%d = [0x%lx] (data: %lu)\n", Rt, address, data);
        }
    } else {
        // Handle non-literal load/store
        address = cpu->regs[Xn];
        printf("Non-literal load/store: initial address: 0x%lx\n", address);
        if (U) { // Unsigned Offset
            if (sf == 0) { // 32-bit variant
                address += (offset * 4);
            } else { // 64-bit variant
                address += (offset * 8);
            }
            printf("Unsigned Offset: new address: 0x%lx\n", address);
        } else {
            if (R) { // Register Offset
                uint32_t Xm = (instruction >> 16) & 0x1F; // Offset register
                address += cpu->regs[Xm];
                printf("Register Offset: Xm: %u, new address: 0x%lx\n", Xm, address);
            } else {
                if (I) { // Pre-Indexed
                    address += simm9;
                    cpu->regs[Xn] = address; // Write-back the updated address to the base register
                    printf("Pre-Indexed: new address: 0x%lx, updated base register X%d: 0x%lx\n", address, Xn, cpu->regs[Xn]);
                } else { // Post-Indexed
                    printf("Post-Indexed: address remains unchanged initially: 0x%lx\n", address);
                }
            }
        }
        if (L) { // Load
            if (Rt == 31) { return; }       // if Rt is ZR register, abort
            if (sf == 0) { // 32-bit load
                data = *(uint32_t *)(byte_memory + address);
                cpu->regs[Rt] = data;
                printf("32-bit LOAD: X%d = [0x%lx] (data: 0x%x)\n", Rt, address, (uint32_t)data);
            } else { // 64-bit load
                data = *(uint64_t *)(byte_memory + address);
                cpu->regs[Rt] = data;
                printf("64-bit LOAD: X%d = [0x%lx] (data: %lu)\n", Rt, address, data);
            }
        } else { // Store
            if (sf == 0) { // 32-bit store
                data = cpu->regs[Rt] & 0xFFFFFFFF;
                *(uint32_t *)(byte_memory + address) = data;
                printf("32-bit STORE: [0x%lx] = X%d (data: 0x%x)\n", address, Rt, (uint32_t)data);
            } else { // 64-bit store
                data = cpu->regs[Rt];
                *(uint64_t *)(byte_memory + address) = data;
                printf("64-bit STORE: [0x%lx] = X%d (data: %lu)\n", address, Rt, data);
            }
        }
        // Handle post-index addressing mode
        if (!literal && !U && !R && !I) {
            cpu->regs[Xn] += simm9; // Update base register with signed offset
            printf("Post-Indexed Update: new base register X%d: 0x%lx\n", Xn, cpu->regs[Xn]);
        }
    }
}

void branch_instruction(CPUState *cpu, uint32_t instruction) {
    uint32_t op = (instruction >> 26) & 0x3F; // Bits 31-26
    int32_t simm26, simm19;
    int64_t offset;

    printf("Branch instruction: PC=0x%lx, instruction=0x%08x, op=0x%x\n", cpu->pc, instruction, op);

    switch (op) {
        case 0x05: // Unconditional branch
            simm26 = (instruction & 0x3FFFFFF); // Bits 25-0
            offset = (int64_t)simm26 << 2;
            printf("Unconditional branch: offset=0x%lx, PC before=0x%lx\n", offset, cpu->pc);
            cpu->pc += offset - 4;
            printf("Unconditional branch to PC=0x%lx\n", cpu->pc);
            break;
        case 0x35: // Register branch
            {
                uint32_t Xn = (instruction >> 5) & 0x1F; // Bits 9-5
                cpu->pc = cpu->regs[Xn];
                printf("Register branch to PC=0x%lx\n", cpu->pc);
            }
            break;
        case 0x15: // Conditional branch
            simm19 = (instruction >> 5) & 0x7FFFF; // Bits 23-5
            uint32_t cond = instruction & 0xF;     // Bits 3-0
            offset = (((int64_t)simm19 << 45) >> 45) << 2;
            if (check_condition(cpu, cond)) {
                printf("Condition met for branch: cond=0x%x, offset=0x%lx\n", cond, offset);
                cpu->pc += offset - 4;
                printf("Conditional branch to PC=0x%lx on condition %x\n", cpu->pc, cond);
                return;
            } else {
                printf("Condition %x not met, no branch taken\n", cond);
            }
            break;
        default:
            printf("Unknown branch instruction: 0x%08x\n", instruction);
            break;
    }
}

void decode_and_execute(CPUState *cpu, uint32_t *memory, uint32_t instruction) {
    printf("\nDecoding instruction at PC=0x%lx: 0x%08x\n", cpu->pc, instruction);
    if (instruction == HALT) {
        printf("HALT instruction executed at PC=0x%lx\n", cpu->pc);
        return;
    }

    uint32_t op0 = (instruction >> 25) & 0xF; // Bits 28-25
    printf("Instruction group: op0=0x%x\n", op0);
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
        case 0xC:
            single_data_transfer(cpu, instruction);
            break;
        case 0xA: // Branches
        case 0xB:
        printf("Branch instruction: 0x%08x\n", instruction);
            branch_instruction(cpu, instruction);  
            break;
        default:
            printf("Unknown instrucstion group: op0=0x%x\n", op0);
            break;
    }
}

void emulate(CPUState *cpu, uint32_t *memory, size_t size) {
    while (cpu->pc < size * 4) {
        uint32_t instruction = memory[cpu->pc / 4];
        decode_and_execute(cpu, memory, instruction);
        cpu->pc += 4; // Increment PC by 4 (size of an instruction)
        if (instruction == HALT) break;
    }
}

void output_state(CPUState *cpu, uint32_t *memory, size_t size) {
    char pstate_str[5];
    format_pstate(cpu->pstate, pstate_str);
    printf("Registers:\n");
    for (int i = 0; i < 31; i++) {
        printf("X%02d = %016lx\n", i, cpu->regs[i]);
    }
    printf("PC = %016lx\n\n", cpu->pc-4);
    printf("PSTATE : %s\n", pstate_str);
    printf("Non-Zero Memory:\n");
    for (size_t i = 0; i < (size + MEMORY_SIZE / sizeof(uint32_t)); i++) {
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