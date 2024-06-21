#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assemble.h"

Label symbolTable[MAX_LABELS];
int labelCount = 0;

Instruction instructions[MAX_LABELS];
int instructionCount = 0;

// Function to add label to the symbol table
void addLabel(char *label, int address) {
    strcpy(symbolTable[labelCount].label, label);
    symbolTable[labelCount].address = address;
    labelCount++;
}

int labelExists(const char *label) {
    for (int i = 0; i < labelCount; i++) {
        if (strcmp(symbolTable[i].label, label) == 0) {
            return 1; // Label exists
        }
    }
    return 0; // Label does not exist
}

// Function to get the address of a label
int getLabelAddress(char *label) {
    for (int i = 0; i < labelCount; i++) {
        if (strcmp(symbolTable[i].label, label) == 0) {
            printf("Found label: %s at address: %d\n", label, symbolTable[i].address);
            return symbolTable[i].address;
        }
    }
    printf("Label not found: %s\n", label);
    return -1; // Label not found
}

// Function to parse and add instruction
void addInstruction(char *line, int address) {
    strcpy(instructions[instructionCount].instruction, line);
    instructions[instructionCount].address = address;
    instructionCount++;
}

int parseOperand(char *operand, int *sf) {
    if (operand == NULL) {
        return 0;
    }
    if (strcmp(operand, "xzr") == 0 || strcmp(operand, "wzr") == 0) {
        return 31;
    }
    if (strncmp(operand, "0x",2) == 0) {
        return strtol(operand, NULL, 16);
    }
    if (operand[0] == '#') {
        return strtoul(&operand[1], NULL, 0);
    } else if (operand[0] == 'x') {
        if (sf != NULL) {
            *sf = 1;
        }
        return atoi(&operand[1]);
    } else if (operand[0] == 'w') {
        return atoi(&operand[1]);
    } else {
        return strtoul(&operand[0], NULL, 0);
    }
}

int arithmeticInstructions(char *mnemonic, char *rd, char *rn, char *operand, char *remainder) {
    printf("Encoding data processing instruction: %s %s %s %s %s\n", mnemonic, rd, rn, operand, remainder);
    int instruction = 0;
    int sf = 0;  // Default to 64-bit
    int opc = 0;
    int shiftAmount = 0;
    int imm12 = 0;
    int rm = 0;
    int Rn = parseOperand(rn, &sf);
    int Rd = parseOperand(rd, &sf);
    int sh = 0;
    int opVal = parseOperand(operand, &sf);
    
    if (strcmp(mnemonic, "cmp") == 0) {
        mnemonic = "subs";
        char *zr = "xzr";
        if (remainder != NULL) {
            char input[30];
            sprintf(input, "%s %s", operand, remainder);
            return arithmeticInstructions(mnemonic, zr, rd, rn, input);
        } else {
            return arithmeticInstructions(mnemonic, zr, rd, rn, NULL);
        }
    } else if (strcmp(mnemonic, "cmn") == 0) {
        mnemonic = "adds";
        char *zr = "xzr";
        if (remainder != NULL) {
            char input[30];
            sprintf(input, "%s %s", operand, remainder);
            return arithmeticInstructions(mnemonic, zr, rd, rn, input);
        } else {
            return arithmeticInstructions(mnemonic, zr, rd, rn, NULL);
        }
    } else if (strcmp(mnemonic, "neg") == 0) {
        mnemonic = "sub";
        char *zr = "xzr";
        return arithmeticInstructions(mnemonic, rd, zr, rn, NULL);
    } else if (strcmp(mnemonic, "negs") == 0) {
        mnemonic = "subs";
        char *zr = "xzr";
        return arithmeticInstructions(mnemonic, rd, zr, rn, NULL);
    }
    
    if (strchr(operand, '#') != NULL) {
        
        if (remainder != NULL) {
            strtok(remainder, " \t\n");
            char *shiftVal = strtok(NULL, " \t\n");
            shiftAmount = parseOperand(shiftVal, NULL);
            if (shiftAmount != 0) {
                sh = 1;
            }
        } 
        imm12 = opVal;

        if (strcmp(mnemonic, "add") == 0) {
            opc = 0b000;
        } else if (strcmp(mnemonic, "adds") == 0) {
            opc = 0b001;
        } else if (strcmp(mnemonic, "sub") == 0) {
            opc = 0b010;
        } else if (strcmp(mnemonic, "subs") == 0) {
            opc = 0b011;
        }

        instruction = (sf << 31) | (opc << 29) | (0b100 << 26) | (0b010 << 23) | (sh << 22) | ((imm12 & 0xFFF) << 10) | (Rn << 5) | Rd;
    } else {
        rm = parseOperand(operand, &sf);
        int shiftCode = 8;
        if (remainder != NULL) {
            char *shiftType = strtok(remainder, " \t\n");
            char *shiftVal = strtok(NULL, " \t\n");
            shiftAmount = parseOperand(shiftVal, NULL);
            if (strcmp(shiftType, "lsl") == 0) {
                shiftCode = 0b1000;
            } else if (strcmp(shiftType, "lsr") == 0) {
                shiftCode = 0b1010;
            } else if (strcmp(shiftType, "asr") == 0) {
                shiftCode = 0b1100;
            } else if (strcmp(shiftType, "ror") == 0) {
                shiftCode = 0b1110; //MAY BE INVALID HERE
            } else {
                exit(EXIT_FAILURE);
            }
        }
        if (strcmp(mnemonic, "add") == 0) {
            opc = 0b000;
        } else if (strcmp(mnemonic, "adds") == 0) {
            opc = 0b001;
        } else if (strcmp(mnemonic, "sub") == 0) {
            opc = 0b010;
        } else if (strcmp(mnemonic, "subs") == 0) {
            opc = 0b011;
        }
        instruction = (sf << 31) | (opc << 29) | (0 << 28) | (0b101 << 25) | (shiftCode << 21) | (rm << 16) | (shiftAmount << 10) | (Rn << 5) | Rd;
    }

    printf("Encoded instruction: 0x%X\n", instruction);
    return instruction;
}

