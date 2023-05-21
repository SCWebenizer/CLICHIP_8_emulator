#include "CLICHIP_8_emulatorConfig.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>





/* Constant values */
#define CONST_STEPS_COUNT 1000

#define CONST_ARGC 2
#define CONST_OK 0
#define CONST_NOK 1

/**
 * @brief Most commonly the machines had 4096 (0x1000) bytes.
 * The CHIP-8 interpreter would be from 0 (0x000) to 511(0x1FF) bytes.
 * The program starts after the first 512 (0x200) bytes. Probably continues up until (0xE99). Total length of 3225 bytes.
 * The last 256 bytes (0xF00 - 0xFFF) are reserved for display refresh.
 * The 96 bytes below that (0xEA0 - 0xEFF), are reserved for the stack and other variables.
 */
#define CONST_MEMORY_SIZE_TOTAL (1 << 12)  // 4096 bytes
#define CONST_MEMORY_START_PROGRAM 0x200
#define CONST_MEMORY_START_RESERVED 0xEA0
#define CONST_MEMORY_START_STACK CONST_MEMORY_START_RESERVED
#define CONST_MEMORY_END_PROGRAM (CONST_MEMORY_START_RESERVED - 1)
#define CONST_MEMORY_END_RESERVED 0xF00
#define CONST_MEMORY_END_STACK CONST_MEMORY_END_RESERVED
#define CONST_MEMORY_SIZE_PROGRAM (CONST_MEMORY_END_PROGRAM - CONST_MEMORY_START_PROGRAM)
#define CONST_MEMORY_STACK_NESTING_LIMIT 12
// Last byte from the reserved memory area will be used as a stack counter, unless there is some better way of doing it
#define CONST_MEMORY_STACK_COUNTER_POS (CONST_MEMORY_END_RESERVED - 1)

#define CONST_DISPLAY_SIZE_X 64
// Formatting with a delimiter column and newline
#define CONST_DISPLAY_SIZE_X_WITH_FORMATTING (CONST_DISPLAY_SIZE_X + 2)
#define CONST_DISPLAY_SIZE_Y 32
// Formatting with two delimiter lines
#define CONST_DISPLAY_SIZE_Y_WITH_FORMATTING (CONST_DISPLAY_SIZE_Y + 2)
#define CONST_DISPLAY_SIZE_BUFFER (CONST_DISPLAY_SIZE_X_WITH_FORMATTING * CONST_DISPLAY_SIZE_Y_WITH_FORMATTING)
#define CONST_DISPLAY_CHARACTER_SET '#'
#define CONST_DISPLAY_CHARACTER_UNSET ' '

#define CONST_REGISTERS_COUNT 16  // Amount of 8-bit registers
#define CONST_OPCODE_POSITION 12  // Instructions are 2 bytes long, we want the 4 most significant bits from 2 bytes
#define CONST_OPCODE_ADDRESS_MASK 0x0FFF  // Mask used to get the 12-bit address from instructions
#define CONST_OPCODE_DATA_MASK 0x00FF  // Mask used to get the 8-bit data value
#define CONST_OPCODE_REGISTER_MASK 0x000F  // Mask used to get the 4-bit register index
#define CONST_OPCODE_REGISTER_X_OFFSET 8  // Offset of bits from the LSB of instructions to reach register X
#define CONST_OPCODE_REGISTER_Y_OFFSET 4  // Offset of bits from the LSB of instructions to reach register Y
#define CONST_REGISTERS_IR_INCREMENT 2  // Amount of bytes jumped over by the PC after an instruction execution
#define CONST_REGISTERS_IR_SKIP (CONST_REGISTERS_IR_INCREMENT << 1)  // Amount of bytes jumped over by the PC after the next instruction gets skipped
#define CONST_REGISTERS_VF_INDEX 0xF
#define CONST_REGISTERS_MAXVALUE 0xFFFF
#define CONST_REGISTERS_MSB_POSITION 15
#define CONST_REGISTERS_MSB_MASK (1 << CONST_REGISTERS_MSB_POSITION)




/* Data structures */
/* struct hwregs - represents the individual registers */
struct hwregs
{
        __uint8_t regV[CONST_REGISTERS_COUNT];
        __uint16_t regI;  // Address register
};

