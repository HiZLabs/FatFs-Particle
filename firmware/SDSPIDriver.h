/*-----------------------------------------------------------------------/
/  Low level disk interface modlue include file   (C)ChaN, 2013          /
/-----------------------------------------------------------------------*/

#ifndef FATFS_PARTICLE_SDSPI_DRIVER
#define FATFS_PARTICLE_SDSPI_DRIVER

#ifndef __cplusplus
#error "SDSPIDriver must be included only in C++"
#else

#include "FatFs-Particle.h"

#if !PLATFORM_THREADING
#error "SDSPIDriver currently requires threading"
#endif

/* MMC/SD command */
#define CMD0	(0)			/* GO_IDLE_STATE */
#define CMD1	(1)			/* SEND_OP_COND (MMC) */
#define	ACMD41	(0x80+41)	/* SEND_OP_COND (SDC) */
#define CMD8	(8)			/* SEND_IF_COND */
#define CMD9	(9)			/* SEND_CSD */
#define CMD10	(10)		/* SEND_CID */
#define CMD12	(12)		/* STOP_TRANSMISSION */
#define ACMD13	(0x80+13)	/* SD_STATUS (SDC) */
#define CMD16	(16)		/* SET_BLOCKLEN */
#define CMD17	(17)		/* READ_SINGLE_BLOCK */
#define CMD18	(18)		/* READ_MULTIPLE_BLOCK */
#define CMD23	(23)		/* SET_BLOCK_COUNT (MMC) */
#define	ACMD23	(0x80+23)	/* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24	(24)		/* WRITE_BLOCK */
#define CMD25	(25)		/* WRITE_MULTIPLE_BLOCK */
#define CMD32	(32)		/* ERASE_ER_BLK_START */
#define CMD33	(33)		/* ERASE_ER_BLK_END */
#define CMD38	(38)		/* ERASE */
#define CMD55	(55)		/* APP_CMD */
#define CMD58	(58)		/* READ_OCR */

#ifndef DEBUGLOG
#define DEBUGLOG(lvl, msg, ...)
#endif

//#define DEBUGLOG(DebugLevel_VVVDeepTrace, x) DEBUGLOG(DebugLevel_Info, x);

#ifndef HOSS_TIMEOUT_CHECKER
#define HOSS_TIMEOUT_CHECKER
class TimeoutChecker {
	uint32_t _start;
	uint32_t _duration;
	uint32_t _end;
public:
	TimeoutChecker(uint32_t timeoutMillis) : _start(0), _duration(timeoutMillis), _end(0) { start(); }
	operator int() const { uint32_t now = millis(); return _start < _end ? now > _end : now > _end && now < _start; }
	void start() { _start = millis(); _end = _start + _duration; }
};
#endif

template<typename Pin>
class SDSPIDriver : public FatFsDriver
{
private:
	SPIClass* _spi;
	uint16_t _cs;
	Pin _cd;
	volatile bool _cd_enabled;
	volatile bool _cd_active_low;
	Pin _wp;
	volatile bool _wp_enabled;
	volatile bool _wp_active_low;
	volatile uint32_t _high_speed_clock;
	volatile uint32_t _low_speed_clock;
	volatile uint32_t _active_clock;
	os_mutex_t _mutex;
	volatile DSTATUS _status;
	volatile BYTE _cardType;
	volatile bool _have_lock;
	volatile bool _busy;
	volatile bool _busy_check;

	os_mutex_t _dmaSignal;

	static std::function<void(void)> _dmaCallback[];
	static void spi_callback() { _dmaCallback[0](); }
	static void spi1_callback() { _dmaCallback[1](); }

	static HAL_SPI_DMA_UserCallback spiCallback(SPIClass* spi, std::function<void()> callback)
	{
		if(spi == &SPI)
		{
			_dmaCallback[0] = callback;
			return &spi_callback;
		}
		else if(spi == &SPI1)
		{
			_dmaCallback[1] = callback;
			return &spi1_callback;
		}
		else return nullptr;
	}


	void assertCS() { digitalWrite(_cs, LOW); }
	void deassertCS() { digitalWrite(_cs, HIGH); }

	void activateLowSpeed() { _active_clock = _low_speed_clock; }
	void activateHighSpeed() { _active_clock = _high_speed_clock; }

	void setSPI()
	{
		_spi->begin(SPI_MODE_MASTER, _cs);
		_spi->setClockSpeed(_active_clock, HZ);
		_spi->setDataMode(SPI_MODE0);
		_spi->setBitOrder(MSBFIRST);
	}

