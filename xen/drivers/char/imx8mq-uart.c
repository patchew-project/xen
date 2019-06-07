/*
 * xen/drivers/char/imx8mq-uart.c
 *
 * Driver for i.MX8MQ UART.
 *
 * Copyright (c) 2019, Amit Singh Tomar <amittomer25@gmail.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms and conditions of the GNU General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include <xen/irq.h>
#include <xen/serial.h>
#include <xen/vmap.h>
#include <asm/io.h>
#include <xen/delay.h>
#include <xen/console.h>

/* Register definitions */
#define URXD    0x0  /* Receiver Register */
#define UTXD    0x40 /* Transmitter Register */
#define UCR1    0x80 /* Control Register 1 */
#define UCR2    0x84 /* Control Register 2 */
#define UCR4    0x8c /* Control Register 4 */
#define USR1    0x94 /* Status Register 1 */
#define USR2    0x98 /* Status Register 2 */
#define UBIR    0xa4 /* BRM Incremental Register */
#define UBMR    0xa8 /* BRM Modulator Register */
#define UTS     0xb4 /* UART Test Register */

/* UART Control Register Bit Fields */
#define  UCR1_ADEN      (1<<15) /* Auto dectect interrupt */
#define  UCR1_TRDYEN    (1<<13) /* Transmitter ready interrupt enable */
#define  UCR1_IDEN      (1<<12) /* Idle condition interrupt */
#define  UCR1_RRDYEN    (1<<9)  /* Recv ready interrupt enable */
#define  UCR1_TXMPTYEN  (1<<6)  /* Transimitter empty interrupt enable */
#define  UCR1_RTSDEN    (1<<5)  /* RTS delta interrupt enable */
#define  UCR1_TDMAEN    (1<<3)  /* Transmitter ready DMA enable */
#define  UCR1_UARTEN    (1<<0)  /* UART enabled */

#define UCR2_TXEN       (1<<2)  /* Transmitter enabled */
#define UCR2_RXEN       (1<<1)  /* Receiver enabled */
#define UCR2_IRTS       (1<<14) /* Ignore RTS pin */
#define UCR2_SRST       (1<<0)  /* SW reset */
#define UCR2_WS         (1<<5)  /* Word size */
#define UCR4_TCEN       (1<<3)  /* Transmit complete interrupt enable */
#define UCR4_DREN       (1<<0)  /* Recv data ready interrupt enable */

#define UTS_TXEMPTY     (1<<6)  /* TxFIFO empty */
#define UTS_RXEMPTY     (1<<5)  /* RxFIFO empty */
#define UTS_TXFULL      (1<<4)  /* TxFIFO full */

#define USR1_TRDY       (1<<13) /* Transmitter ready interrupt/dma flag */
#define USR1_RRDY       (1<<9)  /* Receiver ready interrupt/dma flag */

#define USR2_TXDC       (1<<3)  /* Transmitter complete */

#define TXFIFO_SIZE     32

#define setbits(addr, set)      writel((readl(addr) | (set)), (addr))
#define clrbits(addr, clear)    writel((readl(addr) & ~(clear)), (addr))

static struct imx8mq_uart {
    unsigned int irq;
    void __iomem *regs;
    struct irqaction irqaction;
    struct vuart_info vuart;
} imx8mq_com;

static void imx8mq_uart_interrupt(int irq, void *data,
                                     struct cpu_user_regs *regs)
{
    struct serial_port *port = data;
    struct imx8mq_uart *uart = port->uart;
    uint32_t st1 = readl(uart->regs + USR1);
    uint32_t st2 = readl(uart->regs + USR2);

    if ( st1 & (USR1_RRDY) )
        serial_rx_interrupt(port, regs);

    if ( (st1 & USR1_TRDY) || (st2 & USR2_TXDC) )
        serial_tx_interrupt(port, regs);
}

static void set_baudrate(struct imx8mq_uart *uart)
{
    /* Needed for automatic baud rate detection */
    writel(0xf, uart->regs + UBIR);
    writel((25000000 / (2 * 115200)), uart->regs + UBMR);
    writel((UCR2_WS | UCR2_IRTS | UCR2_RXEN | UCR2_TXEN | UCR2_SRST),
            uart->regs + UCR2);   
    writel(UCR1_UARTEN, uart->regs + UCR1);
}

static void __init imx8mq_uart_init_preirq(struct serial_port *port)
{
    struct imx8mq_uart *uart = port->uart;

    clrbits(uart->regs + UCR1,
            UCR1_ADEN | UCR1_IDEN | UCR1_RRDYEN | UCR1_RTSDEN);

    /* Disable receiver */
    clrbits(uart->regs + UCR2, UCR2_RXEN);
}

static void __init imx8mq_uart_init_postirq(struct serial_port *port)
{
    struct imx8mq_uart *uart = port->uart;

    uart->irqaction.handler = imx8mq_uart_interrupt;
    uart->irqaction.name    = "imx8mq_uart";
    uart->irqaction.dev_id  = port;

    if ( setup_irq(uart->irq, 0, &uart->irqaction) != 0 )
    {
        printk("Failed to allocated imx8mq_uart IRQ %d\n", uart->irq);
        return;
    }

    setbits(uart->regs + UCR1,
            UCR1_ADEN | UCR1_TRDYEN | UCR1_IDEN 
            | UCR1_RRDYEN | UCR1_TXMPTYEN | UCR1_RTSDEN);
    /* Enable receiver */
    setbits(uart->regs + UCR2, UCR2_RXEN);
    /* Generally we do soft reset in preirq stage but here reset does empty the
       Tx FIFO and triggers Tx interrupt which should be enabled by now
    */
    clrbits(uart->regs + UCR2, UCR2_SRST);
    while (!(readl(uart->regs + UCR2) & UCR2_SRST))
    ;

    set_baudrate(uart);
}

