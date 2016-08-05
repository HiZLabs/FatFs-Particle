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

#ifndef LOG_SOURCE_CATEGORY
#define LOG_SOURCE_CATEGORY(x)
#endif

#ifndef LOG_CATEGORY
#define LOG_CATEGORY(x)
#endif

#ifndef LOG
#define LOG(x,y,...)
#endif


extern "C" {
#include "diskio.h"
}

#define DRIVE_NOT_ATTACHED 255

class FatFs;

class FatFsDriver {
private:
	FATFS fs;
	bool _attached;
	BYTE _driveNumber;
	friend class FatFs;
protected:
	FatFsDriver() : _attached(false), _driveNumber(DRIVE_NOT_ATTACHED) {}
public:
	virtual ~FatFsDriver();
	virtual DSTATUS initialize() = 0;
	virtual DSTATUS status() = 0;
	virtual DRESULT read(BYTE* buff, DWORD sector, UINT count) = 0;
	virtual DRESULT write(const BYTE* buff, DWORD sector, UINT count) = 0;
	virtual DRESULT ioctl(BYTE cmd, void* buff) = 0;
	BYTE driveNumber() { return _attached ? _driveNumber : DRIVE_NOT_ATTACHED; }
};

extern const char* FR_strings[];
static const char* FR_string(FRESULT result) { return result < 0 || result > 20 ? "CODE MESSAGE MISSING" : FR_strings[result]; }

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

#include "FatFs-SD.h"

#endif /* FATFS_PARTICLE_H_ */

