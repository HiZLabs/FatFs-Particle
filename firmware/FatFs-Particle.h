/*----------------------------------------------------------------------------/
/  FatFs Driver Linkage for Particle                                          /
/-----------------------------------------------------------------------------/
/
/ Copyright (C) 2016, ChaN, Aaron Heise, all right reserved.
/
/ FatFs module is an open source software. Redistribution and use of FatFs in
/ source and binary forms, with or without modification, are permitted provided
/ that the following condition is met:

/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/
#ifndef FATFS_PARTICLE_H_
#define FATFS_PARTICLE_H_

#include "ff.h"
#include <vector>
#include <memory>
#include "application.h"

extern "C" {
#include "diskio.h"
}

class FatFsDriver : std::mutex {
private:
	FATFS fs;
	bool _attached;
	friend class FatFs;
protected:
	FatFsDriver() : _attached(false) {}
public:
	virtual ~FatFsDriver() {}
	virtual DSTATUS initialize() = 0;
	virtual DSTATUS status() = 0;
	virtual DRESULT read(BYTE* buff, DWORD sector, UINT count) = 0;
	virtual DRESULT write(const BYTE* buff, DWORD sector, UINT count) = 0;
	virtual DRESULT ioctl(BYTE cmd, void* buff) = 0;
};

static const char* FR_strings[] = {
		/*FR_OK = 0*/				"(0) Succeeded",
		/*FR_DISK_ERR*/				"(1) A hard error occurred in the low level disk I/O layer",
		/*FR_INT_ERR*/				"(2) Assertion failed",
		/*FR_NOT_READY*/			"(3) The physical drive cannot work",
		/*FR_NO_FILE*/				"(4) Could not find the file",
		/*FR_NO_PATH*/				"(5) Could not find the path",
		/*FR_INVALID_NAME*/			"(6) The path name format is invalid",
		/*FR_DENIED*/				"(7) Access denied due to prohibited access or directory full",
		/*FR_EXIST*/				"(8) Access denied due to prohibited access",
		/*FR_INVALID_OBJECT*/		"(9) The file/directory object is invalid",
		/*FR_WRITE_PROTECTED*/		"(10) The physical drive is write protected",
		/*FR_INVALID_DRIVE*/		"(11) The logical drive number is invalid",
		/*FR_NOT_ENABLED*/			"(12) The volume has no work area",
		/*FR_NO_FILESYSTEM*/		"(13) There is no valid FAT volume",
		/*FR_MKFS_ABORTED*/			"(14) The f_mkfs() aborted due to any problem",
		/*FR_TIMEOUT*/				"(15) Could not get a grant to access the volume within defined period",
		/*FR_LOCKED*/				"(16) The operation is rejected according to the file sharing policy",
		/*FR_NOT_ENOUGH_CORE*/		"(17) LFN working buffer could not be allocated",
		/*FR_TOO_MANY_OPEN_FILES*/	"(18) Number of open files > _FS_LOCK",
		/*FR_INVALID_PARAMETER*/	"(19) Given parameter is invalid",
};

static const char* FR_string(FRESULT result) { return result < 0 || result > sizeof(FR_strings)/sizeof(const char*) ? "CODE MESSAGE MISSING" : FR_strings[result]; }

class FatFs {
private:
	static std::vector<FatFsDriver*> _drivers;
	friend DSTATUS disk_initialize(BYTE pdrv);
	friend DSTATUS disk_status(BYTE pdrv);
	friend DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
	friend DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
	friend DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff);
public:
	static FRESULT attach(FatFsDriver& driver, BYTE driveNumber);
	static void detach(BYTE driveNumber);
	static const char* fileResultMessage(FRESULT fileResult) { return FR_string(fileResult); }
};

#include "SDSPIDriver.h"

#endif /* FATFS_PARTICLE_H_ */

