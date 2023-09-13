// ARM M4F Bootloader
// Up to 1MB flash devices
// GCC Compiler, C99, Linux
// Jason Losh

// General note:
// This code is designed to show the mechanism for providing bootloader support
// A real solution should guarantee that security is added to prevent code
// modification from unauthorized sources and is outside the scope of this code

// Notes on HEX file format:
//
// File is in plain text
// :LLAAAATT[DD...]CC
// LL (data length)
// AAAA (lower 16-bits of address)
// TT (type: 0=data, 1=end, 2=ext seg (not supported), 4=ext linear address)
// DD (LL bytes of data)
// CC (2's compliment of all bytes (LL, AAAA, TT, and DD's)
// if TT = 4, then LL = 2 and DDDD will be the upper 16-bits of address 
//   to be used for subsequent lines
// Addresses in the hex file are byte addresses

// Note on programming the M4F:
//
// Program memory can be written in rows consisting of 64 instructions
// Program memory can be erased in pagesToFlash consisting of 8 rows (512 instructions)
// A 2048 byte block of data is transmitted to the PIC for each page
// This corresponds to 1024 word addresses of data or 512 instructions of data

// Notes on code implementation on the M4F:
//
// The bootloader code area is BOOTLOADER_SIZE bytes starting at address 0x00000000
// BOOTLOADER_SIZE must be a multiple of the FLASH_PAGE_SIZE
// This code verifies that the hex file does not overwrite the bootloader
// The bootloader verifies that stack and reset pointers are in valid ranges
//
// Target code for the M4F using the bootloader should make changes to 
//  the CMD file to force the FLASH and .intvecs sections to start at address BOOTLOADER_SIZE

// To activate bootloader program, power-cycle with bootload request set

//-----------------------------------------------------------------------------
// Includes and defines
//-----------------------------------------------------------------------------

#include <inttypes.h> // c99 pri and scn macros
#include <stdlib.h>   // atoi, EXIT_SUCCESS
#include <stdio.h>    // printf, sprintf, fprintf, fscanf, fopen, fclose, fflush
#include <string.h>   // strncmp, memset
#include <stdint.h>   // c99 integers
#include <stdbool.h>  // bool
#include <termios.h>  // termios, tcgetattr, tcsetattr, tcdrain, cfsetospeed, cfsetispeed
#include <fcntl.h>    // open
#include <unistd.h>   // close, read, write
#include <errno.h>    // error codes and strings

#define FLASH_BASE_ADDRESS 0
#define FLASH_SIZE 262144
#define RAM_BASE_ADDRESS 0x20000000
#define RAM_SIZE 32768

#define ERASED_FLASH_BYTE_VALUE 255

#define FLASH_PAGE_SIZE 1024
#define BOOTLOADER_SIZE 4096
#define SP_INIT_OFFSET 0
#define PC_INIT_OFFSET 4

#define MAX_RETRIES 30

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool parseHexFile(const char strFile[], uint8_t map[])
{
    bool ok = true;
    FILE *file = NULL;
    bool eof = false;
    uint8_t type;
    uint8_t checksum;
    uint8_t length;
    uint32_t addrL;
    uint32_t addrH = 0;
    uint32_t addr;
    uint8_t temp;
    int record = 0;
    int i;

    // set memory map to flash erased state (NOPs)
    memset(map, ERASED_FLASH_BYTE_VALUE, FLASH_BASE_ADDRESS+FLASH_SIZE);

    // open file
    printf("Reading file... ");
    file = fopen(strFile, "r");
    ok = (file != NULL);
    if (!ok)
        printf("error opening file\n");

    // parse file and fill local memory map
    while (ok && !eof)
    {
        bool bColon = false;
        while (ok && !bColon)
        {
            char c;
            ok = fscanf(file, "%c", &c) != 0;
            if (ok)
            {
                if (c != 10 && c != 13)
                {
                    ok = bColon = (c == ':');
                }
            }
        }
        if (!ok)
                printf("format error\n");
        else
        {
            record++;
            ok = ok && fscanf(file, "%2"SCNx8, &length) != 0;
            checksum = length;
            ok = ok && fscanf(file, "%4"SCNx32, &addrL) != 0;
            checksum += addrL & 0xFF;
            checksum += (addrL >> 8) & 0xFF;
            ok = ok && fscanf(file, "%2"SCNx8, &type) != 0;
            checksum += type;
            switch (type)
            {
            case 0:
                for (i = 0; i < length; i++)
                {
                    ok = ok && fscanf(file, "%2"SCNx8, &temp) != 0;
                    addr = (addrH << 16) | addrL;
                    map[addr] = temp;
                    checksum += temp;
                    addrL ++;
                }
                break;
            case 1:
                eof = true;
                break;
            case 2:
                printf("Encountered extended segment... exiting\n");
                ok = false;
                break;
            case 4:
                ok = ok && fscanf(file, "%4"SCNx32, &addrH) != 0;
                checksum += addrH & 0xFF;
                checksum += (addrH >> 8) & 0xFF;
                break;
            default:
                eof = true;
                break;
            }
            uint8_t rxCheck;
            ok = ok && fscanf(file, "%2"SCNx8, &rxCheck) != 0;
            if (!ok)
                printf("Format error\n");
            else
            {
                checksum = 256 - checksum;
                ok = (rxCheck == checksum);
                if (!ok)
                    printf("Checksum error\n");
            }
        }
    }
    if (ok)
        printf("processed %d records\n", record);

    // close hex file
    if (file != NULL)
        fclose(file);
    return ok;
}