// Function to encode a single data transfer instruction
int logicalInstructions(char *mnemonic, char *rd, char *rn, char *operand, char *remainder) {
    printf("Encoding logical instruction: %s %s %s %s %s\n", mnemonic, rd, rn, operand, remainder);
    
    if ((strcmp(mnemonic, "and") == 0)
    && (strcmp(rd, "x0") == 0)
    && (strcmp(rn, "x0") == 0)
    && (strcmp(operand, "x0") == 0)) {
        return 0x8A000000;
    } else if (strcmp(mnemonic, "tst") == 0) {
        mnemonic = "ands";
        char *zr = "w31";
            if (remainder != NULL) {
            char input[30];
            sprintf(input, "%s %s", operand, remainder);
            return logicalInstructions(mnemonic, zr, rd, rn, input);
        } else {
            return logicalInstructions(mnemonic, zr, rd, rn, NULL);
        }
    }

    int instruction = 0;
    int sf = 0;
    int opc = 0;
    int shiftAmount = 0;
    int Rn = parseOperand(rn, &sf);
    int Rd = parseOperand(rd, &sf);
    int shiftCode = 0;
    int N = 0;

    if (strcmp(mnemonic, "and") == 0) {
            opc = 0b00;
        } else if (strcmp(mnemonic, "bic") == 0) {
            opc = 0b00;
            N = 1;
        } else if (strcmp(mnemonic, "orr") == 0) {
            opc = 0b01;
        } else if (strcmp(mnemonic, "orn") == 0) {
            opc = 0b01;
            N = 1;
        } else if (strcmp(mnemonic, "eor") == 0) {
            opc = 0b10;
        } else if (strcmp(mnemonic, "eon") == 0) {
            opc = 0b10;
            N = 1;
        } else if (strcmp(mnemonic, "ands") == 0) {
            opc = 0b11;
        } else if (strcmp(mnemonic, "bics") == 0) {
            opc = 0b11;
            N = 1;
    }
    
    // Check if operand is an immediate value
    if (strchr(operand, '#') != NULL) {
        int imm = parseOperand(operand, NULL);
        if (remainder != NULL) {
            strtok(remainder, " \t\n");
            char *shiftVal = strtok(NULL, " \t\n");
            shiftAmount = parseOperand(shiftVal, NULL);
            if (shiftAmount != 0) {
            }
        } 
        instruction = (sf << 31) | (opc << 29) | (0b100 << 26) | (shiftAmount << 22) | ((imm & 0xFFF) << 10) | (Rn << 5) | Rd;
    } else {
        if (remainder != NULL) {
            char *shiftType = strtok(remainder, " \t\n");
            char *shiftVal = strtok(NULL, " \t\n");
            shiftAmount = parseOperand(shiftVal, NULL);
            if (strcmp(shiftType, "lsl") == 0) {
                shiftCode = 0b000;
            } else if (strcmp(shiftType, "lsr") == 0) {
                shiftCode = 0b001;
            } else if (strcmp(shiftType, "asr") == 0) {
                shiftCode = 0b010;
            } else if (strcmp(shiftType, "ror") == 0) {
                shiftCode = 0b011; //
            } else {
                exit(EXIT_FAILURE);
            }
        }
        int Rm = parseOperand(operand, &sf);
        instruction = (sf << 31) | (opc << 29) | (0 << 28) | (0b101 << 25) | (shiftCode << 22) | (N << 21) | (Rm << 16) | (shiftAmount << 10) | (Rn << 5) | Rd;
    }

    printf("Encoded instruction: 0x%X\n", instruction);
    return instruction;
}

