/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

#include "fs.h"
#include "memory.h"
#include "strings.h"
#include "fatfs/ff.h"

static FATFS fs;

bool mountFs(bool isSd)
{
    return isSd ? f_mount(&fs, "0:", 1) == FR_OK : f_mount(&fs, "1:", 1) == FR_OK;
}

void unmountCtrNand(void)
{
    f_mount(NULL, "1:", 1);
}

u32 fileRead(void *dest, const char *path, u32 maxSize)
{
    FIL file;
    u32 ret;

    if(f_open(&file, path, FA_READ) != FR_OK) ret = 0;
    else
    {
        u32 size = f_size(&file);
        if(size <= maxSize)
            f_read(&file, dest, size, (unsigned int *)&ret);
        f_close(&file);
    }

    return ret;
}

bool fileWrite(const void *buffer, const char *path, u32 size)
{
    FIL file;
    bool ret;

    switch(f_open(&file, path, FA_WRITE | FA_OPEN_ALWAYS))
    {
        case FR_OK:
        {
            unsigned int written;
            f_write(&file, buffer, size, &written);
            f_truncate(&file);
            f_close(&file);

            ret = (u32)written == size;
            break;
        }
        case FR_NO_PATH:
            for(u32 i = 1; path[i] != 0; i++)
                if(path[i] == '/')
                {
                    char folder[i + 1];
                    memcpy(folder, path, i);
                    folder[i] = 0;
                    f_mkdir(folder);
                }

            ret = fileWrite(buffer, path, size);
            break;
        default:
            ret = false;
            break;
    }

    return ret;
}

u32 firmRead(void *dest)
{
    const char *firmFolders[] = { "00000002", "20000002" };
    char path[48] = "1:/title/00040138/";
    concatenateStrings(path, firmFolders[ISN3DS ? 1 : 0]);
    concatenateStrings(path, "/content");

    DIR dir;

    u32 firmVersion = 0xFFFFFFFF,
        ret = 0;

    if(f_opendir(&dir, path) == FR_OK)
    {
        FILINFO info;

        //Parse the target directory
        while(f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0)
        {
            //Not a cxi
            if(info.fname[9] != 'a' || strlen(info.fname) != 12) continue;

            //Multiple cxis were found
            if(firmVersion != 0xFFFFFFFF) ret = 1;

            u32 tempVersion = hexAtoi(info.altname, 8);

            //FIRM is equal or newer than 11.0
            if(tempVersion >= (ISN3DS ? 0x21 : 0x52)) ret = tempVersion <= (ISN3DS ? 0x26 : 0x56) ? 5 : 2;

            //Found an older cxi
            if(tempVersion < firmVersion) firmVersion = tempVersion;
        }

        f_closedir(&dir);

        if(ret != 1 && firmVersion != 0xFFFFFFFF)
        {
            //Complete the string with the .app name
            concatenateStrings(path, "/00000000.app");

            //Convert back the .app name from integer to array
            hexItoa(firmVersion, path + 35, 8);

            if(!fileRead(dest, path, 0x100000)) ret = 3;
        }
    }

    if(firmVersion == 0xFFFFFFFF) ret = 4;

    return ret;
}