	HAL_SPI_DMA_UserCallback startTransfer() {
		os_mutex_lock(_dmaSignal);
		return spiCallback(_spi, [this](){this->transferComplete();});
	}
		void transferComplete() { os_mutex_unlock(_dmaSignal); };
		void waitForTransfer() { os_mutex_lock(_dmaSignal); os_mutex_unlock(_dmaSignal); }

	/* Send multiple byte */
	void xmit_spi_multi (
		const BYTE *buff,	/* Pointer to the data */
		UINT btx			/* Number of bytes to send (even number) */
	)
	{
		/* Write multiple bytes */
		_spi->transfer((void*)buff, nullptr, btx, startTransfer());
		waitForTransfer();
	}

	void rcvr_spi_multi (
		BYTE *buff,		/* Pointer to data buffer */
		UINT btr		/* Number of bytes to receive (even number) */
	)
	{
		/* Read multiple bytes, send 0xFF as dummy */
		memset(buff, 0xff, btr);


		_spi->transfer(buff, buff, btr, startTransfer());
		waitForTransfer();
	}

	BYTE send_cmd (		/* Return value: R1 resp (bit7==1:Failed to send) */
			BYTE cmd,		/* Command index */
			DWORD arg		/* Argument */
		)
		{
			BYTE n, res;

			if(!wait_ready(10))
				DEBUGLOG(DebugLevel_Error, "SD: wait_ready before cmd failed");

			if (cmd & 0x80) {	/* Send a CMD55 prior to ACMD<n> */
				cmd &= 0x7F;
				res = send_cmd(CMD55, 0);
				if (res > 1) {
		//			DEBUGLOG(DebugLevel_Error, "SD: CMD55 response 0x%x", res);
					return res;
				}
			}

			/* Select the card and wait for ready except to stop multiple block read */
			if (cmd != CMD12) {
				deselect();
				if (!select()) return 0xFF;
			}

			/* Send command packet */
			xmit_spi(0x40 | cmd);				/* Start + command index */
			xmit_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
			xmit_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
			xmit_spi((BYTE)(arg >> 8));			/* Argument[15..8] */
			xmit_spi((BYTE)arg);				/* Argument[7..0] */
			n = 0x01;							/* Dummy CRC + Stop */
			if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
			if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
			xmit_spi(n);

			/* Receive command resp */
			if (cmd == CMD12) {
				xmit_spi(0xFF);					/* Diacard following one byte when CMD12 */
			}

			n = 10;								/* Wait for response (10 bytes max) */
			do {
				res = xmit_spi(0xFF);
			} while ((res & 0x80) && --n);

			return res;							/* Return received response */
		}

	BYTE xmit_spi(const BYTE b)
	{
		BYTE result = _spi->transfer(b);
		return result;
	}

	int wait_ready (	/* 1:Ready, 0:Timeout */
		UINT wt			/* Timeout [ms] */
	)
	{
		BYTE d;

		TimeoutChecker timeout(wt);
	//	DEBUGLOG(DebugLevel_DeepTrace, "wait_ready %u - %u", start, end);

		do {
			d = xmit_spi(0xFF);
	//		if(d != 0xff)
	////			delay(1);
	//			os_thread_yield();
		} while (d != 0xFF && !timeout);	/* Wait for card goes ready or timeout */
		if (d == 0xFF) {
			DEBUGLOG(DebugLevel_VVVDeepTrace, "wait_ready: OK");
		} else {
			DEBUGLOG(DebugLevel_VVVDeepTrace, "wait_ready: timeout");
			DEBUGLOG(DebugLevel_Error, "SD: wait_ready timeout");
		}
		return (d == 0xFF) ? 1 : 0;
	}

	void deselect (void)
	{

		deassertCS();				/* CS = H */
		_spi->transfer(0xFF);		/* Dummy clock (force DO hi-z for multiple slave SPI) */
		DEBUGLOG(DebugLevel_VVVDeepTrace, "deselect: ok");
	}

	int select (void)	/* 1:OK, 0:Timeout */
	{
		assertCS();
		xmit_spi(0xFF);	/* Dummy clock (force DO enabled) */

		if (wait_ready(100)) {
			DEBUGLOG(DebugLevel_VVVDeepTrace, "select: OK");
			return 1;	/* OK */
		}
		DEBUGLOG(DebugLevel_VVVDeepTrace, "select: no");
		deselect();
		return 0;		/* Timeout */
	}