/* struct hwstate - represents the memory, registers and other resources */
struct hwstate
{
        __uint8_t mem[CONST_MEMORY_SIZE_TOTAL];
        // Display color is monochrome, 64 pixels width with 32 pixels height
        __uint64_t display[CONST_DISPLAY_SIZE_Y];
        struct hwregs regs;
        __uint16_t PC;
} state;




/* Functions */
/* Prints the display */
static inline void print_display(void)
{
        __uint8_t line, idx, buffer[CONST_DISPLAY_SIZE_BUFFER + 1];

        // Initial print buffer setup
        buffer[CONST_DISPLAY_SIZE_BUFFER] = 0;
        memset(buffer, CONST_DISPLAY_CHARACTER_UNSET, CONST_DISPLAY_SIZE_BUFFER);

        // Draw the screen frames
        for (idx = 0; idx < CONST_DISPLAY_SIZE_X; idx++)
        {
                buffer[idx] = '-';
                buffer[(CONST_DISPLAY_SIZE_Y_WITH_FORMATTING - 1) * CONST_DISPLAY_SIZE_X_WITH_FORMATTING + idx] = '-';
        }
        for (idx = 0; idx < CONST_DISPLAY_SIZE_Y_WITH_FORMATTING; idx++)
        {
                buffer[CONST_DISPLAY_SIZE_X_WITH_FORMATTING * (idx + 1) - 2] = '|';
                buffer[CONST_DISPLAY_SIZE_X_WITH_FORMATTING * (idx + 1) - 1] = '\n';
        }

        // Fill in the actual screen lines
        for (line = 0; line < CONST_DISPLAY_SIZE_Y; line++)
        {
                // Start at idx = 0 because there's no screen frame on the left side
                // Stop at idx < CONST_DISPLAY_SIZE_X because:
                // - the character at CONST_DISPLAY_SIZE_X is a line delimiter character for the screen frame
                // - the character at CONST_DISPLAY_SIZE_X + 1 is a newline
                for (idx = 0; idx < CONST_DISPLAY_SIZE_X; idx++)
                {
                        // A display line is a 64-bit number, use bitwise operations to fill the buffer
                        if (((state.display[line] >> idx) & 1) == 1)
                        {
                                // Bit is set, pixel is solid white
                                buffer[(line + 1) * CONST_DISPLAY_SIZE_X_WITH_FORMATTING + (CONST_DISPLAY_SIZE_X - idx - 1)] = CONST_DISPLAY_CHARACTER_SET;
                        }
                        else
                        {
                                // Bit is not set, pixel is solid black
                                buffer[(line + 1) * CONST_DISPLAY_SIZE_X_WITH_FORMATTING + (CONST_DISPLAY_SIZE_X - idx - 1)] = CONST_DISPLAY_CHARACTER_UNSET;
                        }
                }
        }

        printf("\n%s\n", buffer);
}

/* Updates the display with the sprite drawing information. Draws a sprite at coordinate (pos_x, pos_y) that has a width of 8 pixels and a height of n pixels */
void draw(__uint8_t pos_x, __uint8_t pos_y, __uint8_t n)
{
        (void) n;
        __uint8_t idx;
        __uint64_t data;
        __int8_t shifts;

        // TODO "Sprite pixels that are set flip the color of the corresponding screen pixel, while unset sprite pixels do nothing"
        // SO DOES IT MEAN I SHOULD LOOK ONLY AT THE SET BITS AND IGNORE THE UNSET BITS ?
        for (idx = 0; idx < n; idx++)
        {
                // Read the sprite data starting from I value location
                // TODO CHECK REGARDING LITTLE/BIG ENDIAN
                data = ((__uint64_t) state.mem[state.regs.regI + idx]);
                shifts = CONST_DISPLAY_SIZE_X - pos_x - 8;

                // printf("[%d] Data %01lx to be shifted %d bits\n", idx, data, shifts);
                // for (__int8_t i = 7; i >= 0; i--)
                // {
                //         printf("%c", '0' + (__uint8_t) ((data >> i) & 1));
                // }
                // printf("\n");
                if (shifts > 0)
                {
                        data = ((__uint64_t) state.mem[state.regs.regI + idx]) << shifts;
                }
                else
                {
                        data = ((__uint64_t) state.mem[state.regs.regI + idx]) >> (-shifts);
                }

                // VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and 0 if not
                if ((state.display[pos_y + idx] & data) > 0)
                {
                        // Pixel goes from set to unset
                        state.regs.regV[CONST_REGISTERS_VF_INDEX] = 1;
                }

                state.display[pos_y + idx] ^= data;
        }

        // printf("DEBUG:\n");
        // for (idx = 0; idx < CONST_DISPLAY_SIZE_Y; idx++)
        // {
        //         for (__int8_t i = CONST_DISPLAY_SIZE_X - 1; i >= 0; i--)
        //         {
        //                 printf("%c", '0' + (__uint8_t) ((state.display[idx] >> i) & 1));
        //         }
        //         printf("\n");
        // }
}

