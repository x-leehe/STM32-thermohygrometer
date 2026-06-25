/**
 ******************************************************************************
 * @file    bsp_spi_flash.c
 * @brief   W25Q64 SPI Flash 底层驱动
 *          接口: SPI1 — PA4(CS), PA5(SCK), PA6(MISO), PA7(MOSI)
 ******************************************************************************
 */

#include "bsp_spi_flash.h"

static __IO uint32_t SPITimeout = SPIT_LONG_TIMEOUT;

static uint16_t SPI_TIMEOUT_UserCallback(uint8_t errorCode);

/**
 * @brief  SPI_FLASH 初始化
 */
void SPI_FLASH_Init(void)
{
    SPI_InitTypeDef  SPI_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能 SPI1 时钟 */
    RCC_APB2PeriphClockCmd(FLASH_SPI_CLK |
                           FLASH_SPI_CS_CLK |
                           FLASH_SPI_SCK_CLK |
                           FLASH_SPI_MISO_CLK |
                           FLASH_SPI_MOSI_CLK, ENABLE);

    /* CS：普通推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = FLASH_SPI_CS_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(FLASH_SPI_CS_PORT, &GPIO_InitStructure);

    /* SCK：复用推挽 */
    GPIO_InitStructure.GPIO_Pin  = FLASH_SPI_SCK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(FLASH_SPI_SCK_PORT, &GPIO_InitStructure);

    /* MISO：复用推挽（或浮空输入，SPI 复用时推挽也可） */
    GPIO_InitStructure.GPIO_Pin  = FLASH_SPI_MISO_PIN;
    GPIO_Init(FLASH_SPI_MISO_PORT, &GPIO_InitStructure);

    /* MOSI：复用推挽 */
    GPIO_InitStructure.GPIO_Pin  = FLASH_SPI_MOSI_PIN;
    GPIO_Init(FLASH_SPI_MOSI_PORT, &GPIO_InitStructure);

    SPI_FLASH_CS_HIGH();

    /* SPI 模式 3：CPOL=1, CPHA=1 */
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(FLASH_SPIx, &SPI_InitStructure);

    SPI_Cmd(FLASH_SPIx, ENABLE);
}

/**
 * @brief  擦除一个扇区（4KB）
 */
void SPI_FLASH_SectorErase(uint32_t SectorAddr)
{
    SPI_FLASH_WriteEnable();
    SPI_FLASH_WaitForWriteEnd();

    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_SectorErase);
    SPI_FLASH_SendByte((SectorAddr & 0xFF0000) >> 16);
    SPI_FLASH_SendByte((SectorAddr & 0xFF00) >> 8);
    SPI_FLASH_SendByte(SectorAddr & 0xFF);
    SPI_FLASH_CS_HIGH();

    SPI_FLASH_WaitForWriteEnd();
}

/**
 * @brief  全片擦除
 */
void SPI_FLASH_BulkErase(void)
{
    SPI_FLASH_WriteEnable();

    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_ChipErase);
    SPI_FLASH_CS_HIGH();

    SPI_FLASH_WaitForWriteEnd();
}

/**
 * @brief  页写入（单页，不超过 256 字节）
 */
void SPI_FLASH_PageWrite(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    SPI_FLASH_WriteEnable();

    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_PageProgram);
    SPI_FLASH_SendByte((WriteAddr & 0xFF0000) >> 16);
    SPI_FLASH_SendByte((WriteAddr & 0xFF00) >> 8);
    SPI_FLASH_SendByte(WriteAddr & 0xFF);

    if (NumByteToWrite > SPI_FLASH_PerWritePageSize)
        NumByteToWrite = SPI_FLASH_PerWritePageSize;

    while (NumByteToWrite--)
    {
        SPI_FLASH_SendByte(*pBuffer);
        pBuffer++;
    }

    SPI_FLASH_CS_HIGH();
    SPI_FLASH_WaitForWriteEnd();
}

/**
 * @brief  任意长度写入（自动跨页）
 */
void SPI_FLASH_BufferWrite(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    uint32_t Addr      = WriteAddr % SPI_FLASH_PageSize;
    uint32_t count     = SPI_FLASH_PageSize - Addr;
    uint32_t NumOfPage = NumByteToWrite / SPI_FLASH_PageSize;
    uint32_t NumOfSingle = NumByteToWrite % SPI_FLASH_PageSize;
    uint32_t temp;

    if (Addr == 0)
    {
        /* 地址已页对齐 */
        if (NumOfPage == 0)
            SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumByteToWrite);
        else
        {
            while (NumOfPage--)
            {
                SPI_FLASH_PageWrite(pBuffer, WriteAddr, SPI_FLASH_PageSize);
                WriteAddr += SPI_FLASH_PageSize;
                pBuffer   += SPI_FLASH_PageSize;
            }
            if (NumOfSingle)
                SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumOfSingle);
        }
    }
    else
    {
        /* 地址未对齐 */
        if (NumOfPage == 0)
        {
            if (NumOfSingle > count)
            {
                temp = NumOfSingle - count;
                SPI_FLASH_PageWrite(pBuffer, WriteAddr, count);
                WriteAddr += count;
                pBuffer   += count;
                SPI_FLASH_PageWrite(pBuffer, WriteAddr, temp);
            }
            else
                SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumByteToWrite);
        }
        else
        {
            NumByteToWrite -= count;
            NumOfPage       = NumByteToWrite / SPI_FLASH_PageSize;
            NumOfSingle     = NumByteToWrite % SPI_FLASH_PageSize;

            SPI_FLASH_PageWrite(pBuffer, WriteAddr, count);
            WriteAddr += count;
            pBuffer   += count;

            while (NumOfPage--)
            {
                SPI_FLASH_PageWrite(pBuffer, WriteAddr, SPI_FLASH_PageSize);
                WriteAddr += SPI_FLASH_PageSize;
                pBuffer   += SPI_FLASH_PageSize;
            }
            if (NumOfSingle)
                SPI_FLASH_PageWrite(pBuffer, WriteAddr, NumOfSingle);
        }
    }
}