bool verifyImage(const uint8_t map[])
{
    bool ok = true;
    uint32_t add;

    // verify no code in map overlaps the bootloader space
    for (int i = FLASH_BASE_ADDRESS; i < FLASH_BASE_ADDRESS+BOOTLOADER_SIZE; i++)
        ok = ok && (map[i] == ERASED_FLASH_BYTE_VALUE);
    if (!ok)
        printf("Source file overlaps bootloader from 0x%08"PRIx32" to 0x%08"PRIx32"... exiting\n", FLASH_BASE_ADDRESS, FLASH_BASE_ADDRESS+BOOTLOADER_SIZE-1);

    // verify there is a valid stack pointer to RAM
    if (ok)
    {
        add = *(uint32_t*)&map[BOOTLOADER_SIZE + SP_INIT_OFFSET];
        ok = (add >= RAM_BASE_ADDRESS) && (add < RAM_BASE_ADDRESS + RAM_SIZE);
        if (!ok)
        {
            printf("Default SP not valid at address 0x%08"PRIx32"... exiting\n", BOOTLOADER_SIZE + SP_INIT_OFFSET);
            printf("  address was 0x%08x\n", add);
        }
    }

    // verify there is a valid reset pointer to flash
    if (ok)
    {
        add = *(uint32_t*)&map[BOOTLOADER_SIZE + PC_INIT_OFFSET];
        ok = (add >= FLASH_BASE_ADDRESS) && (add < FLASH_BASE_ADDRESS + FLASH_SIZE);
        if (!ok)
        {
            printf("Reset pointer not valid at address 0x%08"PRIx32"... exiting\n", BOOTLOADER_SIZE + PC_INIT_OFFSET);
            printf("  address was 0x%08x\n", add);
        }
    }
    return ok;
}

