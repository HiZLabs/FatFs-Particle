#include "utils.h"

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
    return scan_files(path, [&print](String& filePath, FILINFO& fno) -> bool
	{
		print_file_info(filePath, print);
		return true;
	});
}

FRESULT scan_files(String path, std::function<bool(String&, FILINFO&)> cb)
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
            bool result = cb(nextPath, fno);
            if (fno.fattrib & AM_DIR) {                     /* It is a directory */
            	if(result)
            	{
					res = scan_files(nextPath, cb);          /* Enter the directory */
					if (res != FR_OK) break;
            	}
            } else if(!result)
            	break;
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