int multiplicationInstructions(char *mnemonic, char *rd, char *rn, char *rm, char *ra) {
    printf("Encoding multiplication instruction: %s %s %s %s %s\n", mnemonic, rd, rn, rm, ra);

    int instruction = 0;
    int sf = 0;  // Size flag (0 for 32-bit, 1 for 64-bit)
    int x = 0; // Operation code
    int Rn = parseOperand(rn, &sf);
    int Rd = parseOperand(rd, &sf);
    int Rm = parseOperand(rm, &sf); //
    int Ra = parseOperand(ra, &sf);

    //Determine the operation code for the mnemonic
    if (strcmp(mnemonic, "msub") == 0) {
        x = 1;
    } else if (strcmp(mnemonic, "mul") == 0) {
        Ra = 31;
    } else if (strcmp(mnemonic, "mneg") == 0) {
        mnemonic = "msub";
        char *zr = "w31";
        return multiplicationInstructions(mnemonic, rd, rn, rm, zr);
    }

    instruction = (sf << 31) | (0b0011011000 << 21) | (Rm << 16) | (x << 15) | (Ra << 10) | (Rn << 5) | Rd;

    return instruction;
}

int movInstructions(char *mnemonic, char *rd, char *operand, char *remainder) {
    printf("Encoding mov instruction: %s %s %s\n", mnemonic, rd, operand);

    int instruction = 0;
    int sf = 0;  // Size flag (0 for 32-bit, 1 for 64-bit)
    int opi = 0b101;
    int opc = 0b01; // Invalid opc as placeholder
    int Rd = parseOperand(rd, &sf);
    int imm16 = 0;
    int shiftAmount = 0;
    
    if (strcmp(mnemonic, "mov") == 0) {
            mnemonic = "orr";
            char *zr = "w31";
            return (logicalInstructions(mnemonic, rd, zr, operand, NULL));
        } else if (strcmp(mnemonic, "mvn") == 0) {
            mnemonic = "orn";
            char *zr = "xzr";
            return (logicalInstructions(mnemonic, rd, zr, operand, NULL));
        } else if (strcmp(mnemonic, "movn") == 0) {//
            opc = 0b00;
        } else if (strcmp(mnemonic, "movz") == 0) {//
            opc = 0b10;
        } else if (strcmp(mnemonic, "movk") == 0) {//
            opc = 0b11;
        }

    if (remainder != NULL) {
            strtok(remainder, " \t\n");
            char *shiftVal = strtok(NULL, " \t\n");
            shiftAmount = parseOperand(shiftVal, NULL);
            if (shiftAmount != 0) {
            }
            shiftAmount /= 16;
        } 
    imm16 = parseOperand(operand, NULL);
    instruction = (sf << 31) | (opc << 29) | (0b100 << 26) | (opi << 23) | (shiftAmount << 21) | (imm16 << 5) | Rd;

    printf("Encoded instruction: 0x%X\n", instruction);
    return instruction;
}
void parseAddressingMode(char *input, int *reg, int *offset, int *preIndex, int *sf, int registerOn) {
    // Remove the square brackets
    char *start = strchr(input, '[');
    char *end = strchr(input, ']');
    if (start && end) {
        *start = ' ';
        *end = ' ';
    }
    if (registerOn) {
            char *token = strtok(input, ", \t\n");
            if (token) {
                *reg = parseOperand(token, NULL);
            }
            token = strtok(NULL, " ,#");
            if (token) {
                *offset = parseOperand(token, NULL);
            }
    } else {
        char *exclamation = strchr(input, '!');
        if (exclamation == NULL) {
            *preIndex = 0;  // Post-indexing or offset
        } else {
            *preIndex = 1;
            *exclamation = ' ';
        }
        char *token = strtok(input, " ,#");
        if (token) {
            *reg = parseOperand(token, NULL);
        }

        token = strtok(NULL, " ,#");
        if (token != NULL) {
            *offset = parseOperand(token, NULL);
        } else {
            *offset = 0;
        }
    }
}

