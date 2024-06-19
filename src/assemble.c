#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LABELS 1024
#define MAX_LINE_LENGTH 256

typedef struct {
    char label[MAX_LINE_LENGTH];
    int address;
} Label;

Label symbolTable[MAX_LABELS];
int labelCount = 0;

typedef struct {
    char instruction[MAX_LINE_LENGTH];
    int address;
} Instruction;

Instruction instructions[MAX_LABELS];
int instructionCount = 0;

// Function to add label to the symbol table
void addLabel(char *label, int address) {
    printf("Adding label: %s at address: %d\n", label, address);
    strcpy(symbolTable[labelCount].label, label);
    symbolTable[labelCount].address = address;
    labelCount++;
}

// Function to parse and add instruction
void addInstruction(char *line, int address) {
    printf("Adding instruction: %s at address: %d\n", line, address);
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
        printf("Parsing immediate value: %s\n", operand);
        return strtoul(&operand[1], NULL, 0);
    } else if (operand[0] == 'x') {
        printf("Parsing register: %s\n", operand);
        if (sf != NULL) {
            *sf = 1;
        }
        return atoi(&operand[1]);
    } else if (operand[0] == 'w') {
        printf("Parsing register: %s\n", operand);
        return atoi(&operand[1]);
    } else {
        printf("Parsing immediate value: %s\n", operand);
        return strtoul(&operand[0], NULL, 0);
    }
}

int arithmeticInstructions(char *mnemonic, char *rd, char *rn, char *operand, char *remainder) {
    printf("Encoding data processing instruction: %s %s %s %s %s\n", mnemonic, rd, rn, operand, remainder);
    int instruction = 0;
    int sf = 0;  // Default to 64-bit
    int opc = 0;
    int opi = 0;
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
            printf("operand: %s, remainder: %s\n", operand, remainder);
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
            printf("operand: %s, remainder: %s\n", operand, remainder);
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
        printf("Operand is an immediate value: %s\n", operand);
        
        if (remainder != NULL) {
            char *shiftType = strtok(remainder, " \t\n");
            char *shiftVal = strtok(NULL, " \t\n");
            printf("Remainder: %s\n", remainder);
            shiftAmount = parseOperand(shiftVal, NULL);
            if (shiftAmount != 0) {
                printf("how tf we here\n");
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

        printf("Instruction encoding fields: sf=%d opc=%d sh=%d imm12=%d Rn=%d Rd=%d shiftAmount=%d\n", sf, opc, sh, imm12, Rn, Rd, shiftAmount);
        instruction = (sf << 31) | (opc << 29) | (0b100 << 26) | (0b010 << 23) | (sh << 22) | ((imm12 & 0xFFF) << 10) | (Rn << 5) | Rd;
    } else {
        printf("Operand is a register: %s\n", operand);
        rm = parseOperand(operand, &sf);
        int shiftCode = 8;
        if (remainder != NULL) {
            printf("Processing remainder: %s\n", remainder);
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
        printf("Instruction encoding fields: sf=%d opc=%d rm=%d shiftAmount=%d Rn=%d Rd=%d\n", sf, opc, rm, shiftAmount, Rn, Rd);
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
            printf("operand: %s, remainder: %s\n", operand, remainder);
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
    int imm12 = 0;
    int opVal = parseOperand(operand, &sf);
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
        printf("Operand is an immediate value: %s\n", operand);
        int imm = parseOperand(operand, NULL);
        if (remainder != NULL) {
            char *shiftType = strtok(remainder, " \t\n");
            char *shiftVal = strtok(NULL, " \t\n");
            printf("Remainder: %s\n", remainder);
            shiftAmount = parseOperand(shiftVal, NULL);
            if (shiftAmount != 0) {
                printf("how tf we here\n");
            }
        } 
        imm12 = opVal;
        // NOT CURRENTLY WORKING
        instruction = (sf << 31) | (opc << 29) | (0b100 << 26) | (shiftAmount << 22) | ((imm & 0xFFF) << 10) | (Rn << 5) | Rd;
    } else {
        if (remainder != NULL) {
            printf("Processing remainder: %s\n", remainder);
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
        printf("Operand is a register: %s\n", operand);
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

    printf("Encoded instruction???????: 0x%X\n", Rm);
    return instruction;
}

int movInstructions(char *mnemonic, char *rd, char *operand, char *remainder) {
    printf("Encoding mov instruction: %s %s %s\n", mnemonic, rd, operand);

    int instruction = 0;
    int sf = 0;  // Size flag (0 for 32-bit, 1 for 64-bit)
    int opi = 0b101;
    int opc = 0b01; // Invalid opc as placeholder
    int Rn = 0;  // Source register (for register moves)
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
            char *shiftType = strtok(remainder, " \t\n");
            char *shiftVal = strtok(NULL, " \t\n");
            printf("Remainder: %s\n", remainder);
            shiftAmount = parseOperand(shiftVal, NULL);
            if (shiftAmount != 0) {
                printf("shift > 0\n");
            }
            shiftAmount /= 16;
        } 
    imm16 = parseOperand(operand, NULL);
    printf("sf: %d, opc: %d, opi: %d, shiftam: %d, imm16: %d, Rd: %d", sf, opc, opi, shiftAmount, imm16, Rd);
    instruction = (sf << 31) | (opc << 29) | (0b100 << 26) | (opi << 23) | (shiftAmount << 21) | (imm16 << 5) | Rd;

    printf("Encoded instruction: 0x%X\n", instruction);
    return instruction;
}

// Function to process each line and determine if it is a label, instruction, or directive
void processLine(char *line, int address) {
    char *token = strtok(line, " \t\n");
    if (token == NULL) return;

    if (strchr(token, ':') != NULL) {
        // It's a label
        token[strlen(token) - 1] = '\0'; // Remove the colon
        addLabel(token, address);
    } else {
        // It's an instruction or directive
        addInstruction(line, address);
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
        processLine(line, address);
        address += 4; // Each instruction is 4 bytes
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

    // Second pass: Encode instructions
    while (fgets(line, sizeof(line), inputFile)) {
        char *token = strtok(line, " \t\n");
        if (token == NULL) continue;

        if (strchr(token, ':') != NULL) {
            // It's a label, skip it
            continue;
        }
        char *mnemonic = token;
        int binaryInstruction = 0;
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
            // TODO
        } 
        else if (
            strcmp(mnemonic, "b") == 0
            ) {
            // TODO
        } 
        else if (
            mnemonic[0] == '.'
            ) {
            // TODO
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
            printf("%s", line);
            binaryInstruction = logicalInstructions(mnemonic, rd, rn, operand, remainder);
        }

        printf("Writing binary instruction: 0x%X\n", binaryInstruction);
        fwrite(&binaryInstruction, sizeof(int), 1, outputFile);
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