	int xmit_datablock (	/* 1:OK, 0:Failed */
		const BYTE *buff,	/* Ponter to 512 byte data to be sent */
		BYTE token			/* Token */
	)
	{
		BYTE resp;

		DEBUGLOG(DebugLevel_VVVDeepTrace, "xmit_datablock: inside");

		if (!wait_ready(100)) {
			DEBUGLOG(DebugLevel_VVVDeepTrace, "xmit_datablock: not ready");
			return 0;		/* Wait for card ready */
		}
		DEBUGLOG(DebugLevel_VVVDeepTrace, "xmit_datablock: ready");

		xmit_spi(token);					/* Send token */
		if (token != 0xFD) {				/* Send data if token is other than StopTran */
			xmit_spi_multi(buff, 512);		/* Data */
			xmit_spi(0xFF); xmit_spi(0xFF); /* Dummy CRC */
			resp = xmit_spi(0xFF);			/* Receive data resp */
			if ((resp & 0x1F) != 0x05)		/* Function fails if the data packet was not accepted */
				return 0;
		}
		return 1;
	}

	int rcvr_datablock (	/* 1:OK, 0:Error */
		BYTE *buff,			/* Data buffer */
		UINT btr			/* Data block length (byte) */
	)
	{
		BYTE token;

		TimeoutChecker timeout(200);
		do {							// Wait for DataStart token in timeout of 200ms
			token = xmit_spi(0xFF);
		} while ((token == 0xFF) && !timeout);
		if (token != 0xFE) {
			DEBUGLOG(DebugLevel_VVVDeepTrace, "rcvr_datablock: token != 0xFE");
			return 0;					// Function fails if invalid DataStart token or timeout
		}

		rcvr_spi_multi(buff, btr);		// Store trailing data to the buffer
		xmit_spi(0xFF);					// Discard CRC
		return 1;						// Function succeeded
	}
public:
	SDSPIDriver() : FatFsDriver(),
		_spi(nullptr),
		_cd_enabled(false),
		_cd_active_low(true),
		_wp_enabled(false),
		_wp_active_low(true),
		_high_speed_clock(15000000),
		_low_speed_clock(400000),
		_active_clock(_low_speed_clock),
		_mutex(nullptr),
		_status(STA_NOINIT),
		_cardType(0),
		_have_lock(false),
		_dmaSignal(nullptr),
		_busy(false),
		_busy_check(false) {}

	virtual void lock()
	{
		os_mutex_lock(_mutex);
		_busy = true;
		_busy_check = true;
		setSPI();
	}

	virtual void unlock()
	{
		_busy = false;
		os_mutex_unlock(_mutex);
	}

	os_mutex_t get_mutex() { return _mutex; };

	virtual ~SDSPIDriver() {}

	os_mutex_t begin(SPIClass& spi, const uint16_t cs, os_mutex_t mutex = nullptr)
	{
		_spi = &spi;
		_cs = cs;

		if(_mutex != nullptr)
			os_mutex_destroy(_mutex);
		_mutex = mutex;
		if(_mutex == nullptr)
			os_mutex_create(&_mutex);

		if(_dmaSignal == nullptr)
			os_mutex_create(&_dmaSignal);

		_have_lock = false;

		pinMode(cs, OUTPUT);
		digitalWrite(cs, HIGH);

		return _mutex;
	}

