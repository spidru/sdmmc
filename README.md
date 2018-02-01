# SDMMC

## Overview
SDMMC is a generic driver written in embedded C which provides the physical layer communication between a device and an MMC or SD card. Being generic, it can easily be made to work with any device.

Please note that this driver does not include a filesystem. However, SDMMC can be used by a filesystem service to act as the physical layer between the card and the device.

## Usage
To make use of the SDMMC driver, it is first required to define your own functions to read and write to the card (e.g. *sdcard_read()* and *sdcard_write()*), as well as a function to activate and deactivate the appropriate Slave Select (SS) line (required for SPI) (e.g. *sdcard_set_ss()*).

```C
struct sdmmc_dev sdcard =
{
	.read = sdcard_read,
	.set_ss = sdcard_set_ss,
	.write = sdcard_write
};
```

The card can then be initialised by calling the following function:

```C
status = SDMMC_Initialise(&sdcard);
```

## Limitations
- SDXC cards are not supported (yet)
- Only single block read/write functionalities are available