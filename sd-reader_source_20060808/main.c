
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "fat16.h"
#include "fat16_config.h"
#include "partition.h"
#include "sd_raw.h"
#include "sd_raw_config.h"
#include "uart.h"

#define DEBUG 1

/**
 * \mainpage MMC/SD card example application
 *
 * This project is a small test application which implements read and write
 * support for MMC and SD cards.
 *
 * It includes
 * - low-level \link sd_raw MMC read/write routines \endlink
 * - \link partition partition table support \endlink
 * - a simple \link fat16 FAT16 read/write implementation \endlink
 *
 * \section circuit The circuit
 * The curcuit board is a self-made and self-soldered board consisting of a single
 * copper layer and standard DIL components, except of the MMC/SD card connector.
 *
 * The connector is soldered to the bottom side of the board. It has a simple
 * eject button which, when a card is inserted, needs some space beyond the connector
 * itself. As an additional feature the connector has two electrical switches
 * to detect wether a card is inserted and wether this card is write-protected.
 * 
 * I used two microcontrollers during development, the Atmel ATmega8 with 8kBytes
 * of flash, and its pin-compatible alternative, the ATmega168 with 16kBytes flash.
 * The first one is the one I started with, but when I implemented FAT16 write
 * support, I ran out of flash space and switched to the ATmega168.
 * 
 * \section pictures Pictures
 * \image html pic01.jpg "The circuit board used to implement and test this application."
 * \image html pic02.jpg "The MMC/SD card connector on the soldering side of the circuit board."
 *
 * \section software The software
 * The software is written in pure standard ANSI-C. Sure, it might not be the
 * smallest or the fastest one, but I think it is quite flexible.
 *
 * I implemented a simple command prompt which is accessible via the UART. With commands
 * similiar to Unix you can browse different directories, read and write files,
 * create new ones and delete them again, and browse different directories.
 *
 * \htmlonly
 * <p>
 * The following table shows some typical code sizes in bytes:
 * </p>
 *
 * <table border="1" cellpadding="2">
 *     <tr>
 *         <th>layer</th>
 *         <th>code size</th>
 *         <th>static RAM usage</th>
 *     </tr>
 *     <tr>
 *         <td>MMC/SD (read-only)</td>
 *         <td align="right">1034</td>
 *         <td align="right">0</td>
 *     </tr>
 *     <tr>
 *         <td>MMC/SD (read-write)</td>
 *         <td align="right">1486</td>
 *         <td align="right">517</td>
 *     </tr>
 *     <tr>
 *         <td>Partition</td>
 *         <td align="right">408</td>
 *         <td align="right">0</td>
 *     </tr>
 *     <tr>
 *         <td>FAT16 (read-only)</td>
 *         <td align="right">3596</td>
 *         <td align="right">0</td>
 *     </tr>
 *     <tr>
 *         <td>FAT16 (read-write)</td>
 *         <td align="right">6628</td>
 *         <td align="right">0</td>
 *     </tr>
 * </table>
 *
 * <p>
 * The static RAM in the read-write case is used for buffering memory card
 * access. Without this buffer, implementation would have been much more complicated.
 * </p>
 * 
 * <p>
 * Please note that the numbers above do not include the C library functions
 * used, e.g. malloc()/free() and some string functions. These will raise the
 * numbers somewhat if they are not already used in other program parts.
 * </p>
 * 
 * <p>
 * When opening a partition, filesystem, file or directory, a little amount
 * of dynamic RAM is used, as listed in the following table.
 * </p>
 *
 * <table border="1" cellpadding="2">
 *     <tr>
 *         <th>descriptor</th>
 *         <th>dynamic RAM</th>
 *     </tr>
 *     <tr>
 *         <td>partition</td>
 *         <td align="right">15</td>
 *     </tr>
 *     <tr>
 *         <td>filesystem</td>
 *         <td align="right">26</td>
 *     </tr>
 *     <tr>
 *         <td>file</td>
 *         <td align="right">51</td>
 *     </tr>
 *     <tr>
 *         <td>directory</td>
 *         <td align="right">47</td>
 *     </tr>
 * </table>
 * 
 * \endhtmlonly
 *
 * \section adaptation Adapting the software to your needs
 * The only hardware dependent part is the communication
 * layer talking to the memory card. The other parts like partition table and FAT16
 * support are completely independent, you could use them even for managing
 * Compact Flash cards or standard ATAPI hard disks.
 *
 * By changing the MCU* variables in the Makefile, you can use other Atmel
 * microcontrollers or different clock speeds. You might also want to disable
 * write support completely by changing the corresponding defines in the
 * sd_raw_config.h and fat16_config.h files.
 * 
 * \section bugs Bugs or comments?
 * If you have comments or found a bug in the software - there might be some
 * of them - you may contact me per mail at feedback@roland-riegel.de.
 *
 * \section acknowledgements Acknowledgements
 * Thanks go to Ulrich Radig, who explained on his homepage how to interface
 * MMC cards to the Atmel microcontroller (http://www.ulrichradig.de/).
 * I adapted his work for my circuit. Although this is a very simple
 * solution, I had no problems using it.
 * 
 * \section copyright Copyright 2006 by Roland Riegel
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation (http://www.gnu.org/copyleft/gpl.html).
 */

