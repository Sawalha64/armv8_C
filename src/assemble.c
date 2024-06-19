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

unsigned int parseOperand(char *operand, unsigned int *sf) {
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
        printf("parse ELSE case: %s\n", operand);
        return strtol(operand, NULL, 10);
    }
}

unsigned int arithmeticInstructions(char *mnemonic, char *rd, char *rn, char *operand, char *remainder) {
    printf("Encoding data processing instruction: %s %s %s %s %s\n", mnemonic, rd, rn, operand, remainder);
    unsigned int instruction = 0;
    unsigned int sf = 0;  // Default to 64-bit
    unsigned int opc = 0;
    unsigned int opi = 0;
    unsigned int shiftAmount = 0;
    unsigned int imm12 = 0;
    unsigned int rm = 0;
    unsigned int Rn = parseOperand(rn, &sf);
    unsigned int Rd = parseOperand(rd, &sf);
    unsigned int sh = 0;
    unsigned int opVal = parseOperand(operand, &sf);
    
    if (strcmp(mnemonic, "cmp") == 0) {
        mnemonic = "subs";
        char *zr = "xzr";
        return arithmeticInstructions(mnemonic, zr, rd, rn, NULL);
    } else if (strcmp(mnemonic, "cmn") == 0) {
        mnemonic = "adds";
        char *zr = "xzr";
        return arithmeticInstructions(mnemonic, zr, rd, rn, NULL);
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
        unsigned int shiftCode = 8;
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
unsigned int logicalInstructions(char *mnemonic, char *rd, char *rn, char *operand, char *remainder) {
    printf("Encoding logical instruction: %s %s %s %s %s\n", mnemonic, rd, rn, operand, remainder);
    unsigned int instruction = 0;
    unsigned int sf = 0;
    unsigned int opc = 0;
    unsigned int shiftAmount = 0;
    unsigned int imm12 = 0;
    unsigned int opVal = parseOperand(operand, &sf);
    unsigned int Rn = parseOperand(rn, &sf);
    unsigned int Rd = parseOperand(rd, &sf);
    unsigned int shiftCode = 0;
    unsigned int N = 0;
    if ((strcmp(mnemonic, "and") == 0)
    && (strcmp(rd, "x0") == 0)
    && (strcmp(rn, "x0") == 0)
    && (strcmp(operand, "x0") == 0)) {
        return 0x8A000000;
    } else if (strcmp(mnemonic, "tst") == 0) {
        mnemonic = "ands";
        char *zr = "w31";
        return logicalInstructions(mnemonic, zr, rd, rn, NULL);
    }
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
        unsigned int imm = parseOperand(operand, NULL);
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
        unsigned int Rm = parseOperand(operand, &sf);
        instruction = (sf << 31) | (opc << 29) | (0 << 28) | (0b101 << 25) | (shiftCode << 22) | (N << 21) | (Rm << 16) | (shiftAmount << 10) | (Rn << 5) | Rd;
    }

    printf("Encoded instruction: 0x%X\n", instruction);
    return instruction;
}

// Function to process each line and determine if it is a label, instruction, or directive, NEEDS FIXING
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
        unsigned int binaryInstruction = 0;
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
            // TODO
        } 
        else if (
            strncmp(mnemonic, "mov",3) == 0 || 
            strcmp(mnemonic, "mvn") == 0
            ) {
            // TODO
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
        fwrite(&binaryInstruction, sizeof(unsigned int), 1, outputFile);
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
