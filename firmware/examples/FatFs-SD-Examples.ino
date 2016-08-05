#include "FatFs/FatFs.h"
/* The MIT License (MIT)
 *
 * Copyright (c) 2016 Hi-Z Labs, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

//Declare an instance of the SD card driver
FatFsSD SD0;

//If building for a platform with threads, set up disk activity timer
//Takes over RGB LED as and blinks to indicate disk activity
#if PLATFORM_THREADED
Timer diskActivityIndicatorTimer(1, updateDiskActivityIndicator);
#endif

void setup() {
    //set up USB serial for output and wait a bit for it to connect
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Setting up SD driver");
    //SPI interface (A3 = SCK, A4 = DO, A5 = DI); CS is A2
    SD0.begin(SPI, A2);
    //Class 10 cards can go pretty fast.
    //If you have trouble, try lowering this to 25000000 or less.
    SD0.highSpeedClock(50000000);
    //Enable the card detect
    pinMode(D0, INPUT_PULLUP);
    SD0.enableCardDetect(D0, HIGH);
    //Mount drive
    Serial.println("Attaching SD card to drive 0:");
    FRESULT result = FatFs::attach(SD0, 0);
    
    if(result != FR_OK) {
        Serial.println(String("Error attaching SD drive: ") + FatFs::fileResultMessage(result));
    } else {
        Serial.printlnf("SD drive mounted %s", FatFs::fileResultMessage(result));
    
        //if building on a threaded platform, take over LED and start the activity timer
#if PLATFORM_THREADED
        RGB.control(true);
        diskActivityIndicatorTimer.start();
#endif
        //print recursive directory list to Serial
        scan_files("0:", Serial);
        Serial.println();
        //print disk space usage summary to Serial
        print_free_space(0, Serial);
    }
}

//disk activity timer callback - light is red when something happened
void updateDiskActivityIndicator() {
    if(SD0.wasBusySinceLastCheck())
    {
        RGB.color(255,64,64);
    } else {
        RGB.color(0,0,0);
    }
}

void loop() {
    static FIL file;
    
    //1. open a file
    //2. read the first line in
    //3. interpret it as a number,
    //4. increment the number
    //5. truncate the file
    //6. write the incremented number back
    
    //open the file
    FRESULT result = f_open(&file, "0:/sd_test.txt", FA_READ | FA_WRITE | FA_OPEN_ALWAYS );
    if (result == FR_OK)
    {
        char line[81];
        //read a line
        f_gets(line, sizeof line, &file);
        //parse and increment number
        String nextNumber = String(String(line).toInt()+1);
        //move the read/write pointer to the beginning of the file
        f_lseek(&file, 0);
        //truncate the file after the read/write pointer
        f_truncate(&file);
        //write the new number out to the file
        f_puts(nextNumber, &file);
        //close the file to write the data to disk
        result = f_close(&file);
        //report out
        Serial.printf("[%s] ", nextNumber.c_str());
        print_file_info("0:/sd_test.txt", Serial);
        
    } else {
        Serial.printlnf("Error opening file: %s", FatFs::fileResultMessage(result));
    }
    delay(1000);
}

void print_file_info(const char* path, const FILINFO& fileInfo, Print& print) {
    bool isDir;
    Serial.printf("%c%c%c%c%c",
           (isDir = fileInfo.fattrib & AM_DIR) ? 'D' : '-',
           (fileInfo.fattrib & AM_RDO) ? 'R' : '-',
           (fileInfo.fattrib & AM_HID) ? 'H' : '-',
           (fileInfo.fattrib & AM_SYS) ? 'S' : '-',
           (fileInfo.fattrib & AM_ARC) ? 'A' : '-');
    Serial.printf("  %u/%02u/%02u  %02u:%02u",
           (fileInfo.fdate >> 9) + 1980, fileInfo.fdate >> 5 & 15, fileInfo.fdate & 31,
           fileInfo.ftime >> 11, fileInfo.ftime >> 5 & 63);
    Serial.printf(isDir ? "        " : "  %s  ", isDir ? 0 : bytesToPretty(fileInfo.fsize).c_str());
    Serial.println(path);
}

FRESULT print_file_info(const char* path, Print& print) {
    FILINFO fileInfo;
    FRESULT result = f_stat(path, &fileInfo);
    if(result == FR_OK)
    {
        print_file_info(path, fileInfo, print);
    } else {
                Serial.printlnf("%s %s", path, FatFs::fileResultMessage(result));
    }
    return result;
}

//Recursive directory list, adapted from FatFs documentation (http://elm-chan.org/fsw/ff/en/readdir.html)
FRESULT scan_files (String path, Print& print)
{
    FRESULT res;
    DIR dir;
    static FILINFO fno;

    res = f_opendir(&dir, path);                            /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);                    /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;   /* Break on error or end of dir */
            String nextPath = path + "/" + fno.fname;
            print_file_info(nextPath, fno, print);
            if (fno.fattrib & AM_DIR) {                     /* It is a directory */
                
                res = scan_files(nextPath, print);          /* Enter the directory */
                if (res != FR_OK) break;
            }
        }
        f_closedir(&dir);
    }

    return res;
}

//Print free space, adapted from FatFs documentation (http://elm-chan.org/fsw/ff/en/getfree.html)
FRESULT print_free_space(uint8_t driveNumber, Print& print) {
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;

    char path[3];
    path[0] = '0' + driveNumber;
    path[1] = ':';
    path[2] = 0;

    /* Get volume information and free clusters of drive 0 */
    FRESULT res = f_getfree(path, &fre_clust, &fs);
    if (res)
        print.printlnf("Failed to get free space %s", FatFs::fileResultMessage(res));

    /* Get total sectors and free sectors */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    uint64_t totalBytes = tot_sect * 512ll;
    uint64_t freeBytes = fre_sect * 512ll;
    uint64_t usedBytes = totalBytes - freeBytes;
    
    String prettyTotal = bytesToPretty(totalBytes);
    String prettyFree = bytesToPretty(freeBytes);
    String prettyUsed = bytesToPretty(usedBytes);
    
    print.printlnf("Disk %c usage", path[0]);
    print.printlnf("%s used (%3.1f%%)", prettyUsed.c_str(), (double)usedBytes/(double)totalBytes * 100.0);
    print.printlnf("%s free (%3.1f%%)", prettyFree.c_str(), (double)freeBytes/(double)totalBytes * 100.0);
    print.printlnf("%s total\n", prettyTotal.c_str());
    
    return res;
}

static const char prefixes[] = { 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };

#include <math.h>
String bytesToPretty(uint64_t bytes) {
    char buf[15];
    double value = bytes;
    uint8_t oom = 0;
    while(value > 1000 && oom < 7)
    {
        value /= 1024;
        ++oom;
    }
    double integral;
    double fractional = modf(value, &integral);
    
    if(oom == 0)
        snprintf(buf, 15, " %3.0f", integral);
    else if(fractional < 0.05 || integral >= 10.0)
        snprintf(buf, 15, "%3.0f%c", integral, prefixes[oom-1]);
    else
        snprintf(buf, 15, "%3.1f%c", value, prefixes[oom-1]);
    return String(buf);
}