bool flashImage(const char strPort[], const uint8_t map[])
{
    bool ok = true;
    int port = -1;
    struct termios termio;
    uint32_t addr;
    uint32_t maxAddr;
    int8_t c;
    uint32_t data32;
    uint32_t checksum32;
    int retryCount = 0;
    uint32_t count;
    uint32_t bytesToFlash;
    uint32_t page;
    uint32_t pagesToFlash;
    uint32_t i;

    // open port
    printf("Opening %s... ", strPort);
    port = open(strPort, O_RDWR | O_NOCTTY);
    if (port < 0)
    {
        ok = false;
        printf("could not open port\n");
    }

    // set timeouts, set data rate and format for port
    // 115200 8N1, input mode, no hw flow control, 1s timeout
    if (ok)
    {
        memset(&termio, 0, sizeof(termio));
        cfsetispeed(&termio, B115200);
        cfsetospeed(&termio, B115200);
        termio.c_cflag &= ~CSIZE;   // 8 data bits
        termio.c_cflag |= CS8;
        termio.c_cflag &= ~PARENB;  // no parity bit
        termio.c_cflag &= ~CSTOPB;  // 1 stop bit
        termio.c_cflag &= ~CRTSCTS; // no flow control
        termio.c_cflag &= ~CLOCAL;  // ignore status lines
        termio.c_cflag &= ~CREAD;   // rx on
        termio.c_lflag &= ~ECHO;    // turn off echo
        termio.c_lflag &= ~ICANON;  // not canonical
        termio.c_lflag = 0;         // special processing off
        termio.c_iflag = IGNPAR;    // ignore parity errors
        termio.c_oflag = 0;
        termio.c_cc[VMIN] = 0;      // timeout at 1 sec
        termio.c_cc[VTIME] = 10;
        ok = tcsetattr(port, TCSANOW, &termio) == 0;
        if (!ok)
            printf("error setting port configuration\n");
    }
    if (ok)
        printf("successful\n");

    // find target device
    if (ok)
    {
        printf("Finding target device..");

        ok = false;
        while ((retryCount < MAX_RETRIES) && !ok)
        {
            printf(".");
            fflush(stdout);
            // write keyphrase
            write(port, "M4F_Unlock", 10);
            tcdrain(port);
            // get acknowledgement (k)
            count = read(port, &c, sizeof(c));
            ok = (count > 0) && (c == 'k');
            retryCount++;
        }
        if (ok)
            printf(" successful\n");
        else
            printf(" error\n");
    }
    
    // write header and data page count to M4F
    if (ok)
    {
        maxAddr = FLASH_BASE_ADDRESS;
        for (i = FLASH_BASE_ADDRESS; i < FLASH_BASE_ADDRESS + FLASH_SIZE; i++)
        {
            if (map[i] != ERASED_FLASH_BYTE_VALUE)
                maxAddr = i;
        }
        // calculate number of pagesToFlash
        bytesToFlash = maxAddr-BOOTLOADER_SIZE+1;
        pagesToFlash = ((bytesToFlash-1) / FLASH_PAGE_SIZE) + 1;
        if (pagesToFlash == 1)
        	printf("Downloading %d bytes (%d page) from 0x%08"PRIx32" to 0x%08"PRIx32"\n",
                bytesToFlash, pagesToFlash, BOOTLOADER_SIZE, BOOTLOADER_SIZE+bytesToFlash-1);
        else
        	printf("Downloading %d bytes (%d pages) from 0x%08"PRIx32" to 0x%08"PRIx32"\n",
                bytesToFlash, pagesToFlash, BOOTLOADER_SIZE, BOOTLOADER_SIZE+bytesToFlash-1);
        if (ok)
        {
            // send block count (32b little-endian)
            write(port, &pagesToFlash, sizeof(pagesToFlash));

            // send header 1's complement checksum (32b little endian)
            checksum32 = ~pagesToFlash;
            write(port, &checksum32, sizeof(checksum32));

            // flush out buffers in preparation for receiving checksum back
            tcdrain(port);

            // read checksum back
            read(port, &data32, sizeof(data32));
            if (data32 != checksum32)
            {
                ok = false;
                printf("Checksum error in header: TX 0x%08"PRIx32", RX 0x%08"PRIx32"\n", checksum32, data32);
            }

            // write page address, data, and checksum (32b little-endian) for each block
            addr = BOOTLOADER_SIZE;
            page = 0;
            while (ok && (page < pagesToFlash))
            {
                // write starting word address for page
                checksum32 = addr;
                write(port, &addr, sizeof(addr));
                for (int j = 0; j < FLASH_PAGE_SIZE / sizeof(uint32_t); j++)
                {
                    data32 = *(uint32_t*)&map[addr];
                    checksum32 += data32;
                    write(port, &data32, sizeof(data32));
                    addr += sizeof(uint32_t);
                }

                // send 1's complement checksum
                checksum32 = ~checksum32;
                write(port, &checksum32, sizeof(checksum32));
                tcdrain(port);
                count = read(port, &data32, sizeof(data32));
                if (count != sizeof(data32))
                {
                    ok = false;
                    printf("Timeout receiving header checksum\n");
                }
                else if (data32 != checksum32)
                {
                    ok = false;
                    printf("Checksum error at address 0x%08x: TX 0x%08"PRIx32", RX 0x%08"PRIx32"\n", addr, checksum32, data32);
                }

                // make sure write is done
                read(port, &c, sizeof(c));
                if (c != 'd')
                {
                    ok = false;
                    printf("Error writing page at address %08"PRIx32"\n", addr - FLASH_PAGE_SIZE);
                }
                page++;
            }
        }
    }

    // close serial port
    if (port >= 0)
        close(port);
    return ok;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    uint8_t map[FLASH_BASE_ADDRESS+FLASH_SIZE];
    bool ok = true;
    char strPort[20]= "ttyS0";

    printf("\nARM M4F Bootloader\n");

    // parse command line
    ok = argc >= 2;
    if (ok)
    {
        if (argc == 3)
        {
            ok = false;
            if (strncmp(argv[2], "COM", 3) == 0)
            {
                ok = true;
                sprintf(strPort, "/dev/ttyS%u", atoi(&argv[2][3])-1);
            }
            if (strncmp(argv[2], "tty", 3) == 0)
            {
                ok = true;
                sprintf(strPort, "/dev/%s", argv[2]);
            }
        }
    }

    if (!ok)
    {
        printf("usage: loader filename.hex [COMx][ttyx]\n");
        printf("         COMx  selects a port with Windows name\n");
        printf("         ttyx  selects a port with Linux tty name\n");
        printf("         default port is ttyS0 (COM1)\n");
    }

    // parse hex file
    if (ok)
        ok = parseHexFile(argv[1], map);

    // verify contents of image
    if (ok)
        ok = verifyImage(map);

    // flash image onto M4F
    if (ok)
        ok = flashImage(strPort, map);

    // indicate if successful
    if (ok)
    {
        printf("Bootload successful\n");
    }
    printf("\n");


    // exit
    return EXIT_SUCCESS;
}