int singleDataTransfer(char *mnemonic, char *rt, char *rn, char *remainder, int lineNo) {
    printf("\nEncoding single data transfer instruction: %s %s %s %s\n", mnemonic, rt, rn, remainder ? remainder : "NULL");

    int instruction = 0;
    int sf = 0;  // Size flag (0 for 32-bit, 1 for 64-bit)
    int L = 0; // Load/store flag
    int offset = 0; // Immediate offset value
    int labeloffset = 0;
    int preIndex = 0;
    int Rn = -1;
    int U = 0;
    int Rt = parseOperand(rt, &sf);
    int neg = 0;
    int label = labelExists(rn);

    if (label) {
        labeloffset = getLabelAddress(rn);
        labeloffset -= (lineNo * 4);
        labeloffset /= 4;
        if (labeloffset < 0) {
            labeloffset *= -1;
            neg = 1;
        }
    }

    if (strcmp(mnemonic, "str") == 0) {
        L = 0b0; // STR operation code
        printf("Operation: STR\n");
    } else if (strcmp(mnemonic, "ldr") == 0) {
        L = 0b1; // LDR operation code
        printf("Operation: LDR\n");
    } else {
        printf("Unsupported mnemonic: %s\n", mnemonic);
        exit(EXIT_FAILURE);
    }

    if (rn[0] == '#' || label) { // Literal Load NEEDS TO BE CHANGED LATER TO COMPENSATE FOR LABELS
        if (label) {
            offset = labeloffset;
        } else {
            offset = parseOperand(rn, NULL); // Need negatives
        }
        if (neg) {
            instruction = (0 << 31) | (sf << 30) | (0b011000 << 24) | (1 << 23) | (((~offset + 1) & 0x7FFFF) << 5) | Rt;
        } else {
            instruction = (0 << 31) | (sf << 30) | (0b011000 << 24) | (offset << 5) | Rt;
        }
    } else if (remainder == NULL || (strchr(remainder, '#') != NULL && strchr(remainder, ']') != NULL && strchr(remainder, '!') == NULL)) { // Offset
        char input[20];
        sprintf(input, "%s, %s", rn, remainder);
        parseAddressingMode(input, &Rn, &offset, &preIndex, &sf, 0);
        if (sf) {
            offset /= 8;
        } else {
            offset /= 4;
        }
        U = 1;
        instruction = (1 << 31) | (sf << 30) | (0b11100 << 25) | (U << 24) | (0 << 23) | (L << 22) | (offset << 10) | (Rn << 5) | Rt;
    } else if (strchr(remainder, '!') != NULL) { // Pre-Index
        char *input = strcat(rn, remainder);
        parseAddressingMode(input, &Rn, &offset, &preIndex, &sf, 0);
        instruction = (1 << 31) | (sf << 30) | (0b11100 << 25) | (U << 24) | (0 << 23) | (L << 22) | (0 << 21) | (offset << 12) | (preIndex << 11) | (1 << 10) | (Rn << 5) | Rt;
    } else if (strchr(remainder, ']') == NULL) { // Post-Index
        parseAddressingMode(rn, &Rn, &offset, &preIndex, &sf, 1); // Offset should be 0
        char *off = strtok(remainder, " \t\n");
        offset = parseOperand(off, NULL);
        if (offset < 0) {
            offset *= -1;
            instruction = (1 << 31) | (sf << 30) | (0b11100 << 25) | (U << 24) | (0 << 23) | (L << 22) | (0 << 21) | (1 << 20) | (((~offset + 1) & 0xFF) << 12) | (preIndex << 11) | (1 << 10) | (Rn << 5) | Rt;
        } else {
            instruction = (1 << 31) | (sf << 30) | (0b11100 << 25) | (U << 24) | (0 << 23) | (L << 22) | (0 << 21) | (offset << 12) | (preIndex << 11) | (1 << 10) | (Rn << 5) | Rt;
            printf("Post-Index: Rn = %d, Offset = %d\n", Rn, offset);
        }
    } else { // Register
        char string[20];
        sprintf(string, "%s, %s", rn, remainder);
        parseAddressingMode(string, &Rn, &offset, &preIndex, &sf, 1); // Offset should be 0
        instruction = (1 << 31) | (sf << 30) | (0b11100 << 25) | (U << 24) | (0 << 23) | (L << 22) | (1 << 21) | (offset << 16) | (0b011010 << 10) | (Rn << 5) | Rt;
    }

    printf("Encoded instruction: 0x%X\n", instruction);
    return instruction;
}

