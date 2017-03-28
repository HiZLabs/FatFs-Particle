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

FRESULT f_getfree_out(uint8_t driveNumber, uint64_t* bytesFreeOut, uint64_t* bytesUsedOut, uint64_t* bytesTotalOut) {
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;

    char path[3];
    path[0] = '0' + driveNumber;
    path[1] = ':';
    path[2] = 0;

    /* Get volume information and free clusters of drive 0 */
    FRESULT res = f_getfree(path, &fre_clust, &fs);

    /* Get total sectors and free sectors */
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;

    uint64_t totalBytes = tot_sect * 512ll;
    uint64_t freeBytes = fre_sect * 512ll;
    uint64_t usedBytes = totalBytes - freeBytes;

    if(bytesFreeOut)
    	*bytesFreeOut = freeBytes;

    if(bytesUsedOut)
    	*bytesUsedOut = usedBytes;

    if(bytesTotalOut)
    	*bytesTotalOut = totalBytes;

    return res;
}

//Print free space, adapted from FatFs documentation (http://elm-chan.org/fsw/ff/en/getfree.html)
FRESULT print_free_space(uint8_t driveNumber, Print& print) {
    uint64_t totalBytes;
    uint64_t freeBytes;
    uint64_t usedBytes;

    FRESULT result = f_getfree_out(driveNumber, &freeBytes, &usedBytes, &totalBytes);

    if(result)
    	return result;

    String prettyTotal = bytesToPretty(totalBytes);
    String prettyFree = bytesToPretty(freeBytes);
    String prettyUsed = bytesToPretty(usedBytes);

    print.printlnf("Disk %d usage", driveNumber);
    print.printlnf("%s used (%3.1f%%)", prettyUsed.c_str(), (double)usedBytes/(double)totalBytes * 100.0);
    print.printlnf("%s free (%3.1f%%)", prettyFree.c_str(), (double)freeBytes/(double)totalBytes * 100.0);
    print.printlnf("%s total\n", prettyTotal.c_str());

    return result;
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

FRESULT f_append_string(String& path, String& str)
{
	FIL fil;
	FRESULT result = f_open(&fil, path.c_str(), FA_OPEN_APPEND | FA_WRITE);
	if(result != FR_OK)
		return result;
	UINT bw;
	result = f_write(&fil, str.c_str(), str.length(), &bw);
	f_close(&fil);
	return result;
}

FRESULT FileLogHandler::setFilename(String filename)
{
	std::lock_guard<std::recursive_mutex> lg(_mutex);
	_filename = "";
	FIL fil;
	FRESULT result = f_open(&fil, filename.c_str(), FA_OPEN_APPEND | FA_WRITE);
	if(result != FR_OK)
		return result;

	result = f_close(&fil);
	if(result == FR_OK)
		_filename = filename;

	return result;
}

void FileLogHandler::setFormatMessageCallback(std::function<String(const char *, LogLevel, const char*, const LogAttributes)> formatMessageCallback)
{
	std::lock_guard<std::recursive_mutex> lg(_mutex);
	_formatMessageCallback = formatMessageCallback;
}

void FileLogHandler::setErrorCallback(std::function<bool(FRESULT)> errorCallback)
{
	std::lock_guard<std::recursive_mutex> lg(_mutex);
	_errorCallback = errorCallback;
}

FileLogHandler::FileLogHandler(LogLevel level, const LogCategoryFilters &filters) : LogHandler(level, filters)
{
	LogManager::instance()->addHandler(this);
}

static const char* extractFileName(const char *s) {
    const char *s1 = strrchr(s, '/');
    if (s1) {
        return s1 + 1;
    }
    return s;
}

static const char* extractFuncName(const char *s, size_t *size) {
    const char *s1 = s;
    for (; *s; ++s) {
        if (*s == ' ') {
            s1 = s + 1; // Skip return type
        } else if (*s == '(') {
            break; // Skip argument types
        }
    }
    *size = s - s1;
    return s1;
}

void formatLogMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr, String& outStr)
{
	const char *s = nullptr;
	    // Timestamp
	    if (attr.has_time) {
	        outStr += String::format("%010u ", (unsigned)attr.time);
	    }
	    // Category
	    if (category) {
	        outStr += '[';
	        outStr += category;
	        outStr += "] ";
	    }
	    // Source file
	    if (attr.has_file) {
	        s = extractFileName(attr.file); // Strip directory path
	        outStr += s; // File name
	        if (attr.has_line) {
	            outStr += ':';
	            outStr += String::format("%d", (int)attr.line); // Line number
	        }
	        if (attr.has_function) {
	            outStr += ", ";
	        } else {
	            outStr += ": ";
	        }
	    }
	    // Function name
	    if (attr.has_function) {
	        size_t n = 0;
	        s = extractFuncName(attr.function, &n); // Strip argument and return types
	        outStr += String(s, n);
	        outStr += "(): ";
	    }
	    // Level
	    s = LogHandler::levelName(level);
	    outStr += s;
	    outStr += ": ";
	    // Message
	    if (msg) {
	        outStr += msg;
	    }
	    // Additional attributes
	    if (attr.has_code || attr.has_details) {
	        outStr += " [";
	        // Code
	        if (attr.has_code) {
	            outStr += "code = ";
	            outStr += String::format("%" PRIiPTR, (intptr_t)attr.code);
	        }
	        // Details
	        if (attr.has_details) {
	            if (attr.has_code) {
	                outStr += ", ";
	            }
	            outStr += "details = ";
	            outStr += attr.details;
	        }
	        outStr += ']';
	    }
	    outStr += "\r\n";
}

void FileLogHandler::logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr)
{
	std::lock_guard<std::recursive_mutex> lg(_mutex);
	if(_filename.length() == 0)
		return;

	String str;
	if(_formatMessageCallback)
		str = _formatMessageCallback(msg, level, category, attr);
	else
	{
		formatLogMessage(msg, level, category, attr, str);
	}

	write(str.c_str(), str.length());
}

void FileLogHandler::write(const char *data, size_t size)
{
	std::lock_guard<std::recursive_mutex> lg(_mutex);
	if(_filename.length() == 0)
		return;

	bool repeat = false;
	do
	{
		repeat = false;
		String s(data, size);
		FRESULT result = f_append_string(_filename, s);
		Serial.printf("Logging to %s: %s", _filename.c_str(), s.c_str());
		if(result != FR_OK)
		{
			if(_errorCallback && _errorCallback(result))
			{
				repeat = true;
			}
			else
				break;
		}
	} while(repeat);

}
