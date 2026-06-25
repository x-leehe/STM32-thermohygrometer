#ifndef __BSP_SPI_FLASH_H
#define __BSP_SPI_FLASH_H

#include "stm32f10x.h"

//////////////////////////////////////////////////////////////////////////////////
// W25Q64 SPI 串行 Flash 驱动
// 接口: SPI1 — PA4(CS), PA5(SCK), PA6(MISO), PA7(MOSI)
//////////////////////////////////////////////////////////////////////////////////

#define sFLASH_ID                    0xEF4017    // W25Q64 JEDEC ID

#define SPI_FLASH_PageSize           256
#define SPI_FLASH_SectorSize         4096        // 4KB per sector
#define SPI_FLASH_PerWritePageSize   256

/* W25Q64 命令 */
#define W25X_WriteEnable             0x06
#define W25X_WriteDisable            0x04
#define W25X_ReadStatusReg           0x05
#define W25X_WriteStatusReg          0x01
#define W25X_ReadData                0x03
#define W25X_FastReadData            0x0B
#define W25X_PageProgram             0x02
#define W25X_BlockErase              0xD8
#define W25X_SectorErase             0x20
#define W25X_ChipErase               0xC7
#define W25X_PowerDown               0xB9
#define W25X_ReleasePowerDown        0xAB
#define W25X_DeviceID                0xAB
#define W25X_ManufactDeviceID        0x90
#define W25X_JedecDeviceID           0x9F

#define WIP_Flag                     0x01
#define Dummy_Byte                   0xFF

/* SPI 引脚定义 — 对应野火 MINI 板载 W25Q64 */
#define FLASH_SPIx                   SPI1
#define FLASH_SPI_CLK                RCC_APB2Periph_SPI1

#define FLASH_SPI_CS_PORT            GPIOA
#define FLASH_SPI_CS_PIN             GPIO_Pin_4
#define FLASH_SPI_CS_CLK             RCC_APB2Periph_GPIOA

#define FLASH_SPI_SCK_PORT           GPIOA
#define FLASH_SPI_SCK_PIN            GPIO_Pin_5
#define FLASH_SPI_SCK_CLK            RCC_APB2Periph_GPIOA

#define FLASH_SPI_MISO_PORT          GPIOA
#define FLASH_SPI_MISO_PIN           GPIO_Pin_6
#define FLASH_SPI_MISO_CLK           RCC_APB2Periph_GPIOA

#define FLASH_SPI_MOSI_PORT          GPIOA
#define FLASH_SPI_MOSI_PIN           GPIO_Pin_7
#define FLASH_SPI_MOSI_CLK           RCC_APB2Periph_GPIOA

#define SPI_FLASH_CS_LOW()           GPIO_ResetBits(FLASH_SPI_CS_PORT, FLASH_SPI_CS_PIN)
#define SPI_FLASH_CS_HIGH()          GPIO_SetBits(FLASH_SPI_CS_PORT, FLASH_SPI_CS_PIN)

/* 超时 */
#define SPIT_FLAG_TIMEOUT            ((uint32_t)0x1000)
#define SPIT_LONG_TIMEOUT            ((uint32_t)(10 * SPIT_FLAG_TIMEOUT))

/* ========================== 对外接口 ========================== */
void SPI_FLASH_Init(void);
void SPI_FLASH_SectorErase(uint32_t SectorAddr);
void SPI_FLASH_BulkErase(void);
void SPI_FLASH_PageWrite(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void SPI_FLASH_BufferWrite(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void SPI_FLASH_BufferRead(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
uint32_t SPI_FLASH_ReadID(void);
uint32_t SPI_FLASH_ReadDeviceID(void);
void SPI_Flash_PowerDown(void);
void SPI_Flash_WAKEUP(void);

uint8_t SPI_FLASH_SendByte(uint8_t byte);
void SPI_FLASH_WriteEnable(void);
void SPI_FLASH_WaitForWriteEnd(void);

#endif /* __BSP_SPI_FLASH_H */