// Function to encode a branch instruction
int encodeBranchInstruction(char *mnemonic, char *address, int lineNo) {
    int instruction = 0;
    int offset = 0;
    int neg = 0;
    int label = labelExists(address);

    if (label) {
        offset = getLabelAddress(address);
        offset -= (lineNo * 4);
        offset /= 4;
        if (offset < 0) {
            offset *= -1;
            neg = 1;
        }
    } else {
        offset = parseOperand(address, NULL);
    }

    if (strcmp(mnemonic, "b") == 0) { // Unconditional branch
        printf("Unconditional\n");
        if (neg == 0) {
            instruction = (0b000101 << 26) | offset;
        } else {
            instruction = (0b000101 << 26) | (1 << 25) | ((~offset + 1) & 0x1FFFFFF);
        }

    } else if (strcmp(mnemonic, "br") == 0) { // Register branch
        printf("Register\n");
        if (neg == 0) {
            instruction = (0b1101011000011111000000 << 10) | (offset << 5) | 0b0000;
        } else {
            instruction = (0b1101011000011111000000 << 10) | (1 << 9) | (((~offset + 1) & 0xF) << 5) | 0b0000;
        }
    } else { // Conditional branch
        printf("Conditional\n");
        char condition[10];
        char *dotPosition = strchr(mnemonic, '.');
        if (dotPosition != NULL) {
            strcpy(condition, dotPosition + 1);
        }
        int code = -1;
        if (strcmp(condition, "eq") == 0) {
            code = 0x0; // Equal (Z == 1)
        } else if (strcmp(condition, "ne") == 0) {
            code = 0x1; // Not equal (Z == 0)
        } else if (strcmp(condition, "ge") == 0) {
            code = 0xA; // Signed greater or equal (N == 1)
        } else if (strcmp(condition, "lt") == 0) {
            code = 0xB; // Signed less than (N != 1)
        } else if (strcmp(condition, "gt") == 0) {
            code = 0xC; // Signed greater than (Z == 0 && N == V)
        } else if (strcmp(condition, "le") == 0) {
            code = 0xD; // Signed less than or equal (!(Z == 0 && N == V))
        } else if (strcmp(condition, "al") == 0) {
            code = 0xE; // Always (any)
        } else {
            printf("Unknown condition: %s\n", condition);
        }
        
        if (neg == 0) {
            instruction = (0b01010100 << 24) | (offset << 5) | (0 << 4) | code;
        } else {
            instruction = (0b01010100 << 24) | (1 << 23) | (((~offset + 1) & 0x7FFFF) << 5) | (0 << 4) | code;
        }
    }

    printf("Encoding branch instruction: %s %s\n", mnemonic, address);
    return instruction;
}

// Function to encode a special directive
int encodeDirective(char *directive, char *value) {
    if (strcmp(directive, ".int") == 0) {
        return (int)strtol(value, NULL, 0);
    }
    return 0;
}

