#include <stdint.h>
#include <stdio.h>
#include <string.h>
//this is noob programming
enum
{
	R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC,
    R_COND,
    R_COUNT
};

enum
{
	OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP,   /* execute trap */
    OP_COUNT,  /* count opcodes */
    OP_OK
};

enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

#define MEMORY_MAX (1 << 16) 


uint16_t memory[MEMORY_MAX];

void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}


uint16_t mem_read(uint16_t address)
{
    return memory[address];
}

uint16_t reg[R_COUNT];


uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}


void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}


void add(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if(imm_flag)
    {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] + imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] + reg[r2];
    }

    update_flags(r0);
}


void ldi(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t offset = sign_extend(instr & 0x1FF, 9);

    reg[r0] = mem_read(reg[R_PC] + offset);
	printf("%d\n", reg[r0]);

    update_flags(r0);
}


enum
{
	TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
	TRAP_OUT = 0x21,   /* output a character */
	TRAP_PUTS = 0x22,  /* output a word string */
	TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
	TRAP_PUTSP = 0x24, /* output a byte string */
	TRAP_HALT = 0x25,  /* halt the program */
	TRAP_COUNT = 0x6
};

void trap_puts(uint16_t instr)
{
	puts("Hello world");
}


void trap_getc(uint16_t instr)
{
}


typedef struct
{
	char *name;
	void (*trap_handler)(uint16_t);
} Trap_Def;


Trap_Def trap_loopup_table[TRAP_COUNT] = {
	[TRAP_GETC % 0x20] = {.name = "getc", .trap_handler = trap_getc},
	[TRAP_PUTS % 0x20] = {.name = "puts", .trap_handler = trap_puts}
};


typedef struct 
{
    char *name;
    void (*function)(uint16_t);
} Instr_Def;


// void (*instr_lokup_table[OP_COUNT])(uint16_t) = {
//     [OP_ADD] = add,
//     [OP_LDI] = ldi
// };

Instr_Def instr_lokup_table[OP_COUNT] = 
{
    [OP_ADD] = {.name = "add", .function = add},
    [OP_LDI] = {.name = "ldi", .function = ldi}
};


int main(void)
{
    reg[R_COND] = FL_ZRO;

    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    reg[R_R0] = 4;
    memory[PC_START] = (OP_ADD << 12) | (R_R0 << 9) | (R_R0 << 6) | (1 << 5) | 3; // add instruction

    uint8_t no_of_instr = 1;
    uint8_t i = 0;

    while(i < no_of_instr)
    {
        uint16_t instr = memory[reg[R_PC]++];
        uint16_t op = instr >> 12;

        if(op < OP_COUNT)
        {

            printf("Name: %s | Opcode: %d\n", instr_lokup_table[op].name, op);
            instr_lokup_table[op].function(instr);
        }
        else
        {
            printf("No instruction found with opcode %d\n", op);
        }
        i++;
    }

    printf("4 + 3 = %d\n", reg[R_R0]);
    return 0;
}