	virtual DSTATUS initialize() {
		UINT n, cmd, ty, ocr[4];
		std::lock_guard<decltype(*this)> lck(*this);

		activateLowSpeed();

		if (!cardPresent()) {
			return STA_NODISK;
		}

		for (n = 10; n; n--) {
			xmit_spi(0xFF);
		}

		ty = 0;
		TimeoutChecker timeout(1000);

		for(n = 0; n < 200; n++) {
			delay(1);
			send_cmd(CMD0, 0);
		}

		if (send_cmd(CMD0, 0) == 1) {				/* Put the card SPI/Idle state */
			DEBUGLOG(DebugLevel_Trace, "SD: CMD0 accepted");
			timeout.start();					/* Initialization timeout = 1 sec */
			if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2? */
				DEBUGLOG(DebugLevel_Trace, "SD: CMD8 accepted");
				for (n = 0; n < 4; n++) {
					ocr[n] = xmit_spi(0xFF);	/* Get 32 bit return value of R7 resp */
				}
				if (ocr[2] == 0x01 && ocr[3] == 0xAA) {					/* Is the card supports vcc of 2.7-3.6V? */
					DEBUGLOG(DebugLevel_Trace, "SD: CMD8 valid response");
					while (!timeout && send_cmd(ACMD41, 1UL << 30)) ;	/* Wait for end of initialization with ACMD41(HCS) */
					if (!timeout && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
						DEBUGLOG(DebugLevel_Trace, "SD: CMD58 accepted");
						for (n = 0; n < 4; n++) {
							ocr[n] = xmit_spi(0xFF);
						}
						ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* Card id SDv2 */
						DEBUGLOG(DebugLevel_Trace, "SD: card type %d", ty);
					} else {
						if(!timeout)
							DEBUGLOG(DebugLevel_Trace, "SD: CMD58 unexpected response");
					}
				} else {
					DEBUGLOG(DebugLevel_Error, "SD: CMD8 invalid response");
				}
			} else {	/* Not SDv2 card */
				DEBUGLOG(DebugLevel_Trace, "SD: not an SDv2 card");
				if (send_cmd(ACMD41, 0) <= 1) 	{	/* SDv1 or MMC? */
					DEBUGLOG(DebugLevel_Trace, "SD: SDv1");
					ty = CT_SD1; cmd = ACMD41;	/* SDv1 (ACMD41(0)) */
				} else {
					DEBUGLOG(DebugLevel_Trace, "SD: MMCv3");
					ty = CT_MMC; cmd = CMD1;	/* MMCv3 (CMD1(0)) */
				}
				while (!timeout && send_cmd(cmd, 0));			/* Wait for end of initialization */
				if (!timeout || send_cmd(CMD16, 512) != 0) {	/* Set block length: 512 */
					DEBUGLOG(DebugLevel_Error, "SD: unexpected response to CMD16");
					ty = 0;
				}
			}
		}
		else
			DEBUGLOG(DebugLevel_Error, "Did not receive response to CMD0");

		if(timeout)
			DEBUGLOG(DebugLevel_Error, "SD: timeout on initialize");

		_cardType = ty;	/* Card type */
		deselect();

		if (ty) {			/* OK */
			_status &= ~STA_NOINIT;	/* Clear STA_NOINIT flag */
		} else {			/* Failed */
			_status = STA_NOINIT;
			DEBUGLOG(DebugLevel_VVVDeepTrace, "Initialize failed");
		}

		if (!writeProtected()) {
			_status |= STA_PROTECT;
		} else {
			_status &= ~STA_PROTECT;
		}

		activateHighSpeed();

