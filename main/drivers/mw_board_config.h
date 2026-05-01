#ifndef MW_BOARD_CONFIG_H
#define MW_BOARD_CONFIG_H

#include "sdkconfig.h"

/* I2C device addresses from schematic notes */
#define MW_ES8311_I2C_ADDR         0x30
#define MW_SC7A20_I2C_ADDR         0x19

/* SC7A20 register map (subset) */
#define MW_SC7A20_REG_WHO_AM_I     0x0F
#define MW_SC7A20_REG_CTRL1        0x20
#define MW_SC7A20_REG_CTRL4        0x23
#define MW_SC7A20_REG_OUT_X_L      0x28

/* 25Q128 command set (subset) */
#define MW_W25Q128_CMD_JEDEC_ID    0x9F

#endif /* MW_BOARD_CONFIG_H */
