#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

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
    OP_HALT,   /* halt the program */
    OP_COUNT,  /* count opcodes */
};

enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

#define MEMORY_MAX (1 << 16) 


uint16_t memory[MEMORY_MAX];

uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}


uint16_t check_key()
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}


void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}


uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
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


void trap_puts()
{
	uint16_t *str = memory + reg[R_R0];
	while(*str)
	{
		putc((char) *str, stdout);
		str++;
	}
	fflush(stdout);
}


void trap_putsp()
{
    uint16_t *str = memory + reg[R_R0];
    while(*str)
    {
        char char1 = (*str) & 0xFF;
        putc(char1, stdout);

        char char2 = (*str) >> 8;
        if(char2)
            putc(char2, stdout);

        str++;
    }
    fflush(stdout);
}


void trap_getc()
{
	reg[R_R0] = (uint16_t) getchar();
	update_flags(R_R0);
}


void trap_out()
{
	putc((char) reg[R_R0], stdout);
    fflush(stdout);
}


typedef struct
{
	char *name;
	void (*trap_handler)();
} Trap_Def;


Trap_Def trap_lookup_table[TRAP_COUNT] = {
	[TRAP_GETC % 0x20] = {.name = "getc", .trap_handler = trap_getc},
	[TRAP_PUTS % 0x20] = {.name = "puts", .trap_handler = trap_puts},
	[TRAP_OUT % 0x20] = {.name = "out", .trap_handler = trap_out},

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

Instr_Def instr_lookup_table[OP_COUNT] = 
{
    [OP_ADD] = {.name = "add", .function = add},
    [OP_LDI] = {.name = "ldi", .function = ldi}
};


int main(void)
{
    reg[R_COND] = FL_ZRO;

    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

	memory[0x4000] = 'H';
	memory[0x4001] = 'e';
	memory[0x4002] = 'l';
	memory[0x4003] = 'l';
	memory[0x4004] = 'o';
	memory[0x4005] = ' ';
	memory[0x4006] = 'w';
	memory[0x4007] = 'o';
	memory[0x4008] = 'r';
	memory[0x4009] = 'l';
	memory[0x400a] = 'd';
	memory[0x400b] = '\n';
	memory[0x400c] = '\0';

	reg[R_R0] = 0x4000;

	memory[PC_START] = (0xF0 << 8) | TRAP_PUTS;
	// memory[PC_START + 1] = (0xF0 << 8) | TRAP_OUT;
	// memory[PC_START + 1] = (OP_ADD << 12) | (R_R0 << 9) | (R_R0 << 6) | (1 << 5) | 3; // add instruction
    memory[PC_START + 1] = OP_HALT;

    uint8_t running = 1;
    while(running)
    {
        puts("Going round and round");
        uint16_t instr = memory[reg[R_PC]++];
        uint16_t op = instr >> 12;

        if(op < OP_COUNT)
        {
			if((instr & 0xFF00) == 0xF000)
			{
				uint16_t trap_code = (instr & 0xFF);
            	printf("Trap: %s | Trapcode: %d\n", trap_lookup_table[trap_code % 0x20].name, op);
				trap_lookup_table[trap_code % 0x20].trap_handler();
			}
            else if(instr == OP_HALT)
            {
                puts("HALT");
                fflush(stdout);
                running = 0;
            }
			else
			{
            	printf("Name: %s | Opcode: %d\n", instr_lookup_table[op].name, op);
            	instr_lookup_table[op].function(instr);
			}
        }
        else
        {
            printf("No instruction found with opcode %d\n", op);
        }
    }
    return 0;
}
