#ifndef FATFS_UTILS_H
#define FATFS_UTILS_H

#include "FatFs/FatFs.h"

FRESULT scan_files (String path, Print& print);
FRESULT scan_files (String path, std::function<bool(String&, FILINFO&)> cb);
FRESULT print_free_space(uint8_t driveNumber, Print& print);
FRESULT print_file_info(const char* path, Print& print);
String bytesToPretty(uint64_t bytes);
FRESULT f_append_string(String& path, String& str);
FRESULT f_getfree_out(uint8_t driveNumber, uint64_t* bytesFreeOut, uint64_t* bytesUsedOut, uint64_t* bytesTotalOut);

#include "spark_wiring_logging.h"

class FileLogHandler: public spark::LogHandler {
public:
	FRESULT setFilename(String filename);
	void setFormatMessageCallback(std::function<String(const char *, LogLevel, const char*, const LogAttributes)> formatMessageCallback);
	void setErrorCallback(std::function<bool(FRESULT)> errorCallback);
    explicit FileLogHandler(LogLevel level = LOG_LEVEL_INFO, const LogCategoryFilters &filters = {});
protected:
    virtual void logMessage(const char *msg, LogLevel level, const char *category, const LogAttributes &attr) override;
    virtual void write(const char *data, size_t size) override;
private:
    String _filename;
    std::recursive_mutex _mutex;
    std::function<String(const char *, LogLevel, const char*, const LogAttributes)> _formatMessageCallback;
	std::function<bool(FRESULT)> _errorCallback;
};

#endif