		return _status;
	}

	virtual DSTATUS status() {
		if (!cardPresent()) {
			_status = STA_NODISK;
		} else if (writeProtected()) {
			_status |= STA_PROTECT;
		} else {
			_status &= ~STA_PROTECT;
		}
		return _status;
	}

	virtual DRESULT read(BYTE* buff, DWORD sector, UINT count) {
		UINT read = 0;
		std::lock_guard<decltype(*this)> lck(*this);

		DEBUGLOG(DebugLevel_VVVDeepTrace, "disk_read: inside");
		if (!cardPresent() || (_status & STA_NOINIT))
			return RES_NOTRDY;

		if (!(_cardType & CT_BLOCK))
			sector *= 512;						/* LBA ot BA conversion (byte addressing cards) */

		while(count != 0) {						/* Single sector read */
			if(send_cmd(CMD17, sector) == 0)	/* READ_SINGLE_BLOCK */
			{
				if(rcvr_datablock(buff + 512 * read, 512)) {
					count--;
					read++;
				} else
					DEBUGLOG(DebugLevel_Error, "SD: Read failed for sector %d", sector);
			}
			else
				DEBUGLOG(DebugLevel_Error, "SD: CMD17 not accepted");
		}
		deselect();
		return count ? RES_ERROR : RES_OK;		/* Return result */
	}

	virtual DRESULT write(const BYTE* buff, DWORD sector, UINT count)
	{
		std::lock_guard<decltype(*this)> lck(*this);

		DEBUGLOG(DebugLevel_VVVDeepTrace, "disk_write: inside");
		if (!cardPresent())
			return RES_ERROR;
		if (writeProtected()) {
			DEBUGLOG(DebugLevel_VVVDeepTrace, "disk_write: Write protected!!!");
			return RES_WRPRT;
		}
		if (_status & STA_NOINIT)
			return RES_NOTRDY;	/* Check drive status */

		if (_status & STA_PROTECT)
			return RES_WRPRT;	/* Check write protect */

		if (!(_cardType & CT_BLOCK))
			sector *= 512;	/* LBA ==> BA conversion (byte addressing cards) */

		if (count == 1) {	/* Single sector write */
			if ((send_cmd(CMD24, sector) == 0)	/* WRITE_BLOCK */
				&& xmit_datablock(buff, 0xFE))
				count = 0;
		} else {				/* Multiple sector write */
			if (_cardType & CT_SDC) send_cmd(ACMD23, count);	/* Predefine number of sectors */
			if (send_cmd(CMD25, sector) == 0) {	/* WRITE_MULTIPLE_BLOCK */
				do {
					if (!xmit_datablock(buff, 0xFC)) {
						break;
					}
					buff += 512;
				} while (--count);
				if (!xmit_datablock(0, 0xFD)) {	/* STOP_TRAN token */
					count = 1;
				}
			}
		}
		deselect();
		return count ? RES_ERROR : RES_OK;	/* Return result */
	}

	virtual DRESULT ioctl(BYTE cmd, void* buff)
	{
		std::lock_guard<decltype(*this)> lck(*this);

		DRESULT res;
		BYTE n, csd[16];
		DWORD *dp, st, ed, csize;

		if (_status & STA_NOINIT) {
			return RES_NOTRDY;	/* Check if drive is ready */
		}
		if (!cardPresent()) {
			return RES_NOTRDY;
		}

		res = RES_ERROR;

		switch (cmd) {
		case CTRL_SYNC :		/* Wait for end of internal write process of the drive */
			if (select()) res = RES_OK;
			break;

		case GET_SECTOR_COUNT :	/* Get drive capacity in unit of sector (DWORD) */
			if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
				if ((csd[0] >> 6) == 1) {	/* SDC ver 2.00 */
					csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
					*(DWORD*)buff = csize << 10;
				} else {					/* SDC ver 1.XX or MMC ver 3 */
					n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
					csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
					*(DWORD*)buff = csize << (n - 9);
				}
				res = RES_OK;
			}
			break;

		case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (DWORD) */
			if (_cardType & CT_SD2) {	/* SDC ver 2.00 */
				if (send_cmd(ACMD13, 0) == 0) {	/* Read SD status */
					xmit_spi(0xFF);
					if (rcvr_datablock(csd, 16)) {				/* Read partial block */
						for (n = 64 - 16; n; n--) xmit_spi(0xFF);	/* Purge trailing data */

						*(DWORD*)buff = 16UL << (csd[10] >> 4);
						res = RES_OK;
					}
				}
			} else {					/* SDC ver 1.XX or MMC */
				if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {	/* Read CSD */
					if (_cardType & CT_SD1) {	/* SDC ver 1.XX */
						*(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
					} else {					/* MMC */
						*(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
					}
					res = RES_OK;
				}
			}
			break;

		case CTRL_ERASE_SECTOR :	/* Erase a block of sectors (used when _USE_ERASE == 1) */
			if (!(_cardType & CT_SDC)) break;				/* Check if the card is SDC */
			if (ioctl(MMC_GET_CSD, csd)) break;	/* Get CSD */
			if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;	/* Check if sector erase can be applied to the card */
			dp = (DWORD*)buff; st = dp[0]; ed = dp[1];				/* Load sector block */
			if (!(_cardType & CT_BLOCK)) {
				st *= 512; ed *= 512;
			}
			if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000))	/* Erase sector block */
				res = RES_OK;	/* FatFs does not check result of this command */
			break;

		default:
			res = RES_PARERR;
		}

		deselect();

		return res;
	}

	bool cardPresent() { return !_cd_enabled || (digitalRead(_cd) == _cd_active_low? LOW : HIGH); }
	bool writeProtected() { return _wp_enabled && (digitalRead(_wp) == _wp_active_low? LOW : HIGH); }

	uint32_t highSpeedClock() { return _high_speed_clock; }
	uint32_t lowSpeedClock() { return _low_speed_clock; }
	void highSpeedClock(uint32_t clock) { _high_speed_clock = clock; }
	void lowSpeedClock(uint32_t clock) { _low_speed_clock = clock; }
	uint32_t activeClock() { return _active_clock; }

	void enableCardDetect(const Pin& cdPin, bool activeLow = true) { _cd = cdPin; _cd_active_low = activeLow; _cd_enabled = true; }
	void enableWriteProtectDetect(const Pin& wpPin, bool activeLow = true) { _wp = wpPin; _wp_active_low = activeLow; _wp_enabled = true; }

	bool wasBusySinceLastCheck() {
		bool wasBusy;
		ATOMIC_BLOCK()
		{
			wasBusy = _busy_check;
			_busy_check = _busy;
		}
		return wasBusy;
	}
};

template<typename T>
std::function<void(void)> SDSPIDriver<T>::_dmaCallback[2];

#ifndef FATFS_PARTICLE_PINTYPE
#define FATFS_PARTICLE_PINTYPE uint16_t
#endif

typedef SDSPIDriver<FATFS_PARTICLE_PINTYPE> FatFsSD;

#endif
#endif