/**
 * @brief  读取任意长度数据
 */
void SPI_FLASH_BufferRead(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
    SPI_FLASH_CS_LOW();

    SPI_FLASH_SendByte(W25X_ReadData);
    SPI_FLASH_SendByte((ReadAddr & 0xFF0000) >> 16);
    SPI_FLASH_SendByte((ReadAddr & 0xFF00) >> 8);
    SPI_FLASH_SendByte(ReadAddr & 0xFF);

    while (NumByteToRead--)
    {
        *pBuffer = SPI_FLASH_SendByte(Dummy_Byte);
        pBuffer++;
    }

    SPI_FLASH_CS_HIGH();
}

/**
 * @brief  读取 JEDEC ID
 */
uint32_t SPI_FLASH_ReadID(void)
{
    uint32_t Temp = 0, Temp0, Temp1, Temp2;

    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_JedecDeviceID);
    Temp0 = SPI_FLASH_SendByte(Dummy_Byte);
    Temp1 = SPI_FLASH_SendByte(Dummy_Byte);
    Temp2 = SPI_FLASH_SendByte(Dummy_Byte);
    SPI_FLASH_CS_HIGH();

    Temp = (Temp0 << 16) | (Temp1 << 8) | Temp2;
    return Temp;
}

/**
 * @brief  读取 Device ID
 */
uint32_t SPI_FLASH_ReadDeviceID(void)
{
    uint32_t Temp;

    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_DeviceID);
    SPI_FLASH_SendByte(Dummy_Byte);
    SPI_FLASH_SendByte(Dummy_Byte);
    SPI_FLASH_SendByte(Dummy_Byte);
    Temp = SPI_FLASH_SendByte(Dummy_Byte);
    SPI_FLASH_CS_HIGH();

    return Temp;
}

/**
 * @brief  SPI 收发一个字节
 */
uint8_t SPI_FLASH_SendByte(uint8_t byte)
{
    SPITimeout = SPIT_FLAG_TIMEOUT;
    while (SPI_I2S_GetFlagStatus(FLASH_SPIx, SPI_I2S_FLAG_TXE) == RESET)
    {
        if ((SPITimeout--) == 0) return SPI_TIMEOUT_UserCallback(0);
    }

    SPI_I2S_SendData(FLASH_SPIx, byte);

    SPITimeout = SPIT_FLAG_TIMEOUT;
    while (SPI_I2S_GetFlagStatus(FLASH_SPIx, SPI_I2S_FLAG_RXNE) == RESET)
    {
        if ((SPITimeout--) == 0) return SPI_TIMEOUT_UserCallback(1);
    }

    return SPI_I2S_ReceiveData(FLASH_SPIx);
}

/**
 * @brief  发送写使能
 */
void SPI_FLASH_WriteEnable(void)
{
    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_WriteEnable);
    SPI_FLASH_CS_HIGH();
}

/**
 * @brief  等待写入完成（轮询 BUSY 标志）
 */
void SPI_FLASH_WaitForWriteEnd(void)
{
    uint8_t FLASH_Status = 0;

    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_ReadStatusReg);

    do
    {
        FLASH_Status = SPI_FLASH_SendByte(Dummy_Byte);
    }
    while ((FLASH_Status & WIP_Flag) == SET);

    SPI_FLASH_CS_HIGH();
}

/**
 * @brief  进入掉电模式
 */
void SPI_Flash_PowerDown(void)
{
    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_PowerDown);
    SPI_FLASH_CS_HIGH();
}

/**
 * @brief  唤醒
 */
void SPI_Flash_WAKEUP(void)
{
    SPI_FLASH_CS_LOW();
    SPI_FLASH_SendByte(W25X_ReleasePowerDown);
    SPI_FLASH_CS_HIGH();
}

/**
 * @brief  SPI 超时回调
 */
static uint16_t SPI_TIMEOUT_UserCallback(uint8_t errorCode)
{
    (void)errorCode; /* SPI 超时，静默返回 */
    return 0;
}