// Function to process each line and determine if it is a label, instruction, or directive
int processLine(char *line, int address) {
    char *token = strtok(line, " \t\n");
    if (token == NULL) return 1;

    if (strchr(token, ':') != NULL) {
        // It's a label
        token[strlen(token) - 1] = '\0'; // Remove the colon
        addLabel(token, address);
        return 1;
    } else {
        // It's an instruction or directive
        addInstruction(line, address);
        return 0;
    }
}

// Function to parse the assembly file and perform the two-pass assembly
void assemble(char *inputFileName, char *outputFileName) {
    FILE *inputFile = fopen(inputFileName, "r");
    if (inputFile == NULL) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    int address = 0;

    // First pass: Create symbol table
    while (fgets(line, sizeof(line), inputFile)) {
        int blank = processLine(line, address);
        if (blank != 1) {
            address += 4; // Each instruction is 4 bytes
        }
    }
    fclose(inputFile);

    inputFile = fopen(inputFileName, "r");
    if (inputFile == NULL) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }

    FILE *outputFile = fopen(outputFileName, "wb");
    if (outputFile == NULL) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    int lineNo = 0;
    // Second pass: Encode instructions
    while (fgets(line, sizeof(line), inputFile)) {
        char *token = strtok(line, " \t\n");
        if (token == NULL) continue;

        if (strchr(token, ':') != NULL) {
            // It's a label, skip it
            continue;
        }
        int binaryInstruction = 0;
        char *mnemonic = token;
        char *rd = strtok(NULL, ", \t\n");
        char *rn = strtok(NULL, ", \t\n");
        
        if (
            strncmp(mnemonic, "add", 3) == 0 || 
            strncmp(mnemonic, "sub", 3) == 0 || 
            strncmp(mnemonic, "cm", 2) == 0  || 
            strncmp(mnemonic, "neg", 3) == 0
        ) {
            char *operand = strtok(NULL, ", \t\n");
            char *remainder = strtok(NULL, "");
            binaryInstruction = arithmeticInstructions(mnemonic, rd, rn, operand, remainder);
        } 
        else if (
            strcmp(mnemonic, "madd") == 0 || 
            strcmp(mnemonic, "msub") == 0 || 
            strcmp(mnemonic, "mul") == 0  || 
            strcmp(mnemonic, "mneg") == 0
            ) {
            char *operand = strtok(NULL, ", \t\n");
            char *rm = strtok(NULL, " \t\n");
            binaryInstruction = multiplicationInstructions(mnemonic, rd, rn, operand, rm);
        } 
        else if (
            strncmp(mnemonic, "mov",3) == 0 || 
            strcmp(mnemonic, "mvn") == 0
            ) {
            char *remainder = strtok(NULL, "");
            binaryInstruction = movInstructions(mnemonic, rd, rn, remainder);
        }
        else if (
            strcmp(mnemonic, "ldr") == 0 || 
            strcmp(mnemonic, "str") == 0
        ) {
            char *remainder = strtok(NULL, "");
            binaryInstruction = singleDataTransfer(mnemonic, rd, rn, remainder, lineNo);
        } 
        else if (
            strcmp(mnemonic, "b") == 0  ||
            strcmp(mnemonic, "br") == 0 ||
            strncmp(mnemonic, "b.", 2) == 0
            ) {
            binaryInstruction = encodeBranchInstruction(mnemonic, rd, lineNo);
        } 
        else if (
            strncmp(mnemonic, ".int", 3) == 0
            ) {
            binaryInstruction = encodeDirective(mnemonic, rd);
        } 
        else if (
            strncmp(mnemonic, "and", 3) == 0 || 
            strncmp(mnemonic, "bic", 3) == 0 || 
            strncmp(mnemonic, "or", 2) == 0  || 
            strncmp(mnemonic, "eo", 2) == 0  ||
            strncmp(mnemonic, "tst", 3) == 0
            ) {
            char *operand = strtok(NULL, ", \t\n");
            char *remainder = strtok(NULL, "");
            binaryInstruction = logicalInstructions(mnemonic, rd, rn, operand, remainder);
        }
        printf("Writing binary instruction: 0x%X\n", binaryInstruction);
        fwrite(&binaryInstruction, sizeof(int), 1, outputFile);
        lineNo++;
    }

    fclose(inputFile);
    fclose(outputFile);
}
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    assemble(argv[1], argv[2]);
    return 0;
}