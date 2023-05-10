#include "CLICHIP_8_emulatorConfig.h"
#include <stdlib.h>
#include <stdio.h>



// Constant values
#define CONST_ARGC 2
#define CONST_OK 0
#define CONST_NOK 1
#define CONST_INSTRUCTION_BUFFER_COUNT (1 << 11)  // 2048 bytes for 1204 instructions
#define CONST_OPCODE_POSITION 12  // Instructions are 2 bytes long, we want the 4 most significant bits
#define CONST_OPCODE_ADDRESS_MASK 0x0FFF




// Prints the instruction and information about it
// TODO break this up
// TODO put more defined constant values instead
// TODO refine the log prints
// TODO clarify where actual instructions end and sprite data starts, related to I
// ^ Primarily, things can jump around, and address from I is used for graphics and non-graphics things
// TODO actually start adding registers and actual operations
void print_instruction(__uint16_t instruction, __uint16_t index)
{
        // For addresses
        __uint16_t address = instruction & CONST_OPCODE_ADDRESS_MASK;

        printf("[%03X] %04X ", index, instruction);

        switch (instruction >> CONST_OPCODE_POSITION)
        {
                case 0:
                        // Multiple cases.
                        switch (address)
                        {
                                case 0x00E0:
                                        // 00E0 - Clears the screen
                                        printf("disp_clear()");
                                        break;
                                case 0x00EE:
                                        // 00EE - Returns from a subroutine
                                        printf("return");
                                        break;
                                default:
                                        // 0NNN - Calls machine code routine at address NNN
                                        printf("Call machine code routine at address %03X", address);
                        }
                        break;
                case 0x1:
                        // 1NNN - Jumps to address NNN
                        printf("goto %03X", address);
                        break;
                case 0x2:
                        // 2NNN - Calls subroutine at NNN
                        printf("*(%#05X)()", address);
                        break;
                case 0x3:
                        // 3XNN - Skips the next instruction if VX equals NN
                        printf("if (V%01x == %02x)", (instruction & 0x0F00) >> 8, instruction & 0x00FF);
                        break;
                case 0x4:
                        // 4XNN - Skips the next instruction if VX does not equal NN
                        printf("if (V%01x != %02x)", (instruction & 0x0F00) >> 8, instruction & 0x00FF);
                        break;
                case 0x5:
                        // 5XY0 - Skips the next instruction if VX equals VY
                        printf("if (V%01x == V%01x)", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                        break;
                case 0x6:
                        // 6XNN - Sets VX to NN
                        printf("V%01x = %02x", (instruction & 0x0F00) >> 8, instruction & 0x00FF);
                        break;
                case 0x7:
                        // 7XNN - Adds NN to VX (carry flag is not changed)
                        printf("V%01x += %02x", (instruction & 0x0F00) >> 8, instruction & 0x00FF);
                        break;
                case 0x8:
                        // Multiple cases.
                        switch (instruction & 0x000F)
                        {
                                case 0x0000:
                                        // 8XY0 - Sets VX to the value of VY
                                        printf("V%01x = V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                                        break;
                                case 0x0001:
                                        // 8XY1 - Sets VX to VX or VY. (bitwise OR operation)
                                        printf("V%01x |= V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                                        break;
                                case 0x0002:
                                        // 8XY2 - Sets VX to VX and VY. (bitwise AND operation)
                                        printf("V%01x &= V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                                        break;
                                case 0x0003:
                                        // 8XY3 - Sets VX to VX xor VY
                                        printf("V%01x ^= V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                                        break;
                                case 0x0004:
                                        // 8XY4 - Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there is not
                                        printf("V%01x += V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                                        break;
                                case 0x0005:
                                        // 8XY5 - VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there is not
                                        printf("V%01x -= V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                                        break;
                                case 0x0006:
                                        // 8XY6 - Stores the least significant bit of VX in VF and then shifts VX to the right by 1
                                        printf("V%01x >>= V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                                        break;
                                case 0x0007:
                                        // 8XY7 - Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there is not
                                        printf("V%01x = V%01x - V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4, (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x000E:
                                        // 8XYE - Stores the most significant bit of VX in VF and then shifts VX to the left by 1
                                        printf("V%01x <<= V%01x", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                                        break;
                                default:
                                        // Unknown case
                                        printf("<UNDEFINED>");
                        }
                        break;
                case 0x9:
                        // 9XY0 - Skips the next instruction if VX does not equal VY
                        printf("if (V%01x != V%01x)", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4);
                        break;
                case 0xA:
                        // ANNN - Sets I to the address NNN
                        printf("I = %03X", address);
                        break;
                case 0xB:
                        // BNNN - Jumps to the address NNN plus V0
                        printf("PC = V0 + %03X", address);
                        break;
                case 0xC:
                        // CXNN - Sets VX to the result of a bitwise and operation on a random number (Typically: 0 to 255) and NN
                        printf("V%01x = rand() & %02x", (instruction & 0x0F00) >> 8, instruction & 0x00FF);
                        break;
                case 0xD:
                        // DXYN - Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels
                        printf("draw(V%01x, V%01x, %01x)", (instruction & 0x0F00) >> 8, (instruction & 0x00F0) >> 4, instruction & 0x000F);
                        break;
                case 0xE:
                        // Multiple cases.
                        switch (instruction & 0x00FF)
                        {
                                case 0x009E:
                                        // EX9E - Skips the next instruction if the key stored in VX is pressed
                                        printf("if (key() == V%01x)", (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x00A1:
                                        // EXA1 - Skips the next instruction if the key stored in VX is not pressed
                                        printf("if (key() != V%01x)", (instruction & 0x0F00) >> 8);
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
                                        break;
                                case 0x000A:
                                        // FX0A - A key press is awaited, and then stored in VX (blocking operation, all instruction halted until next key event)
                                        printf("V%01x = get_key()", (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x0015:
                                        // FX15 - Sets the delay timer to VX
                                        printf("delay_timer(V%01x)", (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x0018:
                                        // FX18 - Sets the sound timer to VX
                                        printf("sound_timer(V%01x)", (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x001E:
                                        // FX1E - Adds VX to I. VF is not affected
                                        printf("I += V%01x", (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x0029:
                                        // FX29 - Sets I to the location of the sprite for the character in VX
                                        printf("I = sprite_addr[V%01x]", (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x0033:
                                        // FX33 - Stores the binary-coded decimal representation of VX, with the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2
                                        printf("set_BCD(V%01x); (I+0) = BCD(3); (I+1) = BCD(2); (I+2) = BCD(1);", (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x0055:
                                        // FX55 - Stores from V0 to VX (including VX) in memory, starting at address I. The offset from I is increased by 1 for each value written, but I itself is left unmodified
                                        printf("reg_dump(V%01x, &I)", (instruction & 0x0F00) >> 8);
                                        break;
                                case 0x0065:
                                        // FX65 - Fills from V0 to VX (including VX) with values from memory, starting at address I. The offset from I is increased by 1 for each value read, but I itself is left unmodified
                                        printf("reg_load(V%01x, &I)", (instruction & 0x0F00) >> 8);
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
int main(int argc, char **argv)
{
        // Currently not being used
        (void) argc;

        // Parsing program arguments
        FILE *inputFile = fopen(argv[1], "r");
        if (inputFile == NULL)
        {
                printf("Opening input file %s returned error\n", argv[1]);
                return CONST_NOK;
        }

        // Processing input file
        __uint16_t bytesRead, tmp, instruction;
        __uint8_t buf[CONST_INSTRUCTION_BUFFER_COUNT + 1];

        buf[CONST_INSTRUCTION_BUFFER_COUNT] = 0;

        bytesRead = (__uint16_t) fread(buf, 1, CONST_INSTRUCTION_BUFFER_COUNT, inputFile);
        while (bytesRead > 0)
        {
                tmp = 0;
                while (tmp < bytesRead)
                {
                        instruction = ((buf[tmp] << 8) | buf[tmp + 1]);
                        print_instruction(instruction, tmp + 0x200);
                        tmp+=2;
                }

                bytesRead = (__uint16_t) fread(buf, 1, CONST_INSTRUCTION_BUFFER_COUNT, inputFile);
        }

        // cleaning memory and exiting
        if (fclose(inputFile) != 0)
        {
                printf("Closing file %s returned error\n", argv[1]);
        }
        return CONST_OK;
}
