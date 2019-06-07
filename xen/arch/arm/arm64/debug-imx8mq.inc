/*
 * xen/arch/arm/arm64/debug-imx8mq.inc
 *
 * IMX8MQ specific debug code.
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

#define UTXD            0x40
#define UTS             0xb4
#define UTS_TXEMPTY     3<<1

.macro early_uart_init xb, c
/* Uart has already been initialized by bootloader Firmware, for instance by TF-A */
.endm

/*
 * IMX8MQ UART wait UART to be ready to transmit
 * xb: register which contains the UART base address
 * c: scratch register
 */
.macro early_uart_ready xb c
1:
        ldr    w\c, [\xb, #UTS]        /* UART Test Register */
        tbz    w\c, #UTS_TXEMPTY, 1b       /* Check TXEMPTY bit */
.endm

/*
 * IMX8MQ UART transmit character
 * xb: register which contains the UART base address
 * wt: register which contains the character to transmit
 */
.macro early_uart_transmit xb wt
	str    \wt, [\xb, #UTXD]
.endm

/*
 * Local variables:
 * mode: ASM
 * indent-tabs-mode: nil
 * End:
 */
