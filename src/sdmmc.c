#include "sdmmc.h"
#include <stdint.h>

/* Global Variables */
static uint16_t sdmmc_pos;
static uint32_t sdmmc_sector;
static uint8_t sdmmc_type;

/**
 * @brief	Continuously checks reply from card until expected reply is given or until timeout occurs.
 * @return	Error code value
 */
int16_t SDMMC_Response(struct sdmmc_dev *dev, uint8_t response)
{
	uint32_t count = 0;
	uint8_t result;
	while (count < SDMMC_MAX_TIMEOUT)
	{
		dev->read(&result, 1);

		if (result == response) break;
		count++;
	}
	if (count == SDMMC_MAX_TIMEOUT) return SDMMC_ERR_IDLE_STATE_TIMEOUT;	// failure: loop was exited due to timeout
	else return SDMMC_ERR_NONE;
}

uint8_t SDMMC_SendCMD(struct sdmmc_dev *dev, uint8_t cmd, uint32_t arg)
{
	uint8_t buffer[] = {arg>>24, arg>>16, arg>>8, arg};
	uint8_t data;
	uint8_t trail = 1;		// trailing byte (7-bit CRC + stop bit)
	uint8_t retry_count;

//	dev->set_ss(SDMMC_SS_HIGH);
//	dev->set_ss(SDMMC_SS_LOW);

	if (SDMMC_WaitUntilReady(dev) != 0xFF)
	{
		return SDMMC_ERR_IDLE_STATE_TIMEOUT;
	}

	if (cmd & 0x80)
	{
		/* Send CMD55 packet */
		data = SDMMC_CMD55;
		dev->write(&data, 1);
		data = 0;
		for (uint8_t i = 0; i < 4; i++)
		{
			dev->write(&data, 1);
		}
		dev->write(&trail, 1);

		retry_count = 10;
		do
		{
			dev->read(&data, 1);
		} while ((data & 0x80) && --retry_count);	// when the card is busy, MSB in R1 is 1
		if (data != 1) return data;
		cmd = cmd & 0x7F;	// prepare next command (below)
	}

	/* Send CMD packet */
	dev->write(&cmd, 1);
	dev->write(buffer, 4);
	if (cmd == SDMMC_CMD0) trail = 0x95;
	if (cmd == SDMMC_CMD8) trail = 0x87;
	dev->write(&trail, 1);

	if (cmd == SDMMC_CMD12) dev->read(&data, 1);	// skip a stuff byte when stop reading (?)

	retry_count = 10;
	do
	{
		dev->read(&data, 1);
	} while ((data & 0x80) && --retry_count);	// when the card is busy, MSB in R1 is 1
	return data;
}

uint8_t SDMMC_WaitUntilReady(struct sdmmc_dev *dev)
{
	uint8_t data;
	uint16_t timeout = 0x1FFF;

	do
	{
		dev->read(&data, 1);
	}
	while ((data != 0xFF) && (--timeout));

	return data;
}

/**
 * @brief	Checks for a R7-type response
 */