/* Returns the 12-bit address from an instruction */
static inline __uint16_t get_address(__uint16_t instruction)
{
        return instruction & CONST_OPCODE_ADDRESS_MASK;
}

/* Returns the second most significant byte from an instruction, representing register X */
static inline __uint8_t get_regX(__uint16_t instruction)
{
        return (instruction >> CONST_OPCODE_REGISTER_X_OFFSET) & CONST_OPCODE_REGISTER_MASK;
}

/* Returns the third most significant byte from an instruction, representing register Y */
static inline __uint8_t get_regY(__uint16_t instruction)
{
        return (instruction >> CONST_OPCODE_REGISTER_Y_OFFSET) & CONST_OPCODE_REGISTER_MASK;
}

/* Returns the last 2 or 1 most significant bytes from an instruction, representing the data */
static inline __uint8_t get_data(__uint16_t instruction, __uint8_t only_one)
{
        if (only_one == 1)
        {
                return instruction & CONST_OPCODE_REGISTER_MASK;
        }
        else
        {
                return instruction & CONST_OPCODE_DATA_MASK;
        }
}

/* Returns the instruction from the current PC pointer location in the memory */
static inline __uint16_t get_instruction(void)
{
        return ((state.mem[state.PC] << 8) | state.mem[state.PC + 1]);
}

