#include "CLICHIP_8_emulatorConfig.h"
#include <stdlib.h>
#include <stdio.h>



#define CONST_ARGC 2
#define CONST_OK 0
#define CONST_NOK 1
#define CONST_BUFSIZE (1<<10)



int main(int argc, char **argv)
{
        (void) argc;

        FILE *inputFile = fopen(argv[1], "r");
        if (inputFile == NULL)
        {
                printf("Opening input file %s returned error\n", argv[1]);
                return CONST_NOK;
        }

        __uint32_t bytesRead, tmp;
        __uint8_t buf[CONST_BUFSIZE + 1];

        buf[CONST_BUFSIZE] = 0;

        bytesRead = fread(buf, 1, CONST_BUFSIZE, inputFile);
        while (bytesRead > 0)
        {
                tmp = 0;
                while (tmp < bytesRead)
                {
                        printf("[%u]%02x\n", tmp, buf[tmp]);
                        tmp++;
                }

                bytesRead = fread(buf, 1, CONST_BUFSIZE, inputFile);
        }

        if (fclose(inputFile) != 0)
        {
                printf("Closing file %s returned error\n", argv[1]);
        }
        return CONST_OK;
}
