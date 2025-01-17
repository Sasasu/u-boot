/* SPDX-License-Identifier: (GPL-2.0+ or MIT) */
/*
 * Copyright (C) 2020 huangzhenwei@allwinnertech.com
 * Copyright (C) 2021 Samuel Holland <samuel@sholland.org>
 */

#ifndef _DT_BINDINGS_CLK_SUN8I_T113_CCU_H_
#define _DT_BINDINGS_CLK_SUN8I_T113_CCU_H_

#define CLK_PLL_CPUX            0
#define CLK_PLL_DDR0            1
#define CLK_PLL_PERIPH0_4X      2
#define CLK_PLL_PERIPH0_2X      3
#define CLK_PLL_PERIPH0_800M    4
#define CLK_PLL_PERIPH0         5
#define CLK_PLL_PERIPH0_DIV3    6
#define CLK_PLL_VIDEO0_4X       7
#define CLK_PLL_VIDEO0_2X       8
#define CLK_PLL_VIDEO0          9
#define CLK_PLL_VIDEO1_4X       10
#define CLK_PLL_VIDEO1_2X       11
#define CLK_PLL_VIDEO1          12
#define CLK_PLL_VE              13
#define CLK_PLL_AUDIO0_4X       14
#define CLK_PLL_AUDIO0_2X       15
#define CLK_PLL_AUDIO0          16
#define CLK_PLL_AUDIO1          17
#define CLK_PLL_AUDIO1_DIV2     18
#define CLK_PLL_AUDIO1_DIV5     19
#define CLK_CPUX                20
#define CLK_CPUX_AXI            21
#define CLK_CPUX_APB            22
#define CLK_PSI_AHB             23
#define CLK_APB0                24
#define CLK_APB1                25
#define CLK_MBUS                26
#define CLK_DE                  27
#define CLK_BUS_DE              28
#define CLK_DI                  29
#define CLK_BUS_DI              30
#define CLK_G2D                 31
#define CLK_BUS_G2D             32
#define CLK_CE                  33
#define CLK_BUS_CE              34
#define CLK_VE                  35
#define CLK_BUS_VE              36
#define CLK_BUS_DMA             37
#define CLK_BUS_MSGBOX0         38
#define CLK_BUS_MSGBOX1         39
#define CLK_BUS_MSGBOX2         40
#define CLK_BUS_SPINLOCK        41
#define CLK_BUS_HSTIMER         42
#define CLK_AVS                 43
#define CLK_BUS_DBG             44
#define CLK_BUS_PWM             45
#define CLK_BUS_IOMMU           46
#define CLK_DRAM                47
#define CLK_MBUS_DMA            48
#define CLK_MBUS_VE             49
#define CLK_MBUS_CE             50
#define CLK_MBUS_TVIN           51
#define CLK_MBUS_CSI            52
#define CLK_MBUS_G2D            53
#define CLK_BUS_DRAM            54
#define CLK_MMC0                55
#define CLK_MMC1                56
#define CLK_MMC2                57
#define CLK_BUS_MMC0            58
#define CLK_BUS_MMC1            59
#define CLK_BUS_MMC2            60
#define CLK_BUS_UART0           61
#define CLK_BUS_UART1           62
#define CLK_BUS_UART2           63
#define CLK_BUS_UART3           64
#define CLK_BUS_UART4           65
#define CLK_BUS_UART5           66
#define CLK_BUS_I2C0            67
#define CLK_BUS_I2C1            68
#define CLK_BUS_I2C2            69
#define CLK_BUS_I2C3            70
#define CLK_BUS_CAN0            71
#define CLK_BUS_CAN1            72
#define CLK_SPI0                73
#define CLK_SPI1                74
#define CLK_BUS_SPI0            75
#define CLK_BUS_SPI1            76
#define CLK_EMAC_25M            77
#define CLK_BUS_EMAC            78
#define CLK_IR_TX               79
#define CLK_BUS_IR_TX           80
#define CLK_BUS_GPADC           81
#define CLK_BUS_THS             82
#define CLK_I2S0                83
#define CLK_I2S1                84
#define CLK_I2S2                85
#define CLK_I2S2_ASRC           86
#define CLK_BUS_I2S0            87
#define CLK_BUS_I2S1            88
#define CLK_BUS_I2S2            89
#define CLK_SPDIF_TX            90
#define CLK_SPDIF_RX            91
#define CLK_BUS_SPDIF           92
#define CLK_DMIC                93
#define CLK_BUS_DMIC            94
#define CLK_AUDIO_DAC           95
#define CLK_AUDIO_ADC           96
#define CLK_BUS_AUDIO           97
#define CLK_USB_OHCI0           98
#define CLK_USB_OHCI1           99
#define CLK_BUS_OHCI0           100
#define CLK_BUS_OHCI1           101
#define CLK_BUS_EHCI0           102
#define CLK_BUS_EHCI1           103
#define CLK_BUS_OTG             104
#define CLK_BUS_LRADC           105
#define CLK_BUS_DPSS_TOP        106
#define CLK_HDMI_24M            107
#define CLK_HDMI_CEC_32K        108
#define CLK_HDMI_CEC            109
#define CLK_BUS_HDMI            110
#define CLK_MIPI_DSI            111
#define CLK_BUS_MIPI_DSI        112
#define CLK_TCON_LCD0           113
#define CLK_BUS_TCON_LCD0       114
#define CLK_TCON_TV             115
#define CLK_BUS_TCON_TV         116
#define CLK_TVE                 117
#define CLK_BUS_TVE_TOP         118
#define CLK_BUS_TVE             119
#define CLK_TVD                 120
#define CLK_BUS_TVD_TOP         121
#define CLK_BUS_TVD             122
#define CLK_LEDC                123
#define CLK_BUS_LEDC            124
#define CLK_CSI_TOP             125
#define CLK_CSI_MCLK            126
#define CLK_BUS_CSI             127
#define CLK_TPADC               128
#define CLK_BUS_TPADC           129
#define CLK_BUS_TZMA            130
#define CLK_DSP                 131
#define CLK_BUS_DSP_CFG         132
#define CLK_RISCV               133
#define CLK_RISCV_AXI           134
#define CLK_BUS_RISCV_CFG       135
#define CLK_FANOUT_24M          136
#define CLK_FANOUT_12M          137
#define CLK_FANOUT_16M          138
#define CLK_FANOUT_25M          139
#define CLK_FANOUT_32K          140
#define CLK_FANOUT_27M          141
#define CLK_FANOUT_PCLK         142
#define CLK_FANOUT0             143
#define CLK_FANOUT1             144
#define CLK_FANOUT2             145

#endif /* _DT_BINDINGS_CLK_SUN8I_T113_CCU_H_ */