/* Executes the instruction */
void execute_instruction(void)
{
        __uint16_t instruction = get_instruction();
        __uint16_t address;
        __uint8_t regX, regY, data;

        printf("[%03X] %04X      ", state.PC, instruction);

        switch (instruction >> CONST_OPCODE_POSITION)
        {
                case 0:
                        // Multiple cases.
                        address = get_address(instruction);
                        switch (address)
                        {
                                case 0x00E0:
                                        // 00E0 - Clears the screen
                                        printf("disp_clear()");
                                        memset(state.display, 0, sizeof(__uint64_t) * CONST_DISPLAY_SIZE_Y);
                                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                                        break;
                                case 0x00EE:
                                        // 00EE - Returns from a subroutine
                                        printf("return <%01X> [X]", state.mem[CONST_MEMORY_STACK_COUNTER_POS]);
                                        if (state.mem[CONST_MEMORY_STACK_COUNTER_POS] == 0)
                                        {
                                                printf("\nThere is no function to return from\n");
                                        }
                                        else
                                        {
                                                // Decrement the stack nesting level
                                                state.mem[CONST_MEMORY_STACK_COUNTER_POS]--;

                                                // Get the address from the stack
                                                address = state.mem[CONST_MEMORY_START_STACK + (state.mem[CONST_MEMORY_STACK_COUNTER_POS] << 1)]
                                                        | (state.mem[CONST_MEMORY_START_STACK + (state.mem[CONST_MEMORY_STACK_COUNTER_POS] << 1) + 1] << 8);

                                                // Jump to the new address
                                                state.PC = address;
                                        }
                                        break;
                                default:
                                        // 0NNN - Calls machine code routine at address NNN
                                        printf("Call machine code routine at address %03X", address);
                                        // TODO
                        }
                        break;
                case 0x1:
                        // 1NNN - Jumps to address NNN
                        address = get_address(instruction);
                        printf("goto %03X [X]", address);
                        state.PC = address;
                        break;
                case 0x2:
                        // 2NNN - Calls subroutine at NNN
                        address = get_address(instruction);
                        printf("*(%#05X)() <%01X> [X]", address, state.mem[CONST_MEMORY_STACK_COUNTER_POS]);
                        // Update the stack nesting level
                        if (state.mem[CONST_MEMORY_STACK_COUNTER_POS] >= CONST_MEMORY_STACK_NESTING_LIMIT)
                        {
                                printf("\nNesting limit reached, not executing\n");
                        }
                        else
                        {
                                // Save the next address on the stack
                                state.PC += CONST_REGISTERS_IR_INCREMENT;
                                state.mem[CONST_MEMORY_START_STACK + (state.mem[CONST_MEMORY_STACK_COUNTER_POS] << 1)] = state.PC & CONST_OPCODE_DATA_MASK;
                                state.mem[CONST_MEMORY_START_STACK + (state.mem[CONST_MEMORY_STACK_COUNTER_POS] << 1) + 1] = (state.PC >> 8) & CONST_OPCODE_DATA_MASK;

                                // Increment the stack nesting level
                                state.mem[CONST_MEMORY_STACK_COUNTER_POS]++;

                                // Jump to the new address
                                state.PC = address;
                        }
                        break;
                case 0x3:
                        // 3XNN - Skips the next instruction if VX equals NN
                        regX = get_regX(instruction);
                        data = get_data(instruction, 0);
                        printf("if (V%01x<%02x> == %02x)", regX, state.regs.regV[regX], data);
                        if (state.regs.regV[regX] == data)
                        {
                                printf(" >> TRUE [X]");
                                state.PC += CONST_REGISTERS_IR_SKIP;
                        }
                        else
                        {
                                printf(" >> FALSE [X]");
                                state.PC += CONST_REGISTERS_IR_INCREMENT;
                        }
                        break;
                case 0x4:
                        // 4XNN - Skips the next instruction if VX does not equal NN
                        regX = get_regX(instruction);
                        data = get_data(instruction, 0);
                        printf("if (V%01x<%02x> != %02x)",regX, state.regs.regV[regX], data);
                        if (state.regs.regV[regX] != data)
                        {
                                printf(" >> TRUE [X]");
                                state.PC += CONST_REGISTERS_IR_SKIP;
                        }
                        else
                        {
                                printf(" >> FALSE [X]");
                                state.PC += CONST_REGISTERS_IR_INCREMENT;
                        }
                        break;
                case 0x5:
                        // 5XY0 - Skips the next instruction if VX equals VY
                        regX = get_regX(instruction);
                        regY = get_regY(instruction);
                        printf("if (V%01x<%02x> == V%01x<%02x>)", regX, state.regs.regV[regX], regY, state.regs.regV[regY]);
                        if (state.regs.regV[regX] == state.regs.regV[regY])
                        {
                                printf(" >> TRUE [X]");
                                state.PC += CONST_REGISTERS_IR_SKIP;
                        }
                        else
                        {
                                printf(" >> FALSE [X]");
                                state.PC += CONST_REGISTERS_IR_INCREMENT;
                        }
                        break;
                case 0x6:
                        // 6XNN - Sets VX to NN
                        regX = get_regX(instruction);
                        data = get_data(instruction, 0);
                        printf("V%01x<%02x> = %02x [X]", regX, state.regs.regV[regX], data);
                        state.regs.regV[regX] = data;
                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                        break;
                case 0x7:
                        // 7XNN - Adds NN to VX (carry flag is not changed)
                        regX = get_regX(instruction);
                        data = get_data(instruction, 0);
                        printf("V%01x<%02x> += %02x [X]", regX, state.regs.regV[regX], data);
                        state.regs.regV[regX] += data;
                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                        // TODO CHECK CARRY FLAG STUFF
                        break;
                case 0x8:
                        // Multiple cases.
                        regX = get_regX(instruction);
                        regY = get_regY(instruction);
                        switch (instruction & 0x000F)
                        {
                                case 0x0000:
                                        // 8XY0 - Sets VX to the value of VY
                                        printf("V%01x<%02x> = V%01x<%02x> [X]", regX, state.regs.regV[regX], regY, state.regs.regV[regY]);
                                        state.regs.regV[regX] = state.regs.regV[regY];
                                        break;
                                case 0x0001:
                                        // 8XY1 - Sets VX to VX or VY. (bitwise OR operation)
                                        printf("V%01x<%02x> |= V%01x<%02x>", regX, state.regs.regV[regX], regY, state.regs.regV[regY]);
                                        state.regs.regV[regX] |= state.regs.regV[regY];
                                        printf(" >> %02x [X]", state.regs.regV[regX]);
                                        break;
                                case 0x0002:
                                        // 8XY2 - Sets VX to VX and VY. (bitwise AND operation)
                                        printf("V%01x<%02x> &= V%01x<%02x>", regX, state.regs.regV[regX], regY, state.regs.regV[regY]);
                                        state.regs.regV[regX] &= state.regs.regV[regY];
                                        printf(" >> %02x [X]", state.regs.regV[regX]);
                                        break;
                                case 0x0003:
                                        // 8XY3 - Sets VX to VX xor VY
                                        printf("V%01x<%02x> ^= V%01x<%02x>", regX, state.regs.regV[regX], regY, state.regs.regV[regY]);
                                        state.regs.regV[regX] ^= state.regs.regV[regY];
                                        printf(" >> %02x [X]", state.regs.regV[regX]);
                                        break;
                                case 0x0004:
                                        // 8XY4 - Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there is not
                                        printf("V%01x<%02x> += V%01x<%02x>", regX, state.regs.regV[regX], regY, state.regs.regV[regY]);
                                        if ((__uint32_t)state.regs.regV[regX] + state.regs.regV[regY] > CONST_REGISTERS_MAXVALUE)
                                        {
                                                state.regs.regV[CONST_REGISTERS_VF_INDEX] = 1;
                                        }
                                        else
                                        {
                                                state.regs.regV[CONST_REGISTERS_VF_INDEX] = 0;
                                        }
                                        state.regs.regV[regX] += state.regs.regV[regY];
                                        printf(" >> %02x [X]", state.regs.regV[regX]);
                                        break;
                                case 0x0005:
                                        // 8XY5 - VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there is not
                                        printf("V%01x<%02x> -= V%01x<%02x>", regX, state.regs.regV[regX], regY, state.regs.regV[regY]);
                                        if (state.regs.regV[regX] - state.regs.regV[regY] > state.regs.regV[regX])
                                        {
                                                state.regs.regV[CONST_REGISTERS_VF_INDEX] = 0;
                                        }
                                        else
                                        {
                                                state.regs.regV[CONST_REGISTERS_VF_INDEX] = 1;
                                        }
                                        state.regs.regV[regX] -= state.regs.regV[regY];
                                        printf(" >> %02x [X]", state.regs.regV[regX]);
                                        break;
                                case 0x0006:
                                        // 8XY6 - Stores the least significant bit of VX in VF and then shifts VX to the right by 1
                                        printf("V%01x<%02x> >>= 1", regX, state.regs.regV[regX]);
                                        state.regs.regV[CONST_REGISTERS_VF_INDEX] = state.regs.regV[regX] & 1;
                                        state.regs.regV[regX] >>= 1;
                                        printf(" >> %02x [X]", state.regs.regV[regX]);
                                        break;
                                case 0x0007:
                                        // 8XY7 - Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there is not
                                        printf("V%01x<%02x> = V%01x<%02x> V%01x<%02x>", regX, state.regs.regV[regX], regY, state.regs.regV[regY], regX, state.regs.regV[regX]);
                                        if (state.regs.regV[regY] - state.regs.regV[regX] > state.regs.regV[regY])
                                        {
                                                state.regs.regV[CONST_REGISTERS_VF_INDEX] = 0;
                                        }
                                        else
                                        {
                                                state.regs.regV[CONST_REGISTERS_VF_INDEX] = 1;
                                        }
                                        state.regs.regV[regX] = state.regs.regV[regY] - state.regs.regV[regX];
                                        printf(" >> %02x [X]", state.regs.regV[regX]);
                                        break;
                                case 0x000E:
                                        // 8XYE - Stores the most significant bit of VX in VF and then shifts VX to the left by 1
                                        printf("V%01x<%02x> <<= 1", regX, state.regs.regV[regX]);
                                        state.regs.regV[CONST_REGISTERS_VF_INDEX] = (state.regs.regV[regX] & CONST_REGISTERS_MSB_MASK) >> CONST_REGISTERS_MSB_POSITION;
                                        state.regs.regV[regX] <<= 1;
                                        printf(" >> %02x [X]", state.regs.regV[regX]);
                                        break;
                                default:
                                        // Unknown case
                                        printf("<UNDEFINED>");
                        }
                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                        break;
                case 0x9:
                        // 9XY0 - Skips the next instruction if VX does not equal VY
                        regX = get_regX(instruction);
                        regY = get_regY(instruction);
                        printf("if (V%01x<%02x> != V%01x<%02x>)", regX, state.regs.regV[regX], regY, state.regs.regV[regY]);
                        if (state.regs.regV[regX] != state.regs.regV[regY])
                        {
                                printf(" >> TRUE [X]");
                                state.PC += CONST_REGISTERS_IR_SKIP;
                        }
                        else
                        {
                                printf(" >> FALSE [X]");
                                state.PC += CONST_REGISTERS_IR_INCREMENT;
                        }
                        break;
                case 0xA:
                        // ANNN - Sets I to the address NNN
                        address = get_address(instruction);
                        printf("I = %03X [X]", address);
                        state.regs.regI = address;
                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                        break;
                case 0xB:
                        // BNNN - Jumps to the address NNN plus V0
                        address = get_address(instruction);
                        printf("PC = V0 + %03X [X]", address);
                        state.PC += address + state.regs.regV[0];
                        break;
                case 0xC:
                        // CXNN - Sets VX to the result of a bitwise and operation on a random number (Typically: 0 to 255) and NN
                        regX = get_regX(instruction);
                        data = get_data(instruction, 0);
                        __uint8_t tmp = (__uint8_t) rand();
                        printf("V%01x<%02x> = rand()<%02x> & %02x", regX, state.regs.regV[regX], tmp, data);
                        state.regs.regV[regX] = tmp & data;
                        printf(" >> %02x [X]", state.regs.regV[regX]);
                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                        break;
                case 0xD:
                        // DXYN - Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels
                        regX = get_regX(instruction);
                        regY = get_regY(instruction);
                        data = get_data(instruction, 1);
                        printf("draw(V%01x<%02x>, V%01x<%02x>, %01x) [X]", regX, state.regs.regV[regX], regY, state.regs.regV[regY], data);
                        draw(state.regs.regV[regX], state.regs.regV[regY], data);
                        // Setting VF is handled by the function
                        print_display();
                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                        break;
                case 0xE:
                        // Multiple cases.
                        switch (instruction & 0x00FF)
                        {
                                case 0x009E:
                                        // EX9E - Skips the next instruction if the key stored in VX is pressed
                                        printf("if (key() == V%01x)", (instruction & 0x0F00) >> 8);
                                        // TODO
                                        break;
                                case 0x00A1:
                                        // EXA1 - Skips the next instruction if the key stored in VX is not pressed
                                        printf("if (key() != V%01x)", (instruction & 0x0F00) >> 8);
                                        // TODO
                                        break;
                                default:
                                        // Unknown case
                                        printf("<UNDEFINED>");
                        }
                        break;
                case 0xF:
                        // Multiple cases.
                        switch (instruction & 0x00FF)
                        {
                                case 0x0007:
                                        // FX07 - Sets VX to the value of the delay timer
                                        printf("V%01x = get_delay()", (instruction & 0x0F00) >> 8);
                                        // TODO
                                        break;
                                case 0x000A:
                                        // FX0A - A key press is awaited, and then stored in VX (blocking operation, all instruction halted until next key event)
                                        printf("V%01x = get_key()", (instruction & 0x0F00) >> 8);
                                        // TODO
                                        break;
                                case 0x0015:
                                        // FX15 - Sets the delay timer to VX
                                        printf("delay_timer(V%01x)", (instruction & 0x0F00) >> 8);
                                        // TODO
                                        break;
                                case 0x0018:
                                        // FX18 - Sets the sound timer to VX
                                        printf("sound_timer(V%01x)", (instruction & 0x0F00) >> 8);
                                        // TODO
                                        break;
                                case 0x001E:
                                        // FX1E - Adds VX to I. VF is not affected
                                        regX = get_regX(instruction);
                                        printf("I += V%01x<%02x> [X]", regX, state.regs.regV[regX]);
                                        state.regs.regI += state.regs.regV[regX];
                                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                                        break;
                                case 0x0029:
                                        // FX29 - Sets I to the location of the sprite for the character in VX
                                        printf("I = sprite_addr[V%01x]", (instruction & 0x0F00) >> 8);
                                        // TODO
                                        break;
                                case 0x0033:
                                        // FX33 - Stores the binary-coded decimal representation of VX, with the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2
                                        printf("set_BCD(V%01x); (I+0) = BCD(3); (I+1) = BCD(2); (I+2) = BCD(1);", (instruction & 0x0F00) >> 8);
                                        // TODO
                                        break;
                                case 0x0055:
                                        // FX55 - Stores from V0 to VX (including VX) in memory, starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified
                                        regX = get_regX(instruction);
                                        printf("reg_dump(V%01x, &I) [X]", regX);
                                        for (data = 0; data < regX; data++)
                                        {
                                                state.mem[state.regs.regI + data] = state.regs.regV[data];
                                        }
                                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                                        break;
                                case 0x0065:
                                        // FX65 - Fills from V0 to VX (including VX) with values from memory, starting at address I. The offset from I is increased by 1 for each value read, but I itself is left unmodified
                                        regX = get_regX(instruction);
                                        printf("reg_load(V%01x, &I) [X]", regX);
                                        for (data = 0; data < regX; data++)
                                        {
                                                state.regs.regV[data] = state.mem[state.regs.regI + data];
                                        }
                                        state.PC += CONST_REGISTERS_IR_INCREMENT;
                                        break;
                                default:
                                        // Unknown case
                                        printf("<UNDEFINED>");
                        }
                        break;
        }

        printf("\n");
}