int16_t SDMMC_ResponseR7(struct sdmmc_dev *dev)
{
	int16_t ret;
	uint8_t data[4];
	uint32_t reply;

	ret = SDMMC_Response(dev, 0x01);
	if (ret != SDMMC_ERR_NONE) return ret;
	dev->read(data, 4);		// receive 32-bit reply
	reply = (uint32_t) (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
	if ((reply & 0xFFF) == 0x1AA)
		return SDMMC_CMD8_ACCEPTED;
	return SDMMC_CMD8_REJECTED;
}

int16_t SDMMC_ResponseR3(struct sdmmc_dev *dev)
{
	int16_t ret;
	uint8_t data[4];
	uint32_t reply;

	ret = SDMMC_Response(dev, 0x01);
	if (ret != SDMMC_ERR_NONE) return ret;
	dev->read(data, 4);		// receive 32-bit reply
	reply = (uint32_t) (data[0]<<24)|(data[1]<<16)|(data[2]<<8)|data[3];
	if (reply & 0x40000000)	// check if CCS (bit 30) = 1
		return 2;
	return 1;
}

/**
 * @brief	Initialises the SD card.
 * TODO check support for SDXC
 * @note	Adapted from: https://www.ccsinfo.com/forum/viewtopic.php?t=36866
 */
int16_t SDMMC_Initialise(struct sdmmc_dev *dev)
{
	uint8_t buffer[4];	// 32-bit buffer
	uint8_t retry_count;
	uint8_t cmd;

	// allowing 80 clock cycles for initialisation
	dev->set_ss(SDMMC_SS_HIGH);
	for (uint8_t i = 0; i < 10; i++)
	{
		dev->read(buffer, 1);
	}
	dev->set_ss(SDMMC_SS_LOW);

	// CMD0
	if (SDMMC_SendCMD(dev, SDMMC_CMD0, 0) == 1)
	{
		// CMD8
		if (SDMMC_SendCMD(dev, SDMMC_CMD8, 0x1AA) == 1)
		{
			// SDv2
			dev->read(buffer, 4);
			if (buffer[2] == 0x01 && buffer[3] == 0xAA)
			{
				// ACMD41
				retry_count = 10;
				while (--retry_count && SDMMC_SendCMD(dev, SDMMC_ACMD41, 1<<30) != 0x00);	// HCS=1 (host supports high capacity)
				// CMD58
				if (SDMMC_SendCMD(dev, SDMMC_CMD58, 0) == 0)
				{
					// Read OCR
					dev->read(buffer, 4);
					// Check CCS bit
					sdmmc_type = (buffer[0] & 0x40) ? (SDMMC_TYPE_SD2 | SDMMC_TYPE_BLOCK) : SDMMC_TYPE_SD2;	// SDv2 (HC or SC)
				}
			}
		}
		else
		{
			// SDv1 or MMCv3
			if (SDMMC_SendCMD(dev, SDMMC_ACMD41, 0) <= 1)
			{
				// SDv1
				sdmmc_type = SDMMC_TYPE_SD1;
				cmd = SDMMC_ACMD41;
			}
			else
			{
				// MMCv3
				sdmmc_type = SDMMC_TYPE_MMC;
				cmd = SDMMC_CMD1;
			}

			retry_count = 10;
			while (--retry_count && SDMMC_SendCMD(dev, cmd, 0));
			if ((retry_count == 0) || (SDMMC_SendCMD(dev, SDMMC_CMD16, SDMMC_SECTOR_SIZE) != 0))
			{
				// invalid card type
				sdmmc_type = 0;
			}
		}
	}

	dev->set_ss(SDMMC_SS_HIGH);
	if (sdmmc_type == 0) return SDMMC_ERR_INIT;
	sdmmc_sector = sdmmc_pos = 0;
	return SDMMC_ERR_NONE;
}

/**
 * Sends one of the commands from the SPI command set.
 * @deprecated
 */
void SDMMC_SendCommand(struct sdmmc_dev *dev, uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t readBytes)
{
	uint8_t i = 0;
	uint8_t buffer[] = {cmd,arg>>24,arg>>16,arg>>8,arg,crc};

	uint8_t resp = SDMMC_Response(dev, 0xFF);
	dev->write(buffer, 6);
}

/**
 * @brief	Reads the specified amount of bytes from the specified sector and offset on disk.
 * @note	Currently only supports single block read (i.e. can only read up to 512 bytes at a time).
 */
int8_t SDMMC_Read(struct sdmmc_dev *dev, uint32_t sector, uint16_t offset, uint8_t* buffer, uint32_t bytes)
{
	uint16_t count = 0;
	uint32_t address;
	uint8_t data_byte;
	uint8_t ret = SDMMC_ERR_READ_BLOCK_TIMEOUT;

	dev->set_ss(SDMMC_SS_LOW);

	/* SDSC/SDHC bytes/blocks addressing */
	if (sdmmc_type & SDMMC_TYPE_BLOCK)
	{
		// SDHC
		address = sector;
	}
	else
	{
		// SDSC
		address = sector << 9;	// sector<<9 never exceeds UINT32_MAX, since SDSC can only be at most 4GB
	}



	if (SDMMC_SendCMD(dev, SDMMC_CMD17, address) == 0x00)
	{
		// wait for start of data token (0xFE)
		count = 65535;	// FIXME this should give a timeout of around 100ms
		do {
			dev->read(&data_byte, 1);
		} while (data_byte == 0xFF && --count);
		if (data_byte == 0xFE)
		{
			// skip initial sector data
			count = 0;
			while (count < offset)
			{
				dev->read(&data_byte, 1);	// dummy read
				count++;
			}

			// read desired portion of sector data
			dev->read(buffer, bytes);
			count += bytes;
			count += offset;

			// skip remaining sector data + checksum
			while (count < 514)
			{
				dev->read(&data_byte, 1);	// dummy read
				count++;
			}

			ret = SDMMC_ERR_NONE;
		}
	}

	dev->set_ss(SDMMC_SS_HIGH);
	return ret;
}

/**
 * Currently only implements a single block write
 */
int16_t SDMMC_WriteBlock(struct sdmmc_dev *dev, uint32_t sector, const uint8_t* data, uint32_t length)
{
	uint16_t count = 0;
	uint32_t address;
	uint8_t temp;

	if (sector == 0) return SDMMC_ERR_ACCESS_DENIED;	// prevent writing to sector 0 TODO extend this to all boot sectors

	/* SDSC/SDHC bytes/blocks addressing */
	if (sdmmc_type & SDMMC_TYPE_BLOCK)
	{
		// SDHC
		address = sector;
	}
	else
	{
		// SDSC
		address = sector << 9;		// sector<<9 never exceeds UINT32_MAX, since SDSC can only be at most 4GB
	}

	dev->set_ss(SDMMC_SS_LOW);
	SDMMC_SendCommand(dev, SDMMC_CMD24,address,0xFF,8);
	if (SDMMC_Response(dev, 0x00) != SDMMC_ERR_NONE)	// 0x00 indicates successful command
	{
		dev->set_ss(SDMMC_SS_HIGH);
		return SDMMC_ERR_READ_BLOCK_TIMEOUT;
	}

	// send token
	temp = 0xFE;
	dev->write(&temp, 1);

	dev->write(data, length);

	// write checksum
	temp = 0xFF;
	dev->write(&temp, 1);
	dev->write(&temp, 1);

	if (SDMMC_Response(dev, 0xE5) != SDMMC_ERR_NONE)	// 0xE5 indicates a successful write request
	{
		dev->set_ss(SDMMC_SS_HIGH);
		return SDMMC_ERR_READ_BLOCK_TIMEOUT;
	}

	do
	{
		dev->read(&temp, 1);
	}
	while (temp == 0x00);	// wait for write to finish

	dev->set_ss(SDMMC_SS_HIGH);
	return 0;
}

//int16_t SDMMC_WriteSingleBlock(uint32_t sector, const uint8_t *data)
//{
//	uint16_t count = 0;
//	uint32_t address;
//	uint8_t checksum[2] = {0xFF,0xFF};
//	if (sector == 0) return SDMMC_ERR_ACCESS_DENIED;	// prevent writing to sector 0 TODO extend this to all boot sectors
//	/* SDSC/SDHC bytes/blocks addressing */
//	if (sdmmc_type == 1)
//		address = sector << 9;		// sector<<9 never exceeds UINT32_MAX, since SDSC can only be at most 4GB
//	else //if (sdmmc_type == 2)
//		address = sector;
//	SDMMC_SPI_CS_LOW();
//	SDMMC_SendCommand(CMD24,address,0xFF,8);
//	if (SDMMC_Response(0x00) != SDMMC_ERR_NONE)	// 0x00 indicates successful command
//	{
//		SDMMC_SPI_CS_HIGH();
//		return SDMMC_ERR_READ_BLOCK_TIMEOUT;
//	}
//	SDMMC_SPI_SEND(0xFE);
//	memcpy(g_spi_buffer,data,SDMMC_SECTOR_SIZE);
//	memcpy(&g_spi_buffer[SDMMC_SECTOR_SIZE],checksum,2);
//	SPI_DMA_SendBytes(SPI1,SDMMC_SECTOR_SIZE+2);
//	return SDMMC_SECTOR_SIZE+2;
//}
