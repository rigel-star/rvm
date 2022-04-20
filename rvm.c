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
	OP_BR, /* branch */
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


// sign extend
uint16_t sext(uint16_t x, int bit_count)
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
        uint16_t imm5 = sext(instr & 0x1F, 5);
        reg[r0] = reg[r1] + imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] + reg[r2];
    }

    update_flags(r0);
}


void and(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if(imm_flag)
    {
        uint16_t imm5 = sext(instr & 0x1F, 5);
        reg[r0] = reg[r1] & imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] & reg[r2];
    }

    update_flags(r0);
}


void not(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;

    reg[r0] = ~reg[r1];
    update_flags(r0);
}


void ldi(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t offset = sext(instr & 0x1FF, 9);

    reg[r0] = mem_read(reg[R_PC] + offset);
	printf("%d\n", reg[r0]);

    update_flags(r0);
}


void br(uint16_t instr)
{
    uint16_t pc_offset = sext(instr & 0x1FF, 9);
    uint16_t cond_flag = (instr >> 9) & 0x7;
    if (cond_flag & reg[R_COND])
    {
        reg[R_PC] += pc_offset;
    }
}


void jmp(uint16_t instr)
{
    uint16_t r1 = (instr >> 6) & 0x7;
    reg[R_PC] = reg[r1];
}


void jsr(uint16_t instr)
{
    uint16_t long_flag = (instr >> 11) & 1;
    reg[R_R7] = reg[R_PC];
    if (long_flag)
    {
        uint16_t long_pc_offset = sext(instr & 0x7FF, 11);
        reg[R_PC] += long_pc_offset;  /* JSR */
    }
    else
    {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r1]; /* JSRR */
    }
}


void ld(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sext(instr & 0x1FF, 9);
    reg[r0] = mem_read(reg[R_PC] + pc_offset);
    update_flags(r0);
}


void ldr(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sext(instr & 0x3F, 6);
    reg[r0] = mem_read(reg[r1] + offset);
    update_flags(r0);
}


void lea(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sext(instr & 0x1FF, 9);
    reg[r0] = reg[R_PC] + pc_offset;
    update_flags(r0);
}


void st(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sext(instr & 0x1FF, 9);
    mem_write(reg[R_PC] + pc_offset, reg[r0]);
}


void sti(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sext(instr & 0x1FF, 9);
    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
}


void str(uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sext(instr & 0x3F, 6);
    mem_write(reg[r1] + offset, reg[r0]);
}


void rti(uint16_t instr)
{
    printf("Not to be used: %s\n", __func__);
    abort();
}


void res(uint16_t instr)
{
    printf("Not to be used: %s\n", __func__);
    abort();
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


// takes single char as input and spits it out back
void trap_in()
{
    char c = getchar();
    putc(c, stdout);
    fflush(stdout);
    reg[R_R0] = (uint16_t) c;

    update_flags(R_R0);
}


typedef struct
{
	char *name;
	void (*trap_handler)();
} Trap_Def;


Trap_Def trap_lookup_table[TRAP_COUNT] = {
	[TRAP_GETC % 0x20] = {.name = "getc", .trap_handler = trap_getc},
	[TRAP_PUTS % 0x20] = {.name = "puts", .trap_handler = trap_puts},
    [TRAP_PUTSP % 0x20] = {.name = "putsp", .trap_handler = trap_putsp},
	[TRAP_OUT % 0x20] = {.name = "out", .trap_handler = trap_out},
    [TRAP_IN % 0x20] = {.name = "in", .trap_handler = trap_in}
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
    [OP_AND] = {.name = "and", .function = and},
    [OP_NOT] = {.name = "not", .function = not},
    [OP_BR] = {.name = "br", .function = br},
    [OP_JMP] = {.name = "jmp", .function = jmp},
    [OP_JSR] = {.name = "jsr", .function = jsr},
    [OP_ST] = {.name = "st", .function = st},
    [OP_STI] = {.name = "sti", .function = sti},
    [OP_STR] = {.name = "str", .function = str},
    [OP_LDI] = {.name = "ldi", .function = ldi},
    [OP_LDR] = {.name = "ldr", .function = ldr},
    [OP_LEA] = {.name = "lea", .function = lea},
    [OP_LD] = {.name = "ld", .function = ld},
    [OP_RES] = {.name = "res", .function = res},
    [OP_RTI] = {.name = "rti", .function = rti}
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

    memory[PC_START] = (OP_TRAP << 12) | TRAP_IN;
    memory[PC_START + 1] = (OP_TRAP << 12) | TRAP_HALT;

    uint8_t running = 1;
    while(running)
    {
        uint16_t instr = memory[reg[R_PC]++];
        uint16_t op = instr >> 12;

        // if instruction is a trap
        if(op == OP_TRAP)
        {
            uint16_t trap_code = (instr & 0xFF);       
            if(trap_code == TRAP_HALT)
            {
                puts("[HALT]");
                fflush(stdout);
                running = 0;
            }
            else 
                trap_lookup_table[trap_code % 0x20].trap_handler();
        }
        // if instruction is opcode 
        else
            instr_lookup_table[op].function(instr);
    }
    return 0;
}