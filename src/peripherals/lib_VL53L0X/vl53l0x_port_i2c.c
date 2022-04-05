
#include "sapphire.h"

#include "vl53l0x_platform.h"
#include "vl53l0x_i2c_platform.h"

/** int  VL53L0X_I2CWrite(VL53L0X_Dev_t dev, void *buff, uint8_t len);
 * @brief       Write data buffer to VL53L0 device via i2c
 * @param dev   The device to write to
 * @param buff  The data buffer
 * @param len   The length of the transaction in byte
 * @return      0 on success
 */
int VL53L0X_I2CWrite(VL53L0X_DEV dev, uint8_t *buff, uint8_t len)
{

    i2c_v_write( dev->I2cDevAddr, buff, len );

	return 0;
}


/** int VL53L0X_I2CRead(VL53L0X_Dev_t dev, void *buff, uint8_t len);
 * @brief       Read data buffer from VL53L0 device via i2c
 * @param dev   The device to read from
 * @param buff  The data buffer to fill
 * @param len   The length of the transaction in byte
 * @return      transaction status
 */
int VL53L0X_I2CRead(VL53L0X_DEV dev, uint8_t *buff, uint8_t len)
{

    i2c_v_read( dev->I2cDevAddr, buff, len );

	return 0;
}
