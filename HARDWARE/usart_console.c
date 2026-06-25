/**
 ******************************************************************************
 * @file    usart_console.c
 * @brief   USART1 串口控制台 — printf 重定向
 *
 * 引脚: PA9(TX), PA10(RX) — 板载 CH340 USB 转串口
 * 用 USB 线连接开发板 USB-UART 口，PC 端打开串口助手 (115200 8N1)
 ******************************************************************************
 */

#include "usart_console.h"
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

/* ========================== USART1 初始化 ========================== */

/**
 * @brief  初始化 USART1: 115200 8-N-1
 */
void USART_Console_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    /* 使能 GPIOA 和 USART1 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* PA9 — USART1_TX：复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA10 — USART1_RX：浮空输入 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* USART1 基础配置 */
    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    /* 使能 USART1 */
    USART_Cmd(USART1, ENABLE);
}

/* ========================== printf 重定向 ========================== */

/**
 * @brief  实现 _write_r — newlib-nano 的底层输出系统调用
 *
 * printf 系列函数最终会调用此函数将数据发送到 USART1。
 * 文件描述符 fd=1(stdout) 和 fd=2(stderr) 都重定向到串口。
 */
int _write_r(struct _reent *r, int fd, const void *buf, size_t cnt)
{
    size_t i;
    const char *p = (const char *)buf;

    (void)r;

    /* stdout(1) 和 stderr(2) → USART1 */
    if (fd != 1 && fd != 2)
    {
        r->_errno = EBADF;
        return -1;
    }

    for (i = 0; i < cnt; i++)
    {
        /* 等待发送数据寄存器空 */
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
            ;
        USART_SendData(USART1, p[i]);
    }

    return (int)cnt;
}

/* ========================== 其他必要桩函数 ========================== */

/**
 * @brief  _close — 嵌入式环境无文件系统，直接返回成功
 */
int _close_r(struct _reent *r, int fd)
{
    (void)r;
    (void)fd;
    return 0;
}

/**
 * @brief  _lseek — 嵌入式环境不支持，返回错误
 */
off_t _lseek_r(struct _reent *r, int fd, off_t pos, int whence)
{
    (void)r;
    (void)fd;
    (void)pos;
    (void)whence;
    r->_errno = ESPIPE;
    return (off_t)-1;
}

/**
 * @brief  _read — 嵌入式环境不支持标准输入
 */
ssize_t _read_r(struct _reent *r, int fd, void *buf, size_t cnt)
{
    (void)r;
    (void)fd;
    (void)buf;
    (void)cnt;
    r->_errno = EBADF;
    return -1;
}

/**
 * @brief  _fstat — 嵌入式环境不支持文件状态
 */
int _fstat_r(struct _reent *r, int fd, struct stat *st)
{
    (void)r;
    (void)fd;
    (void)st;
    r->_errno = EBADF;
    return -1;
}

/**
 * @brief  _isatty — 返回 1 表示 stdout 是终端（启用行缓冲）
 */
int _isatty_r(struct _reent *r, int fd)
{
    (void)r;
    if (fd == 1 || fd == 2)
        return 1;
    r->_errno = EBADF;
    return 0;
}
