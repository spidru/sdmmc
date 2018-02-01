#ifndef __SDMMC_H__
#define __SDMMC_H__

#include <stdint.h>

#define SDMMC_SECTOR_SIZE		((uint16_t) 512)

enum sdmmc_ss_level
{
	SDMMC_SS_LOW = 0,
	SDMMC_SS_HIGH
};

typedef int8_t (*sdmmc_comms_rw_t)(uint8_t *data, uint16_t data_size_bytes);
typedef void (*sdmmc_comms_ss_t)(enum sdmmc_ss_level);

struct sdmmc_dev
{
	sdmmc_comms_rw_t read;
	sdmmc_comms_rw_t write;
	sdmmc_comms_ss_t set_ss;
};

/* SDMMC SPI INTERFACE */
//#define SDMMC_SPI_SPIx			UOMR_SD_SPIx
//#define SDMMC_SPI_CS_GPIOx		GPIOA
//#define SDMMC_SPI_CS_GPIO_Pin	GPIO_Pin_4
//#define SDMMC_SPI_CS_HIGH() 	(GPIO_WriteBit(SDMMC_SPI_CS_GPIOx,SDMMC_SPI_CS_GPIO_Pin,Bit_SET))
//#define SDMMC_SPI_CS_LOW() 		(GPIO_WriteBit(SDMMC_SPI_CS_GPIOx,SDMMC_SPI_CS_GPIO_Pin,Bit_RESET))
//#define SDMMC_SPI_RECEIVE()		(SPI_ReceiveByte(SDMMC_SPI_SPIx))
//#define SDMMC_SPI_SEND(data)	(SPI_SendByte(SDMMC_SPI_SPIx,data))
/***********************************************************/
/* SPI Command Set */
#define SDMMC_CMD0		0x40	// software reset
#define SDMMC_CMD1		0x41	// initiate initialisation process
#define SDMMC_CMD8		0x48	// check voltage range (SDC v2 only)
#define SDMMC_CMD9		0x49	// read CSD register
#define SDMMC_CMD12		0x4C	// force stop transmission
#define SDMMC_CMD16		0x50	// set read/write block size
#define SDMMC_CMD17		0x51	// read a block
#define SDMMC_CMD18		0x52	// read multiple blocks
#define SDMMC_CMD24		0x58	// write a block
#define SDMMC_CMD25		0x59	// write multiple blocks
#define SDMMC_CMD41		0x69
#define SDMMC_CMD55		0x77	// leading command for ACMD commands
#define SDMMC_CMD58		0x7A	// read OCR
#define SDMMC_ACMD41	0xE9	// ACMDx = CMDx + 0x80
// (note: all commands are ORed with 64 [e.g. CMD16 = 16+64 = 0x50])
/***********************************************************/
/* SDMMC Card Type Definitions */
#define SDMMC_TYPE_MMC		0x01
#define SDMMC_TYPE_SD1		0x02								// SD v1
#define SDMMC_TYPE_SD2		0x04								// SD v2
#define SDMMC_TYPE_SDC		(SDMMC_TYPE_SD1|SDMMC_TYPE_SD2)		// SD
#define SDMMC_TYPE_BLOCK	0x08								// block addressing
/***********************************************************/
/* Error Codes */
#define SDMMC_ERR_NONE								+0
#define SDMMC_ERR_INIT								-1
#define SDMMC_ERR_READ_BLOCK_DATA_TOKEN_MISSING		-5
#define SDMMC_ERR_BAD_REPLY							-6
#define SDMMC_ERR_ACCESS_DENIED						-7
#define SDMMC_ERR_IDLE_STATE_TIMEOUT				-10
#define SDMMC_ERR_OP_COND_TIMEOUT					-11
#define SDMMC_ERR_SET_BLOCKLEN_TIMEOUT				-12
#define SDMMC_ERR_READ_BLOCK_TIMEOUT				-13
#define SDMMC_ERR_APP_CMD_TIMEOUT					-14
#define SDMMC_ERR_APP_SEND_IF_COND_TIMEOUT			-15

#define SDMMC_WAIT_MS								1
#define SDMMC_CMD8_ACCEPTED							2
#define SDMMC_CMD8_REJECTED							3
#define SDMMC_MAX_TIMEOUT							1024		// TODO arbitrary value; might need revising

int16_t SDMMC_Initialise(struct sdmmc_dev *dev);
int16_t SDMMC_Response(struct sdmmc_dev *dev, uint8_t response);
int16_t SDMMC_ResponseR3(struct sdmmc_dev *dev);
int16_t SDMMC_ResponseR7(struct sdmmc_dev *dev);
uint8_t SDMMC_SendCMD(struct sdmmc_dev *dev, uint8_t cmd, uint32_t arg);
void SDMMC_SendCommand(struct sdmmc_dev *dev, uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t readBytes);
int8_t SDMMC_Read(struct sdmmc_dev *dev, uint32_t sector, uint16_t offset, uint8_t* buffer, uint32_t bytes);
uint8_t SDMMC_WaitUntilReady(struct sdmmc_dev *dev);
int16_t SDMMC_WriteBlock(struct sdmmc_dev *dev, uint32_t sector, const uint8_t* data, uint32_t length);

#endif