// TODO break up functions more
// TODO improve the argument parsing
// TODO add another argument to specify where the program entry point is
// TODO add more safety checks
// TODO improve the randomization
int main(int argc, char **argv)
{
        // Currently not being used
        (void) argc;

        /* Parsing program arguments */
        FILE *inputFile = fopen(argv[1], "r");
        if (argc != CONST_ARGC)
        {
                printf("Invalid argument count\n");
                return CONST_NOK;
        }

        if (inputFile == NULL)
        {
                printf("Opening input file %s returned error\n", argv[1]);
                return CONST_NOK;
        }



        /* Preparation and processing of the input file */
        // Initial variables
        __uint16_t bytesRead, oldPC;

        // Setup rand()
        srand(time(NULL));

        // Preparation, set all registers and memory to 0, and I to the start position
        memset(&state, 0, sizeof(struct hwstate));
        state.PC = CONST_MEMORY_START_PROGRAM;

        // Reading the program in the buffer in the common starting location
        bytesRead = (__uint16_t) fread(state.mem + CONST_MEMORY_START_PROGRAM, 1, CONST_MEMORY_SIZE_PROGRAM, inputFile);

        if (bytesRead > 0)
        {
                printf("Read %d out of %d bytes\n", bytesRead, CONST_MEMORY_SIZE_PROGRAM);
                if (bytesRead == CONST_MEMORY_SIZE_PROGRAM)
                {
                        printf("Program memory buffer is full, possibly the input file is bigger. Ignoring the rest\n");
                }

                for (int steps = CONST_STEPS_COUNT; steps > 0; steps--)
                {
                        oldPC = state.PC;
                        // print_instruction(instruction);
                        execute_instruction();
                        if (state.PC == oldPC)
                        {
                                printf("PC no longer advancing, aborting...\n");
                                break;
                        }
                }
        }
        else
        {
                printf("Could not read any bytes\n");
        }



        /* cleaning memory and exiting */
        if (fclose(inputFile) != 0)
        {
                printf("Closing file %s returned error\n", argv[1]);
        }

        return CONST_OK;
}