uint8_t read_line(char* buffer, uint8_t buffer_length);
uint8_t find_file_in_dir(struct fat16_fs_struct* fs, struct fat16_dir_struct* dd, const char* name, struct fat16_dir_entry_struct* dir_entry);
struct fat16_file_struct* open_file_in_dir(struct fat16_fs_struct* fs, struct fat16_dir_struct* dd, const char* name); 

int main()
{
    /* setup uart */
    uart_init();
    /* setup stdio */
    uart_connect_stdio();

    /* setup sd card slot */
    if(!sd_raw_init())
    {
#if DEBUG
        printf_P(PSTR("MMC/SD initialization failed\n"));
#endif
        return 1;
    }

    /* open first partition */
    struct partition_struct* partition = partition_open(sd_raw_read,
                                                        sd_raw_read_interval,
                                                        sd_raw_write,
                                                        0);

    if(!partition)
    {
        /* If the partition did not open, assume the storage device
         * is a "superfloppy", i.e. has no MBR.
         */
        partition = partition_open(sd_raw_read,
                                   sd_raw_read_interval,
                                   sd_raw_write,
                                   -1
                                  );
        if(!partition)
        {
#if DEBUG
            printf_P(PSTR("opening partition failed\n"));
#endif
            return 1;
        }
    }

    /* open file system */
    struct fat16_fs_struct* fs = fat16_open(partition);
    if(!fs)
    {
#if DEBUG
        printf_P(PSTR("opening filesystem failed\n"));
#endif
        return 1;
    }

    /* open root directory */
    struct fat16_dir_entry_struct directory;
    fat16_get_dir_entry_of_path(fs, "/", &directory);

    struct fat16_dir_struct* dd = fat16_open_dir(fs, &directory);
    if(!dd)
    {
#if DEBUG
        printf_P(PSTR("opening root directory failed\n"));
#endif
        return 1;
    }
    
    /* provide a simple shell */
    char buffer[24];
    while(1)
    {
        /* print prompt */
        uart_putc('>');
        uart_putc(' ');

        /* read command */
        char* command = buffer;
        if(read_line(command, sizeof(buffer)) < 1)
            continue;

        /* execute command */
        if(strncmp_P(command, PSTR("cd "), 3) == 0)
        {
            command += 3;
            if(command[0] == '\0')
                continue;

            /* change directory */
            struct fat16_dir_entry_struct subdir_entry;
            if(find_file_in_dir(fs, dd, command, &subdir_entry))
            {
                struct fat16_dir_struct* dd_new = fat16_open_dir(fs, &subdir_entry);
                if(dd_new)
                {
                    fat16_close_dir(dd);
                    dd = dd_new;
                    continue;
                }
            }

            printf_P(PSTR("directory not found: %s\n"), command);
        }
        else if(strcmp_P(command, PSTR("ls")) == 0)
        {
            /* print directory listing */
            struct fat16_dir_entry_struct dir_entry;
            while(fat16_read_dir(dd, &dir_entry))
            {
                printf_P(PSTR("%10lu  %s%c\n"),
                         dir_entry.file_size,
                         dir_entry.long_name,
                         (dir_entry.attributes & FAT16_ATTRIB_DIR) ? '/' : ' '
                        );
            }
        }
        else if(strncmp_P(command, PSTR("cat "), 4) == 0)
        {
            command += 4;
            if(command[0] == '\0')
                continue;
            
            /* search file in current directory and open it */
            struct fat16_file_struct* fd = open_file_in_dir(fs, dd, command);
            if(!fd)
            {
                printf_P(PSTR("error opening %s\n"), command);
                continue;
            }

            /* print file contents */
            uint8_t buffer[8];
            uint32_t offset = 0;
            while(fat16_read_file(fd, buffer, sizeof(buffer)) > 0)
            {
                printf_P(PSTR("%08lx: %02x %02x %02x %02x %02x %02x %02x %02x\n"),
                         offset,
                         buffer[0],
                         buffer[1],
                         buffer[2],
                         buffer[3],
                         buffer[4],
                         buffer[5],
                         buffer[6],
                         buffer[7]
                        );
                offset += 8;
            }

            fat16_close_file(fd);
        }
#if FAT16_WRITE_SUPPORT
        else if(strncmp_P(command, PSTR("rm "), 3) == 0)
        {
            command += 3;
            if(command[0] == '\0')
                continue;
            
            struct fat16_dir_entry_struct file_entry;
            if(find_file_in_dir(fs, dd, command, &file_entry))
            {
                if(fat16_delete_file(fs, &file_entry))
                    continue;
            }

            printf_P(PSTR("error deleting file: %s\n"), command);
        }
        else if(strncmp_P(command, PSTR("touch "), 6) == 0)
        {
            command += 6;
            if(command[0] == '\0')
                continue;

            struct fat16_dir_entry_struct file_entry;
            if(!fat16_create_file(dd, command, &file_entry))
                printf_P(PSTR("error creating file: %s\n"), command);
        }
        else if(strncmp_P(command, PSTR("write "), 6) == 0)
        {
            command += 6;
            if(command[0] == '\0')
                continue;

            char* offset_value = command;
            while(*offset_value != ' ' && *offset_value != '\0')
                ++offset_value;

            if(*offset_value == ' ')
                *offset_value++ = '\0';
            else
                continue;

            /* search file in current directory and open it */
            struct fat16_file_struct* fd = open_file_in_dir(fs, dd, command);
            if(!fd)
            {
                printf_P(PSTR("error opening %s\n"), command);
                continue;
            }

            int32_t offset = strtol(offset_value, 0, 0);
            if(!fat16_seek_file(fd, &offset, FAT16_SEEK_SET))
            {
                printf_P(PSTR("error seeking on %s\n"), command);
                fat16_close_file(fd);
                continue;
            }

            /* read text from the shell and write it to the file */
            uint8_t data_len;
            while(1)
            {
                /* give a different prompt */
                uart_putc('<');
                uart_putc(' ');

                /* read one line of text */
                data_len = read_line(buffer, sizeof(buffer));
                if(!data_len)
                    break;

                /* write text to file */
                if(fat16_write_file(fd, (uint8_t*) buffer, data_len) != data_len)
                {
                    printf_P(PSTR("error writing to file\n"));
                    break;
                }
            }

            fat16_close_file(fd);
        }
#endif
#if SD_RAW_WRITE_BUFFERING
        else if(strcmp_P(command, PSTR("sync")) == 0)
        {
            if(!sd_raw_sync())
                printf_P(PSTR("error syncing disk\n"));
        }
#endif
        else
        {
            printf_P(PSTR("unknown command: %s\n"), command);
        }
    }

    /* close file system */
    fat16_close(fs);

    /* close partition */
    partition_close(partition);
    
    return 0;
}

uint8_t read_line(char* buffer, uint8_t buffer_length)
{
    memset(buffer, 0, buffer_length);

    uint8_t read_length = 0;
    while(read_length < buffer_length - 1)
    {
        uint8_t c = uart_getc();
        uart_putc(c);

        if(c != '\n')
        {
            buffer[read_length] = c;
        }
        else
        {
            buffer[read_length] = '\0';
            break;
        }
        
        ++read_length;
    }

    return read_length;
}

uint8_t find_file_in_dir(struct fat16_fs_struct* fs, struct fat16_dir_struct* dd, const char* name, struct fat16_dir_entry_struct* dir_entry)
{
    while(fat16_read_dir(dd, dir_entry))
    {
        if(strcmp(dir_entry->long_name, name) == 0)
        {
            fat16_reset_dir(dd);
            return 1;
        }
    }

    return 0;
}

struct fat16_file_struct* open_file_in_dir(struct fat16_fs_struct* fs, struct fat16_dir_struct* dd, const char* name)
{
    struct fat16_dir_entry_struct file_entry;
    if(!find_file_in_dir(fs, dd, name, &file_entry))
        return 0;

    return fat16_open_file(fs, &file_entry);
}