static void imx8mq_uart_suspend(struct serial_port *port)
{
    BUG();
}

static void imx8mq_uart_resume(struct serial_port *port)
{
    BUG();
}

static void imx8mq_uart_putc(struct serial_port *port, char c)
{
    struct imx8mq_uart *uart = port->uart;

    writel(c, uart->regs + UTXD);
}

static int imx8mq_uart_tx_ready(struct serial_port *port)
{
    struct imx8mq_uart *uart = port->uart;
    uint32_t reg;

    reg = readl(uart->regs + UTS);

    if( reg & UTS_TXEMPTY)
        return TXFIFO_SIZE;
    if ( reg & UTS_TXFULL)
        return 0;

    return 1;
}

static void imx8mq_uart_stop_tx(struct serial_port *port)
{

    struct imx8mq_uart *uart = port->uart;
    
    setbits(uart->regs + UCR1, UCR1_RRDYEN);
    clrbits(uart->regs + UCR1, (UCR1_TXMPTYEN | UCR1_TDMAEN));
    clrbits(uart->regs + UCR4, UCR4_TCEN);
}

static void imx8mq_uart_start_tx(struct serial_port *port)
{
    struct imx8mq_uart *uart = port->uart;

    clrbits(uart->regs + UCR4, UCR4_DREN);
    clrbits(uart->regs + UCR1, UCR1_TDMAEN | UCR1_RRDYEN);
    setbits(uart->regs + UCR2, UCR2_TXEN | UCR2_RXEN);
    setbits(uart->regs + UCR1, UCR1_TXMPTYEN);
}


static int __init imx8mq_irq(struct serial_port *port)
{
    struct imx8mq_uart *uart = port->uart;

    return uart->irq;
}

static const struct vuart_info *imx8mq_vuart_info(struct serial_port *port)
{
    struct imx8mq_uart *uart = port->uart;

    return &uart->vuart;
}

static int imx8mq_uart_getc(struct serial_port *port, char *c)
{
    struct imx8mq_uart *uart = port->uart;

    if ( (readl(uart->regs + UTS) & UTS_RXEMPTY) )
        return 0;

    *c = readl(uart->regs + URXD) & 0xff;

    return 1;
}

static struct uart_driver __read_mostly imx8mq_uart_driver = {
    .init_preirq  = imx8mq_uart_init_preirq,
    .init_postirq = imx8mq_uart_init_postirq,
    .endboot      = NULL,
    .suspend      = imx8mq_uart_suspend,
    .resume       = imx8mq_uart_resume,
    .putc         = imx8mq_uart_putc,
    .getc         = imx8mq_uart_getc,
    .tx_ready     = imx8mq_uart_tx_ready,
    .stop_tx      = imx8mq_uart_stop_tx,
    .start_tx     = imx8mq_uart_start_tx,
    .irq          = imx8mq_irq,
    .vuart_info   = imx8mq_vuart_info,
};

static int __init imx8mq_uart_init(struct dt_device_node *dev, const void *data)
{
    const char *config = data;
    struct imx8mq_uart *uart;
    int res;
    u64 addr, size;

    if ( strcmp(config, "") )
        printk("WARNING: UART configuration is not supported\n");

    uart = &imx8mq_com;

    res = dt_device_get_address(dev, 0, &addr, &size);
    if ( res )
    {
        printk("imx8mq3700: Unable to retrieve the base address of the UART\n");
        return res;
    }

    res = platform_get_irq(dev, 0);
    if ( res < 0 )
    {
        printk("imx8mq: Unable to retrieve the IRQ\n");
        return -EINVAL;
    }

    uart->irq  = res;

    uart->regs = ioremap_nocache(addr, size);
    if ( !uart->regs )
    {
        printk("imx8mq3700: Unable to map the UART memory\n");
        return -ENOMEM;
    }
   
    uart->vuart.base_addr = addr;
    uart->vuart.size = size;
    uart->vuart.data_off = UCR1;
    uart->vuart.status_off = UTS;
    uart->vuart.status = USR1;

    /* Register with generic serial driver. */
    serial_register_uart(SERHND_DTUART, &imx8mq_uart_driver, uart);

    dt_device_set_used_by(dev, DOMID_XEN);

    return 0;
}

static const struct dt_device_match imx8mq_dt_match[] __initconst =
{
    DT_MATCH_COMPATIBLE("fsl,imx6q-uart"),
    DT_MATCH_COMPATIBLE("fsl,imx8mq-uart"),
    { /* sentinel */ },
};

DT_DEVICE_START(imx8mq, "NXP imx8mq UART", DEVICE_SERIAL)
    .dt_match = imx8mq_dt_match,
    .init = imx8mq_uart_init,
DT_DEVICE_END

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
