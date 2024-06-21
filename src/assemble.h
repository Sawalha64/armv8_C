#define MAX_LABELS 1024
#define MAX_LINE_LENGTH 256
#define MEMORY_OFFSET 0

typedef struct {
    char label[MAX_LINE_LENGTH];
    int address;
} Label;

typedef struct {
    char instruction[MAX_LINE_LENGTH];
    int address;
} Instruction;

void addLabel(char *label, int address);
int labelExists(const char *label);
int getLabelAddress(char *label);
void addInstruction(char *line, int address);
int parseOperand(char *operand, int *sf);
int arithmeticInstructions(char *mnemonic, char *rd, char *rn, char *operand, char *remainder);
int logicalInstructions(char *mnemonic, char *rd, char *rn, char *operand, char *remainder);
int multiplicationInstructions(char *mnemonic, char *rd, char *rn, char *rm, char *ra);
int movInstructions(char *mnemonic, char *rd, char *operand, char *remainder);
void parseAddressingMode(char *input, int *reg, int *offset, int *preIndex, int *sf, int registerOn);
int singleDataTransfer(char *mnemonic, char *rt, char *rn, char *remainder, int lineNo);
int encodeBranchInstruction(char *mnemonic, char *address, int lineNo);
int encodeDirective(char *directive, char *value);
int processLine(char *line, int address);
void assemble(char *inputFileName, char *outputFileName);