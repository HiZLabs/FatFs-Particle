#include "FatFs/FatFs.h"

FRESULT scan_files (String path, Print& print);
FRESULT print_free_space(uint8_t driveNumber, Print& print);
FRESULT print_file_info(const char* path, Print& print);
String bytesToPretty(uint64_t bytes);

