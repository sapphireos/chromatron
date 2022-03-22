/*
 *  stmvl53l0x_module.c - Linux kernel modules for STM VL53L0 FlightSense TOF
 *						 sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
/*
 * API includes
 */
#include "vl53l0x_api.h"

//#define USE_INT
#define IRQ_NUM	   59
//#define DEBUG_TIME_LOG
#ifdef DEBUG_TIME_LOG
struct timeval start_tv, stop_tv;
#endif

/*
 * Global data
 */

#ifdef CAMERA_CCI
static struct stmvl53l0x_module_fn_t stmvl53l0x_module_func_tbl = {
	.init = stmvl53l0x_init_cci,
	.deinit = stmvl53l0x_exit_cci,
	.power_up = stmvl53l0x_power_up_cci,
	.power_down = stmvl53l0x_power_down_cci,
};
#else
static struct stmvl53l0x_module_fn_t stmvl53l0x_module_func_tbl = {
	.init = stmvl53l0x_init_i2c,
	.deinit = stmvl53l0x_exit_i2c,
	.power_up = stmvl53l0x_power_up_i2c,
	.power_down = stmvl53l0x_power_down_i2c,
};
#endif
struct stmvl53l0x_module_fn_t *pmodule_func_tbl;

struct stmvl53l0x_api_fn_t {
	int8_t (*GetVersion)(VL53L0X_Version_t *pVersion);
	int8_t (*GetPalSpecVersion)(VL53L0X_Version_t *pPalSpecVersion);

	int8_t (*GetProductRevision)(VL53L0X_DEV Dev,
					uint8_t *pProductRevisionMajor,
					uint8_t *pProductRevisionMinor);
	int8_t (*GetDeviceInfo)(VL53L0X_DEV Dev,
				VL53L0X_DeviceInfo_t *pVL53L0X_DeviceInfo);
	int8_t (*GetDeviceErrorStatus)(VL53L0X_DEV Dev,
				VL53L0X_DeviceError *pDeviceErrorStatus);
	int8_t (*GetRangeStatusString)(uint8_t RangeStatus,
				char *pRangeStatusString);
	int8_t (*GetDeviceErrorString)(VL53L0X_DeviceError ErrorCode,
				char *pDeviceErrorString);
	int8_t (*GetPalErrorString)(VL53L0X_Error PalErrorCode,
				char *pPalErrorString);
	int8_t (*GetPalStateString)(VL53L0X_State PalStateCode,
				char *pPalStateString);
	int8_t (*GetPalState)(VL53L0X_DEV Dev,	VL53L0X_State *pPalState);
	int8_t (*SetPowerMode)(VL53L0X_DEV Dev,
				VL53L0X_PowerModes PowerMode);
	int8_t (*GetPowerMode)(VL53L0X_DEV Dev,
				VL53L0X_PowerModes *pPowerMode);
	int8_t (*SetOffsetCalibrationDataMicroMeter)(VL53L0X_DEV Dev,
				int32_t OffsetCalibrationDataMicroMeter);
	int8_t (*GetOffsetCalibrationDataMicroMeter)(VL53L0X_DEV Dev,
				int32_t *pOffsetCalibrationDataMicroMeter);
	int8_t (*SetLinearityCorrectiveGain)(VL53L0X_DEV Dev,
				int16_t LinearityCorrectiveGain);
	int8_t (*GetLinearityCorrectiveGain)(VL53L0X_DEV Dev,
				uint16_t *pLinearityCorrectiveGain);
	int8_t (*SetGroupParamHold)(VL53L0X_DEV Dev,
				uint8_t GroupParamHold);
	int8_t (*GetUpperLimitMilliMeter)(VL53L0X_DEV Dev,
				uint16_t *pUpperLimitMilliMeter);
	int8_t (*SetDeviceAddress)(VL53L0X_DEV Dev,
				uint8_t DeviceAddress);
	int8_t (*DataInit)(VL53L0X_DEV Dev);
	int8_t (*SetTuningSettingBuffer)(VL53L0X_DEV Dev,
				uint8_t *pTuningSettingBuffer,
				uint8_t UseInternalTuningSettings);
	int8_t (*GetTuningSettingBuffer)(VL53L0X_DEV Dev,
				uint8_t **pTuningSettingBuffer,
				uint8_t *pUseInternalTuningSettings);
	int8_t (*StaticInit)(VL53L0X_DEV Dev);
	int8_t (*WaitDeviceBooted)(VL53L0X_DEV Dev);
	int8_t (*ResetDevice)(VL53L0X_DEV Dev);
	int8_t (*SetDeviceParameters)(VL53L0X_DEV Dev,
				const VL53L0X_DeviceParameters_t *pDeviceParameters);
	int8_t (*GetDeviceParameters)(VL53L0X_DEV Dev,
				VL53L0X_DeviceParameters_t *pDeviceParameters);
	int8_t (*SetDeviceMode)(VL53L0X_DEV Dev,
				VL53L0X_DeviceModes DeviceMode);
	int8_t (*GetDeviceMode)(VL53L0X_DEV Dev,
				VL53L0X_DeviceModes *pDeviceMode);
	int8_t (*SetHistogramMode)(VL53L0X_DEV Dev,
				VL53L0X_HistogramModes HistogramMode);
	int8_t (*GetHistogramMode)(VL53L0X_DEV Dev,
				VL53L0X_HistogramModes *pHistogramMode);
	int8_t (*SetMeasurementTimingBudgetMicroSeconds)(VL53L0X_DEV Dev,
				uint32_t  MeasurementTimingBudgetMicroSeconds);
	int8_t (*GetMeasurementTimingBudgetMicroSeconds)(
				VL53L0X_DEV Dev,
				uint32_t *pMeasurementTimingBudgetMicroSeconds);
	int8_t (*GetVcselPulsePeriod)(VL53L0X_DEV Dev,
				VL53L0X_VcselPeriod VcselPeriodType,
				uint8_t	*pVCSELPulsePeriod);
	int8_t (*SetVcselPulsePeriod)(VL53L0X_DEV Dev,
				VL53L0X_VcselPeriod VcselPeriodType,
				uint8_t VCSELPulsePeriod);
	int8_t (*SetSequenceStepEnable)(VL53L0X_DEV Dev,
				VL53L0X_SequenceStepId SequenceStepId,
				uint8_t SequenceStepEnabled);
	int8_t (*GetSequenceStepEnable)(VL53L0X_DEV Dev,
				VL53L0X_SequenceStepId SequenceStepId,
				uint8_t *pSequenceStepEnabled);
	int8_t (*GetSequenceStepEnables)(VL53L0X_DEV Dev,
				VL53L0X_SchedulerSequenceSteps_t *pSchedulerSequenceSteps);
	int8_t (*SetSequenceStepTimeout)(VL53L0X_DEV Dev,
				VL53L0X_SequenceStepId SequenceStepId,
				FixPoint1616_t TimeOutMilliSecs);
	int8_t (*GetSequenceStepTimeout)(VL53L0X_DEV Dev,
				VL53L0X_SequenceStepId SequenceStepId,
				FixPoint1616_t *pTimeOutMilliSecs);
	int8_t (*GetNumberOfSequenceSteps)(VL53L0X_DEV Dev,
				uint8_t *pNumberOfSequenceSteps);
	int8_t (*GetSequenceStepsInfo)(
				VL53L0X_SequenceStepId SequenceStepId,
				char *pSequenceStepsString);
	int8_t (*SetInterMeasurementPeriodMilliSeconds)(
				VL53L0X_DEV Dev,
				uint32_t InterMeasurementPeriodMilliSeconds);
	int8_t (*GetInterMeasurementPeriodMilliSeconds)(
				VL53L0X_DEV Dev,
				uint32_t *pInterMeasurementPeriodMilliSeconds);
	int8_t (*SetXTalkCompensationEnable)(VL53L0X_DEV Dev,
				uint8_t XTalkCompensationEnable);
	int8_t (*GetXTalkCompensationEnable)(VL53L0X_DEV Dev,
				uint8_t *pXTalkCompensationEnable);
	int8_t (*SetXTalkCompensationRateMegaCps)(
				VL53L0X_DEV Dev,
				FixPoint1616_t XTalkCompensationRateMegaCps);
	int8_t (*GetXTalkCompensationRateMegaCps)(
				VL53L0X_DEV Dev,
				FixPoint1616_t *pXTalkCompensationRateMegaCps);
	int8_t (*GetNumberOfLimitCheck)(
				uint16_t *pNumberOfLimitCheck);
	int8_t (*GetLimitCheckInfo)(VL53L0X_DEV Dev,
				uint16_t LimitCheckId, char *pLimitCheckString);
	int8_t (*SetLimitCheckEnable)(VL53L0X_DEV Dev,
				uint16_t LimitCheckId,
				uint8_t LimitCheckEnable);
	int8_t (*GetLimitCheckEnable)(VL53L0X_DEV Dev,
				uint16_t LimitCheckId, uint8_t *pLimitCheckEnable);
	int8_t (*SetLimitCheckValue)(VL53L0X_DEV Dev,
				uint16_t LimitCheckId,
				FixPoint1616_t LimitCheckValue);
	int8_t (*GetLimitCheckValue)(VL53L0X_DEV Dev,
				uint16_t LimitCheckId,
				FixPoint1616_t *pLimitCheckValue);
	int8_t (*GetLimitCheckCurrent)(VL53L0X_DEV Dev,
				uint16_t LimitCheckId, FixPoint1616_t *pLimitCheckCurrent);
	int8_t (*SetWrapAroundCheckEnable)(VL53L0X_DEV Dev,
				uint8_t WrapAroundCheckEnable);
	int8_t (*GetWrapAroundCheckEnable)(VL53L0X_DEV Dev,
				uint8_t *pWrapAroundCheckEnable);
	int8_t (*PerformSingleMeasurement)(VL53L0X_DEV Dev);
	int8_t (*PerformRefCalibration)(VL53L0X_DEV Dev,
				uint8_t *pVhvSettings, uint8_t *pPhaseCal);
	int8_t (*SetRefCalibration)(VL53L0X_DEV Dev, uint8_t VhvSettings, uint8_t PhaseCal); 
	int8_t (*GetRefCalibration)(VL53L0X_DEV Dev, uint8_t *pVhvSettings, uint8_t *pPhaseCal);
	int8_t (*PerformXTalkCalibration)(VL53L0X_DEV Dev,
				FixPoint1616_t XTalkCalDistance,
				FixPoint1616_t *pXTalkCompensationRateMegaCps);
	int8_t (*PerformOffsetCalibration)(VL53L0X_DEV Dev,
				FixPoint1616_t CalDistanceMilliMeter,
				int32_t *pOffsetMicroMeter);
	int8_t (*StartMeasurement)(VL53L0X_DEV Dev);
	int8_t (*StopMeasurement)(VL53L0X_DEV Dev);
	int8_t (*GetMeasurementDataReady)(VL53L0X_DEV Dev,
				uint8_t *pMeasurementDataReady);
	int8_t (*WaitDeviceReadyForNewMeasurement)(VL53L0X_DEV Dev,
				uint32_t MaxLoop);
	int8_t (*GetRangingMeasurementData)(VL53L0X_DEV Dev,
				VL53L0X_RangingMeasurementData_t *pRangingMeasurementData);
	int8_t (*GetHistogramMeasurementData)(VL53L0X_DEV Dev,
				VL53L0X_HistogramMeasurementData_t *pHistogramMeasurementData);
	int8_t (*PerformSingleRangingMeasurement)(VL53L0X_DEV Dev,
				VL53L0X_RangingMeasurementData_t *pRangingMeasurementData);
	int8_t (*PerformSingleHistogramMeasurement)(VL53L0X_DEV Dev,
				VL53L0X_HistogramMeasurementData_t *pHistogramMeasurementData);
	int8_t (*SetNumberOfROIZones)(VL53L0X_DEV Dev,
				uint8_t NumberOfROIZones);
	int8_t (*GetNumberOfROIZones)(VL53L0X_DEV Dev,
				uint8_t *pNumberOfROIZones);
	int8_t (*GetMaxNumberOfROIZones)(VL53L0X_DEV Dev,
				uint8_t *pMaxNumberOfROIZones);
	int8_t (*SetGpioConfig)(VL53L0X_DEV Dev,
				uint8_t Pin,
				VL53L0X_DeviceModes DeviceMode,
				VL53L0X_GpioFunctionality Functionality,
				VL53L0X_InterruptPolarity Polarity);
	int8_t (*GetGpioConfig)(VL53L0X_DEV Dev,
				uint8_t Pin,
				VL53L0X_DeviceModes *pDeviceMode,
				VL53L0X_GpioFunctionality *pFunctionality,
				VL53L0X_InterruptPolarity *pPolarity);
	int8_t (*SetInterruptThresholds)(VL53L0X_DEV Dev,
				VL53L0X_DeviceModes DeviceMode,
				FixPoint1616_t ThresholdLow,
				FixPoint1616_t ThresholdHigh);
	int8_t (*GetInterruptThresholds)(VL53L0X_DEV Dev,
				VL53L0X_DeviceModes DeviceMode,
				FixPoint1616_t *pThresholdLow,
				FixPoint1616_t *pThresholdHigh);
	int8_t (*ClearInterruptMask)(VL53L0X_DEV Dev,
				uint32_t InterruptMask);
	int8_t (*GetInterruptMaskStatus)(VL53L0X_DEV Dev,
				uint32_t *pInterruptMaskStatus);
	int8_t (*EnableInterruptMask)(VL53L0X_DEV Dev, uint32_t InterruptMask);
	int8_t (*SetSpadAmbientDamperThreshold)(VL53L0X_DEV Dev,
				uint16_t SpadAmbientDamperThreshold);
	int8_t (*GetSpadAmbientDamperThreshold)(VL53L0X_DEV Dev,
				uint16_t *pSpadAmbientDamperThreshold);
	int8_t (*SetSpadAmbientDamperFactor)(VL53L0X_DEV Dev,
				uint16_t SpadAmbientDamperFactor);
	int8_t (*GetSpadAmbientDamperFactor)(VL53L0X_DEV Dev,
				uint16_t *pSpadAmbientDamperFactor);
	int8_t (*PerformRefSpadManagement)(VL53L0X_DEV Dev,
				uint32_t *refSpadCount, uint8_t *isApertureSpads);
	int8_t (*SetReferenceSpads)(VL53L0X_DEV Dev, uint32_t count, uint8_t isApertureSpads);
	int8_t (*GetReferenceSpads)(VL53L0X_DEV Dev, uint32_t *pSpadCount, uint8_t *pIsApertureSpads);
};

static struct stmvl53l0x_api_fn_t stmvl53l0x_api_func_tbl = {
	.GetVersion = VL53L0X_GetVersion,
	.GetPalSpecVersion = VL53L0X_GetPalSpecVersion,
	.GetProductRevision = VL53L0X_GetProductRevision,
	.GetDeviceInfo = VL53L0X_GetDeviceInfo,
	.GetDeviceErrorStatus = VL53L0X_GetDeviceErrorStatus,
	.GetRangeStatusString = VL53L0X_GetRangeStatusString,
	.GetDeviceErrorString = VL53L0X_GetDeviceErrorString,
	.GetPalErrorString = VL53L0X_GetPalErrorString,
	.GetPalState = VL53L0X_GetPalState,
	.SetPowerMode = VL53L0X_SetPowerMode,
	.GetPowerMode = VL53L0X_GetPowerMode,
	.SetOffsetCalibrationDataMicroMeter =
		VL53L0X_SetOffsetCalibrationDataMicroMeter,
	.SetLinearityCorrectiveGain =
		VL53L0X_SetLinearityCorrectiveGain,
	.GetLinearityCorrectiveGain =
		VL53L0X_GetLinearityCorrectiveGain,
	.GetOffsetCalibrationDataMicroMeter =
		VL53L0X_GetOffsetCalibrationDataMicroMeter,
	.SetGroupParamHold = VL53L0X_SetGroupParamHold,
	.GetUpperLimitMilliMeter = VL53L0X_GetUpperLimitMilliMeter,
	.SetDeviceAddress = VL53L0X_SetDeviceAddress,
	.DataInit = VL53L0X_DataInit,
	.SetTuningSettingBuffer = VL53L0X_SetTuningSettingBuffer,
	.GetTuningSettingBuffer = VL53L0X_GetTuningSettingBuffer,
	.StaticInit = VL53L0X_StaticInit,
	.WaitDeviceBooted = VL53L0X_WaitDeviceBooted,
	.ResetDevice = VL53L0X_ResetDevice,
	.SetDeviceParameters = VL53L0X_SetDeviceParameters,
	.SetDeviceMode = VL53L0X_SetDeviceMode,
	.GetDeviceMode = VL53L0X_GetDeviceMode,
	.SetHistogramMode = VL53L0X_SetHistogramMode,
	.GetHistogramMode = VL53L0X_GetHistogramMode,
	.SetMeasurementTimingBudgetMicroSeconds =
		VL53L0X_SetMeasurementTimingBudgetMicroSeconds,
	.GetMeasurementTimingBudgetMicroSeconds =
		VL53L0X_GetMeasurementTimingBudgetMicroSeconds,
	.GetVcselPulsePeriod = VL53L0X_GetVcselPulsePeriod,
	.SetVcselPulsePeriod = VL53L0X_SetVcselPulsePeriod,
	.SetSequenceStepEnable = VL53L0X_SetSequenceStepEnable,
	.GetSequenceStepEnable = VL53L0X_GetSequenceStepEnable,
	.GetSequenceStepEnables = VL53L0X_GetSequenceStepEnables,
	.SetSequenceStepTimeout = VL53L0X_SetSequenceStepTimeout,
	.GetSequenceStepTimeout = VL53L0X_GetSequenceStepTimeout,
	.GetNumberOfSequenceSteps = VL53L0X_GetNumberOfSequenceSteps,
	.GetSequenceStepsInfo = VL53L0X_GetSequenceStepsInfo,
	.SetInterMeasurementPeriodMilliSeconds =
		VL53L0X_SetInterMeasurementPeriodMilliSeconds,
	.GetInterMeasurementPeriodMilliSeconds =
		VL53L0X_GetInterMeasurementPeriodMilliSeconds,
	.SetXTalkCompensationEnable = VL53L0X_SetXTalkCompensationEnable,
	.GetXTalkCompensationEnable = VL53L0X_GetXTalkCompensationEnable,
	.SetXTalkCompensationRateMegaCps =
		VL53L0X_SetXTalkCompensationRateMegaCps,
	.GetXTalkCompensationRateMegaCps =
		VL53L0X_GetXTalkCompensationRateMegaCps,
	.GetNumberOfLimitCheck = VL53L0X_GetNumberOfLimitCheck,
	.GetLimitCheckInfo = VL53L0X_GetLimitCheckInfo,
	.SetLimitCheckEnable = VL53L0X_SetLimitCheckEnable,
	.GetLimitCheckEnable = VL53L0X_GetLimitCheckEnable,
	.SetLimitCheckValue = VL53L0X_SetLimitCheckValue,
	.GetLimitCheckValue = VL53L0X_GetLimitCheckValue,
	.GetLimitCheckCurrent = VL53L0X_GetLimitCheckCurrent,
	.SetWrapAroundCheckEnable = VL53L0X_SetWrapAroundCheckEnable,
	.GetWrapAroundCheckEnable = VL53L0X_GetWrapAroundCheckEnable,
	.PerformSingleMeasurement = VL53L0X_PerformSingleMeasurement,
	.PerformRefCalibration = VL53L0X_PerformRefCalibration,
	.SetRefCalibration = VL53L0X_SetRefCalibration,
	.GetRefCalibration = VL53L0X_GetRefCalibration,
	.PerformXTalkCalibration = VL53L0X_PerformXTalkCalibration,
	.PerformOffsetCalibration = VL53L0X_PerformOffsetCalibration,
	.StartMeasurement = VL53L0X_StartMeasurement,
	.StopMeasurement = VL53L0X_StopMeasurement,
	.GetMeasurementDataReady = VL53L0X_GetMeasurementDataReady,
	.WaitDeviceReadyForNewMeasurement =
		VL53L0X_WaitDeviceReadyForNewMeasurement,
	.GetRangingMeasurementData = VL53L0X_GetRangingMeasurementData,
	.GetHistogramMeasurementData = VL53L0X_GetHistogramMeasurementData,
	.PerformSingleRangingMeasurement = VL53L0X_PerformSingleRangingMeasurement,
	.PerformSingleHistogramMeasurement =
		VL53L0X_PerformSingleHistogramMeasurement,
	.SetNumberOfROIZones = VL53L0X_SetNumberOfROIZones,
	.GetNumberOfROIZones = VL53L0X_GetNumberOfROIZones,
	.GetMaxNumberOfROIZones = VL53L0X_GetMaxNumberOfROIZones,
	.SetGpioConfig = VL53L0X_SetGpioConfig,
	.GetGpioConfig = VL53L0X_GetGpioConfig,
	.SetInterruptThresholds = VL53L0X_SetInterruptThresholds,
	.GetInterruptThresholds = VL53L0X_GetInterruptThresholds,
	.ClearInterruptMask = VL53L0X_ClearInterruptMask,
	.GetInterruptMaskStatus = VL53L0X_GetInterruptMaskStatus,
	.EnableInterruptMask = VL53L0X_EnableInterruptMask,
	.SetSpadAmbientDamperThreshold = VL53L0X_SetSpadAmbientDamperThreshold,
	.GetSpadAmbientDamperThreshold = VL53L0X_GetSpadAmbientDamperThreshold,
	.SetSpadAmbientDamperFactor = VL53L0X_SetSpadAmbientDamperFactor,
	.GetSpadAmbientDamperFactor = VL53L0X_GetSpadAmbientDamperFactor,
	.PerformRefSpadManagement = VL53L0X_PerformRefSpadManagement,
    .SetReferenceSpads = VL53L0X_SetReferenceSpads,
	.GetReferenceSpads = VL53L0X_GetReferenceSpads,

};
struct stmvl53l0x_api_fn_t *papi_func_tbl;

/*
 * IOCTL definitions
 */
#define VL53L0X_IOCTL_INIT			_IO('p', 0x01)
#define VL53L0X_IOCTL_XTALKCALB		_IOW('p', 0x02, unsigned int)
#define VL53L0X_IOCTL_OFFCALB		_IOW('p', 0x03, unsigned int)
#define VL53L0X_IOCTL_STOP			_IO('p', 0x05)
#define VL53L0X_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL53L0X_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL53L0X_IOCTL_GETDATAS \
			_IOR('p', 0x0b, VL53L0X_RangingMeasurementData_t)
#define VL53L0X_IOCTL_REGISTER \
			_IOWR('p', 0x0c, struct stmvl53l0x_register)
#define VL53L0X_IOCTL_PARAMETER \
			_IOWR('p', 0x0d, struct stmvl53l0x_parameter)

//#define CALIBRATION_FILE 1
#ifdef CALIBRATION_FILE
int8_t offset_calib;
int16_t xtalk_calib;
#endif

static long stmvl53l0x_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg);
/*static int stmvl53l0x_flush(struct file *file, fl_owner_t id);*/
static int stmvl53l0x_open(struct inode *inode, struct file *file);
static int stmvl53l0x_init_client(struct stmvl53l0x_data *data);
static int stmvl53l0x_start(struct stmvl53l0x_data *data, uint8_t scaling,
			init_mode_e mode);
static int stmvl53l0x_stop(struct stmvl53l0x_data *data);

#ifdef CALIBRATION_FILE
static void stmvl53l0x_read_calibration_file(struct stmvl53l0x_data *data)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;
	int i, is_sign = 0;

	f = filp_open("/data/calibration/offset", O_RDONLY, 0);
	if (f != NULL && !IS_ERR(f) && f->f_dentry != NULL) {
		fs = get_fs();
		set_fs(get_ds());
		/* init the buffer with 0 */
		for (i = 0; i < 8; i++)
			buf[i] = 0;
		f->f_op->read(f, buf, 8, &f->f_pos);
		set_fs(fs);
		vl53l0x_dbgmsg("offset as:%s, buf[0]:%c\n", buf, buf[0]);
		offset_calib = 0;
		for (i = 0; i < 8; i++) {
			if (i == 0 && buf[0] == '-')
				is_sign = 1;
			else if (buf[i] >= '0' && buf[i] <= '9')
				offset_calib = offset_calib * 10 +
					(buf[i] - '0');
			else
				break;
		}
		if (is_sign == 1)
			offset_calib = -offset_calib;
		vl53l0x_dbgmsg("offset_calib as %d\n", offset_calib);
/*later
		SetOffsetCalibrationData(vl53l0x_dev, offset_calib);
*/
		filp_close(f, NULL);
	} else {
		vl53l0x_errmsg("no offset calibration file exist!\n");
	}

	is_sign = 0;
	f = filp_open("/data/calibration/xtalk", O_RDONLY, 0);
	if (f != NULL && !IS_ERR(f) && f->f_dentry != NULL) {
		fs = get_fs();
		set_fs(get_ds());
		/* init the buffer with 0 */
		for (i = 0; i < 8; i++)
			buf[i] = 0;
		f->f_op->read(f, buf, 8, &f->f_pos);
		set_fs(fs);
		vl53l0x_dbgmsg("xtalk as:%s, buf[0]:%c\n", buf, buf[0]);
		xtalk_calib = 0;
		for (i = 0; i < 8; i++) {
			if (i == 0 && buf[0] == '-')
				is_sign = 1;
			else if (buf[i] >= '0' && buf[i] <= '9')
				xtalk_calib = xtalk_calib * 10 + (buf[i] - '0');
			else
				break;
		}
		if (is_sign == 1)
			xtalk_calib = -xtalk_calib;
		vl53l0x_dbgmsg("xtalk_calib as %d\n", xtalk_calib);
/* later
		SetXTalkCompensationRate(vl53l0x_dev, xtalk_calib);
*/
		filp_close(f, NULL);
	} else {
		vl53l0x_errmsg("no xtalk calibration file exist!\n");
	}

}

static void stmvl53l0x_write_offset_calibration_file(void)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;

	f = filp_open("/data/calibration/offset", O_WRONLY|O_CREAT, 0644);
	if (f != NULL) {
		fs = get_fs();
		set_fs(get_ds());
		snprintf(buf, 5, "%d", offset_calib);
		vl53l0x_dbgmsg("write offset as:%s, buf[0]:%c\n", buf, buf[0]);
		f->f_op->write(f, buf, 8, &f->f_pos);
		set_fs(fs);
	}
	filp_close(f, NULL);

}

static void stmvl53l0x_write_xtalk_calibration_file(void)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;

	f = filp_open("/data/calibration/xtalk", O_WRONLY|O_CREAT, 0644);
	if (f != NULL) {
		fs = get_fs();
		set_fs(get_ds());
		snprintf(buf, 4, "%d", xtalk_calib);
		vl53l0x_dbgmsg("write xtalk as:%s, buf[0]:%c\n", buf, buf[0]);
		f->f_op->write(f, buf, 8, &f->f_pos);
		set_fs(fs);
	}
	filp_close(f, NULL);

}
#endif
#ifdef DEBUG_TIME_LOG
static void stmvl53l0x_DebugTimeGet(struct timeval *ptv)
{
	do_gettimeofday(ptv);
}

static void stmvl53l0x_DebugTimeDuration(struct timeval *pstart_tv,
			struct timeval *pstop_tv)
{
	long total_sec, total_msec;
	total_sec = pstop_tv->tv_sec - pstart_tv->tv_sec;
	total_msec = (pstop_tv->tv_usec - pstart_tv->tv_usec)/1000;
	total_msec += total_sec * 1000;
	pr_err("elapsedTime:%ld\n", total_msec);
}
#endif

static void stmvl53l0x_setupAPIFunctions(void)
{

	/*cut 1.1*/
	vl53l0x_errmsg("to setup API cut 1.1\n");
	papi_func_tbl->GetVersion = VL53L0X_GetVersion;
	papi_func_tbl->GetPalSpecVersion = VL53L0X_GetPalSpecVersion;
	papi_func_tbl->GetProductRevision = VL53L0X_GetProductRevision;
	papi_func_tbl->GetDeviceInfo = VL53L0X_GetDeviceInfo;
	papi_func_tbl->GetDeviceErrorStatus = VL53L0X_GetDeviceErrorStatus;
	papi_func_tbl->GetRangeStatusString = VL53L0X_GetRangeStatusString;
	papi_func_tbl->GetDeviceErrorString = VL53L0X_GetDeviceErrorString;
	papi_func_tbl->GetPalErrorString = VL53L0X_GetPalErrorString;
	papi_func_tbl->GetPalState = VL53L0X_GetPalState;
	papi_func_tbl->SetPowerMode = VL53L0X_SetPowerMode;
	papi_func_tbl->GetPowerMode = VL53L0X_GetPowerMode;
	papi_func_tbl->SetOffsetCalibrationDataMicroMeter = 
		VL53L0X_SetOffsetCalibrationDataMicroMeter;
	papi_func_tbl->GetOffsetCalibrationDataMicroMeter = 
		VL53L0X_GetOffsetCalibrationDataMicroMeter;
	papi_func_tbl->SetLinearityCorrectiveGain =
		VL53L0X_SetLinearityCorrectiveGain;
	papi_func_tbl->GetLinearityCorrectiveGain =
		VL53L0X_GetLinearityCorrectiveGain;
	papi_func_tbl->SetGroupParamHold = VL53L0X_SetGroupParamHold;
	papi_func_tbl->GetUpperLimitMilliMeter = 
		VL53L0X_GetUpperLimitMilliMeter;
	papi_func_tbl->SetDeviceAddress = VL53L0X_SetDeviceAddress;
	papi_func_tbl->DataInit = VL53L0X_DataInit;
	papi_func_tbl->SetTuningSettingBuffer = VL53L0X_SetTuningSettingBuffer;
	papi_func_tbl->GetTuningSettingBuffer = VL53L0X_GetTuningSettingBuffer;
	papi_func_tbl->StaticInit = VL53L0X_StaticInit;
	papi_func_tbl->WaitDeviceBooted = VL53L0X_WaitDeviceBooted;
	papi_func_tbl->ResetDevice = VL53L0X_ResetDevice;
	papi_func_tbl->SetDeviceParameters = VL53L0X_SetDeviceParameters;
	papi_func_tbl->SetDeviceMode = VL53L0X_SetDeviceMode;
	papi_func_tbl->GetDeviceMode = VL53L0X_GetDeviceMode;
	papi_func_tbl->SetHistogramMode = VL53L0X_SetHistogramMode;
	papi_func_tbl->GetHistogramMode = VL53L0X_GetHistogramMode;
	papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds = 
		VL53L0X_SetMeasurementTimingBudgetMicroSeconds;
	papi_func_tbl->GetMeasurementTimingBudgetMicroSeconds = 
		VL53L0X_GetMeasurementTimingBudgetMicroSeconds;
	papi_func_tbl->GetVcselPulsePeriod = VL53L0X_GetVcselPulsePeriod;
	papi_func_tbl->SetVcselPulsePeriod = VL53L0X_SetVcselPulsePeriod;
	papi_func_tbl->SetSequenceStepEnable = VL53L0X_SetSequenceStepEnable;
	papi_func_tbl->GetSequenceStepEnable = VL53L0X_GetSequenceStepEnable;
	papi_func_tbl->GetSequenceStepEnables = VL53L0X_GetSequenceStepEnables;
	papi_func_tbl->SetSequenceStepTimeout = VL53L0X_SetSequenceStepTimeout;
	papi_func_tbl->GetSequenceStepTimeout = VL53L0X_GetSequenceStepTimeout;
	papi_func_tbl->GetNumberOfSequenceSteps = 
		VL53L0X_GetNumberOfSequenceSteps;
	papi_func_tbl->GetSequenceStepsInfo = VL53L0X_GetSequenceStepsInfo;
	papi_func_tbl->SetInterMeasurementPeriodMilliSeconds = 
		VL53L0X_SetInterMeasurementPeriodMilliSeconds;
	papi_func_tbl->GetInterMeasurementPeriodMilliSeconds = 
		VL53L0X_GetInterMeasurementPeriodMilliSeconds;
	papi_func_tbl->SetXTalkCompensationEnable = 
		VL53L0X_SetXTalkCompensationEnable;
	papi_func_tbl->GetXTalkCompensationEnable = 
		VL53L0X_GetXTalkCompensationEnable;
	papi_func_tbl->SetXTalkCompensationRateMegaCps = 
		VL53L0X_SetXTalkCompensationRateMegaCps;
	papi_func_tbl->GetXTalkCompensationRateMegaCps = 
		VL53L0X_GetXTalkCompensationRateMegaCps;
	papi_func_tbl->GetNumberOfLimitCheck = VL53L0X_GetNumberOfLimitCheck;
	papi_func_tbl->GetLimitCheckInfo = VL53L0X_GetLimitCheckInfo;
	papi_func_tbl->SetLimitCheckEnable = VL53L0X_SetLimitCheckEnable;
	papi_func_tbl->GetLimitCheckEnable = VL53L0X_GetLimitCheckEnable;
	papi_func_tbl->SetLimitCheckValue = VL53L0X_SetLimitCheckValue;
	papi_func_tbl->GetLimitCheckValue = VL53L0X_GetLimitCheckValue;
	papi_func_tbl->GetLimitCheckCurrent = VL53L0X_GetLimitCheckCurrent;
	papi_func_tbl->SetWrapAroundCheckEnable = 
		VL53L0X_SetWrapAroundCheckEnable;
	papi_func_tbl->GetWrapAroundCheckEnable = 
		VL53L0X_GetWrapAroundCheckEnable;
	papi_func_tbl->PerformSingleMeasurement = 
		VL53L0X_PerformSingleMeasurement;
	papi_func_tbl->PerformRefCalibration = VL53L0X_PerformRefCalibration;
	papi_func_tbl->SetRefCalibration = VL53L0X_SetRefCalibration;
	papi_func_tbl->GetRefCalibration = VL53L0X_GetRefCalibration;
	papi_func_tbl->PerformXTalkCalibration = 
		VL53L0X_PerformXTalkCalibration;
	papi_func_tbl->PerformOffsetCalibration = 
		VL53L0X_PerformOffsetCalibration;
	papi_func_tbl->StartMeasurement = VL53L0X_StartMeasurement;
	papi_func_tbl->StopMeasurement = VL53L0X_StopMeasurement;
	papi_func_tbl->GetMeasurementDataReady = 
		VL53L0X_GetMeasurementDataReady;
	papi_func_tbl->WaitDeviceReadyForNewMeasurement =
		VL53L0X_WaitDeviceReadyForNewMeasurement;
	papi_func_tbl->GetRangingMeasurementData =
		VL53L0X_GetRangingMeasurementData;
	papi_func_tbl->GetHistogramMeasurementData =
		VL53L0X_GetHistogramMeasurementData;
	papi_func_tbl->PerformSingleRangingMeasurement =
		VL53L0X_PerformSingleRangingMeasurement;
	papi_func_tbl->PerformSingleHistogramMeasurement =
		VL53L0X_PerformSingleHistogramMeasurement;
	papi_func_tbl->SetNumberOfROIZones = VL53L0X_SetNumberOfROIZones;
	papi_func_tbl->GetNumberOfROIZones = VL53L0X_GetNumberOfROIZones;
	papi_func_tbl->GetMaxNumberOfROIZones = VL53L0X_GetMaxNumberOfROIZones;
	papi_func_tbl->SetGpioConfig = VL53L0X_SetGpioConfig;
	papi_func_tbl->GetGpioConfig = VL53L0X_GetGpioConfig;
	papi_func_tbl->SetInterruptThresholds = VL53L0X_SetInterruptThresholds;
	papi_func_tbl->GetInterruptThresholds = VL53L0X_GetInterruptThresholds;
	papi_func_tbl->ClearInterruptMask = VL53L0X_ClearInterruptMask;
	papi_func_tbl->GetInterruptMaskStatus = VL53L0X_GetInterruptMaskStatus;
	papi_func_tbl->EnableInterruptMask = VL53L0X_EnableInterruptMask;
	papi_func_tbl->SetSpadAmbientDamperThreshold =
		VL53L0X_SetSpadAmbientDamperThreshold;
	papi_func_tbl->GetSpadAmbientDamperThreshold =
		VL53L0X_GetSpadAmbientDamperThreshold;
	papi_func_tbl->SetSpadAmbientDamperFactor =
		VL53L0X_SetSpadAmbientDamperFactor;
	papi_func_tbl->GetSpadAmbientDamperFactor =
		VL53L0X_GetSpadAmbientDamperFactor;
	papi_func_tbl->PerformRefSpadManagement = 
		VL53L0X_PerformRefSpadManagement;
	papi_func_tbl->SetReferenceSpads = VL53L0X_SetReferenceSpads;
	papi_func_tbl->GetReferenceSpads = VL53L0X_GetReferenceSpads;

}

static void stmvl53l0x_ps_read_measurement(struct stmvl53l0x_data *data)
{
	struct timeval tv;
	VL53L0X_DEV vl53l0x_dev = data;
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	FixPoint1616_t LimitCheckCurrent;

	do_gettimeofday(&tv);

	data->ps_data = data->rangeData.RangeMilliMeter;
	input_report_abs(data->input_dev_ps, ABS_DISTANCE,
		(int)(data->ps_data + 5) / 10);
	input_report_abs(data->input_dev_ps, ABS_HAT0X, tv.tv_sec);
	input_report_abs(data->input_dev_ps, ABS_HAT0Y, tv.tv_usec);
	input_report_abs(data->input_dev_ps, ABS_HAT1X,
		data->rangeData.RangeMilliMeter);
	input_report_abs(data->input_dev_ps, ABS_HAT1Y,
		data->rangeData.RangeStatus);
	input_report_abs(data->input_dev_ps, ABS_HAT2X,
		data->rangeData.SignalRateRtnMegaCps);
	input_report_abs(data->input_dev_ps, ABS_HAT2Y,
		data->rangeData.AmbientRateRtnMegaCps);
	input_report_abs(data->input_dev_ps, ABS_HAT3X,
		data->rangeData.MeasurementTimeUsec);
	input_report_abs(data->input_dev_ps, ABS_HAT3Y,
		data->rangeData.RangeDMaxMilliMeter);
	Status = papi_func_tbl->GetLimitCheckCurrent(vl53l0x_dev, 
										VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE,
										&LimitCheckCurrent);
	if (Status == VL53L0X_ERROR_NONE) {
		input_report_abs(data->input_dev_ps, ABS_WHEEL,
				LimitCheckCurrent);
	}
	input_report_abs(data->input_dev_ps, ABS_PRESSURE,
		data->rangeData.EffectiveSpadRtnCount);
	input_sync(data->input_dev_ps);

	if (data->enableDebug)
		vl53l0x_errmsg(
"range:%d, RtnRateMcps:%d,err:0x%x,Dmax:%d,rtnambr:%d,time:%d,"
	"Spad:%d,SigmaLimit:%d\n",
			data->rangeData.RangeMilliMeter,
			data->rangeData.SignalRateRtnMegaCps,
			data->rangeData.RangeStatus,
			data->rangeData.RangeDMaxMilliMeter,
			data->rangeData.AmbientRateRtnMegaCps,
			data->rangeData.MeasurementTimeUsec,
			data->rangeData.EffectiveSpadRtnCount,
			LimitCheckCurrent
			);


}

static void stmvl53l0x_cancel_handler(struct stmvl53l0x_data *data)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	ret = cancel_delayed_work(&data->dwork);
	if (ret == 0)
		vl53l0x_errmsg("cancel_delayed_work return FALSE\n");

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

}

void stmvl53l0x_schedule_handler(struct stmvl53l0x_data *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	cancel_delayed_work(&data->dwork);
	schedule_delayed_work(&data->dwork, 0);
	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

}


#ifdef USE_INT
static irqreturn_t stmvl53l0x_interrupt_handler(int vec, void *info)
{

	struct stmvl53l0x_data *data = (struct stmvl53l0x_data *)info;

	if (data->irq == vec) {
		data->interrupt_received = 1;
		schedule_delayed_work(&data->dwork, 0);
	}
	return IRQ_HANDLED;
}
#else
/* Flag used to exit the thread when kthread_stop() is invoked */
static int poll_thread_exit = 0;
int stmvl53l0x_poll_thread(void *data)
{
	VL53L0X_DEV vl53l0x_dev = data;
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint32_t sleep_time = 0;
	uint32_t interruptStatus = 0;

	pr_err("%s(%d) : Starting Polling thread\n", __FUNCTION__, __LINE__);

	while(!kthread_should_stop()) {
		/* Check if enable_ps_sensor is true or exit request is made. If not block */
		wait_event(vl53l0x_dev->poll_thread_wq, 
			(vl53l0x_dev->enable_ps_sensor || poll_thread_exit));
		if (poll_thread_exit) {
			pr_err("%s(%d) : Exiting the poll thread\n", __FUNCTION__, __LINE__);
			break;
		}

		mutex_lock(&vl53l0x_dev->work_mutex);

		sleep_time = vl53l0x_dev->delay_ms;
		Status = VL53L0X_GetInterruptMaskStatus(vl53l0x_dev, &interruptStatus);
		if (Status == VL53L0X_ERROR_NONE &&
			interruptStatus &&
			interruptStatus != vl53l0x_dev->interruptStatus) {
			vl53l0x_dev->interruptStatus = interruptStatus;
			vl53l0x_dev->noInterruptCount = 0;
			stmvl53l0x_schedule_handler(vl53l0x_dev);
			
		} else {
			vl53l0x_dev->noInterruptCount++;
		}

		//Force Clear interrupt mask and restart if no interrupt after twice the timingBudget
		if ((vl53l0x_dev->noInterruptCount * vl53l0x_dev->delay_ms) > (vl53l0x_dev->timingBudget * 2)) {
			pr_err("No interrupt after (%u) msec(TimingBudget = %u) ."
					" Clear Interrupt Mask and restart\n",(vl53l0x_dev->noInterruptCount * vl53l0x_dev->delay_ms) ,
															vl53l0x_dev->timingBudget );
			Status = papi_func_tbl->ClearInterruptMask(vl53l0x_dev, 0);
			if (vl53l0x_dev->deviceMode == VL53L0X_DEVICEMODE_SINGLE_RANGING) {
				Status = papi_func_tbl->StartMeasurement(vl53l0x_dev);
				if (Status != VL53L0X_ERROR_NONE) {
					pr_err("%s(%d) : Status = %d\n", __FUNCTION__ , __LINE__, Status);
				}
			}
		}
		
		mutex_unlock(&vl53l0x_dev->work_mutex);
		
		//Sleep for delay_ms milliseconds
		msleep(sleep_time); 
	}

	return 0;
}
#endif

/* work handler */
static void stmvl53l0x_work_handler(struct work_struct *work)
{
	struct stmvl53l0x_data *data = container_of(work, struct stmvl53l0x_data,
				dwork.work);
	VL53L0X_DEV vl53l0x_dev = data;
	//uint8_t val;

	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	mutex_lock(&data->work_mutex);
	/* vl53l0x_dbgmsg("Enter\n"); */


	if (vl53l0x_dev->enable_ps_sensor == 1) {
#ifdef DEBUG_TIME_LOG
		stmvl53l0x_DebugTimeGet(&stop_tv);
		stmvl53l0x_DebugTimeDuration(&start_tv, &stop_tv);
#endif
		if (vl53l0x_dev->interrupt_received == 1) { 	/* ISR has scheduled this function */
		
			Status = papi_func_tbl->GetInterruptMaskStatus(vl53l0x_dev, &vl53l0x_dev->interruptStatus);
			if (Status != VL53L0X_ERROR_NONE) {
				pr_err("%s(%d) : Status = %d\n", __FUNCTION__ , __LINE__, Status);
			}
			vl53l0x_dev->interrupt_received = 0;
		} 
		if (data->enableDebug)
			pr_err("interruptStatus:0x%x, interrupt_received:%d\n",
				vl53l0x_dev->interruptStatus, vl53l0x_dev->interrupt_received);

		if (vl53l0x_dev->interruptStatus == vl53l0x_dev->gpio_function) { 
				Status = papi_func_tbl->ClearInterruptMask(vl53l0x_dev, 0);
			if (Status != VL53L0X_ERROR_NONE) {
				pr_err("%s(%d) : Status = %d\n", __FUNCTION__ , __LINE__, Status);
			} else {
				Status = papi_func_tbl->GetRangingMeasurementData( vl53l0x_dev, 
																&(data->rangeData));
				/* to push the measurement */
				if (Status == VL53L0X_ERROR_NONE)
					stmvl53l0x_ps_read_measurement(data);
				else
					pr_err("%s(%d) : Status = %d\n", __FUNCTION__ , __LINE__, Status);

				if (data->enableDebug)
					pr_err("Measured range:%d\n", data->rangeData.RangeMilliMeter);

				if (data->deviceMode == VL53L0X_DEVICEMODE_SINGLE_RANGING) 
					Status = papi_func_tbl->StartMeasurement(vl53l0x_dev);
			}
		}
#ifdef DEBUG_TIME_LOG
		stmvl53l0x_DebugTimeGet(&start_tv);
#endif

		}


	vl53l0x_dev->interruptStatus = 0;

	mutex_unlock(&data->work_mutex);

}


/*
 * SysFS support
 */
static ssize_t stmvl53l0x_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->enable_ps_sensor);
}

static ssize_t stmvl53l0x_store_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);

	unsigned long val = simple_strtoul(buf, NULL, 10);

	if ((val != 0) && (val != 1)) {
		vl53l0x_errmsg("store unvalid value=%ld\n", val);
		return count;
	}
	mutex_lock(&data->work_mutex);
	vl53l0x_dbgmsg("Enter, enable_ps_sensor flag:%d\n",
		data->enable_ps_sensor);
	vl53l0x_dbgmsg("enable ps senosr ( %ld)\n", val);

	if (val == 1) {
		/* turn on tof sensor */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0x_start(data, 3, NORMAL_MODE);
		} else {
			vl53l0x_errmsg("Already enabled. Skip !");
		}
	} else {
		/* turn off tof sensor */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0x_stop(data);
		}
	}
	vl53l0x_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);

	return count;
}

static DEVICE_ATTR(enable_ps_sensor, 0664/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_enable_ps_sensor,
					stmvl53l0x_store_enable_ps_sensor);

static ssize_t stmvl53l0x_show_enable_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->enableDebug);
}

/* for debug */
static ssize_t stmvl53l0x_store_enable_debug(struct device *dev,
					struct device_attribute *attr, const
					char *buf, size_t count)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);
	long on = simple_strtoul(buf, NULL, 10);

	if ((on != 0) &&  (on != 1)) {
		vl53l0x_errmsg("set debug=%ld\n", on);
		return count;
	}
	data->enableDebug = on;

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(enable_debug, 0660/*S_IWUSR | S_IRUGO*/,
				   stmvl53l0x_show_enable_debug,
					stmvl53l0x_store_enable_debug);

static ssize_t stmvl53l0x_show_set_delay_ms(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->delay_ms);
}

/* for work handler scheduler time */
static ssize_t stmvl53l0x_store_set_delay_ms(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);
	long delay_ms = simple_strtoul(buf, NULL, 10);

	if (delay_ms == 0) {
		vl53l0x_errmsg("set delay_ms=%ld\n", delay_ms);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->delay_ms = delay_ms;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_delay_ms, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_set_delay_ms,
					stmvl53l0x_store_set_delay_ms);

/* Timing Budget */
static ssize_t stmvl53l0x_show_timing_budget(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 10, "%d\n", data->timingBudget);
}

static ssize_t stmvl53l0x_store_set_timing_budget(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);
	long timingBudget = simple_strtoul(buf, NULL, 10);

	if (timingBudget == 0) {
		vl53l0x_errmsg("set timingBudget=%ld\n", timingBudget);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->timingBudget = timingBudget;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_timing_budget, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_timing_budget,
					stmvl53l0x_store_set_timing_budget);


/* Long Range  */
static ssize_t stmvl53l0x_show_long_range(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->useLongRange);
}

static ssize_t stmvl53l0x_store_set_long_range(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l0x_data *data = dev_get_drvdata(dev);
	long useLongRange = simple_strtoul(buf, NULL, 10);

	if ((useLongRange != 0) &&  (useLongRange != 1)) {
		vl53l0x_errmsg("set useLongRange=%ld\n", useLongRange);
		return count;
	}

	mutex_lock(&data->work_mutex);
	data->useLongRange = useLongRange;
	if (useLongRange) {
		data->timingBudget = 26000;
	} else {
		data->timingBudget = 200000;
	}
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_long_range, 0660/*S_IWUGO | S_IRUGO*/,
				   stmvl53l0x_show_long_range,
					stmvl53l0x_store_set_long_range);


static struct attribute *stmvl53l0x_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_debug.attr,
	&dev_attr_set_delay_ms.attr ,
	&dev_attr_set_timing_budget.attr ,
	&dev_attr_set_long_range.attr ,
	NULL
};


static const struct attribute_group stmvl53l0x_attr_group = {
	.attrs = stmvl53l0x_attributes,
};

/*
 * misc device file operation functions
 */
static int stmvl53l0x_ioctl_handler(struct file *file,
			unsigned int cmd, unsigned long arg,
			void __user *p)
{
	int rc = 0;
	unsigned int xtalkint = 0;
	unsigned int targetDistance = 0;
	int8_t offsetint = 0;
	struct stmvl53l0x_data *data =
			container_of(file->private_data,
				struct stmvl53l0x_data, miscdev);
	struct stmvl53l0x_register reg;
	struct stmvl53l0x_parameter parameter;
	VL53L0X_DEV vl53l0x_dev = data;
	VL53L0X_DeviceModes deviceMode;
	uint8_t page_num = 0;

	if (!data)
		return -EINVAL;

	vl53l0x_dbgmsg("Enter enable_ps_sensor:%d\n", data->enable_ps_sensor);
	switch (cmd) {
	/* enable */
	case VL53L0X_IOCTL_INIT:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_INIT\n");
		/* turn on tof sensor only if it's not enabled by other
		client */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0x_start(data, 3, NORMAL_MODE);
		} else
			rc = -EINVAL;
		break;
	/* crosstalk calibration */
	case VL53L0X_IOCTL_XTALKCALB:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_XTALKCALB\n");
		data->xtalkCalDistance = 100;
		if (copy_from_user(&targetDistance, (unsigned int *)p,
			sizeof(unsigned int))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		data->xtalkCalDistance = targetDistance;

		/* turn on tof sensor only if it's not enabled by other
		client */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0x_start(data, 3, XTALKCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up Xtalk value */
	case VL53L0X_IOCTL_SETXTALK:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_SETXTALK\n");
		if (copy_from_user(&xtalkint, (unsigned int *)p,
			sizeof(unsigned int))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl53l0x_dbgmsg("SETXTALK as 0x%x\n", xtalkint);
#ifdef CALIBRATION_FILE
		xtalk_calib = xtalkint;
		stmvl53l0x_write_xtalk_calibration_file();
#endif
/* later
		SetXTalkCompensationRate(vl53l0x_dev, xtalkint);
*/
		break;
	/* offset calibration */
	case VL53L0X_IOCTL_OFFCALB:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_OFFCALB\n");
		data->offsetCalDistance = 50;
		if (copy_from_user(&targetDistance, (unsigned int *)p,
			sizeof(unsigned int))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		data->offsetCalDistance = targetDistance;
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0x_start(data, 3, OFFSETCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up offset value */
	case VL53L0X_IOCTL_SETOFFSET:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_SETOFFSET\n");
		if (copy_from_user(&offsetint, (int8_t *)p, sizeof(int8_t))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl53l0x_dbgmsg("SETOFFSET as %d\n", offsetint);
#ifdef CALIBRATION_FILE
		offset_calib = offsetint;
		stmvl53l0x_write_offset_calibration_file();
#endif
/* later
		SetOffsetCalibrationData(vl53l0x_dev, offsetint);
*/
		break;
	/* disable */
	case VL53L0X_IOCTL_STOP:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_STOP\n");
		/* turn off tof sensor only if it's enabled by other client */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0x_stop(data);
		}
		break;
	/* Get all range data */
	case VL53L0X_IOCTL_GETDATAS:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_GETDATAS\n");
		if (copy_to_user((VL53L0X_RangingMeasurementData_t *)p,
			&(data->rangeData),
			sizeof(VL53L0X_RangingMeasurementData_t))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	/* Register tool */
	case VL53L0X_IOCTL_REGISTER:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_REGISTER\n");
		if (copy_from_user(&reg, (struct stmvl53l0x_register *)p,
			sizeof(struct stmvl53l0x_register))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		reg.status = 0;
		page_num = (uint8_t)((reg.reg_index & 0x0000ff00) >> 8);
		vl53l0x_dbgmsg(
"VL53L0X_IOCTL_REGISTER,	page number:%d\n", page_num);
		if (page_num != 0)
			reg.status = VL53L0X_WrByte(vl53l0x_dev, 0xFF, page_num);

		switch (reg.reg_bytes) {
		case(4):
			if (reg.is_read)
				reg.status = VL53L0X_RdDWord(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					&reg.reg_data);
			else
				reg.status = VL53L0X_WrDWord(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					reg.reg_data);
			break;
		case(2):
			if (reg.is_read)
				reg.status = VL53L0X_RdWord(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					(uint16_t *)&reg.reg_data);
			else
				reg.status = VL53L0X_WrWord(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					(uint16_t)reg.reg_data);
			break;
		case(1):
			if (reg.is_read)
				reg.status = VL53L0X_RdByte(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					(uint8_t *)&reg.reg_data);
			else
				reg.status = VL53L0X_WrByte(vl53l0x_dev,
					(uint8_t)reg.reg_index,
					(uint8_t)reg.reg_data);
			break;
		default:
			reg.status = -1;

		}
		if (page_num != 0)
			reg.status = VL53L0X_WrByte(vl53l0x_dev, 0xFF, 0);


		if (copy_to_user((struct stmvl53l0x_register *)p, &reg,
				sizeof(struct stmvl53l0x_register))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	/* parameter access */
	case VL53L0X_IOCTL_PARAMETER:
		vl53l0x_dbgmsg("VL53L0X_IOCTL_PARAMETER\n");
		if (copy_from_user(&parameter, (struct stmvl53l0x_parameter *)p,
				sizeof(struct stmvl53l0x_parameter))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		parameter.status = 0;
		if (data->enableDebug)
			vl53l0x_dbgmsg("VL53L0X_IOCTL_PARAMETER Name = %d\n", parameter.name);
		switch (parameter.name) {
		case (OFFSET_PAR):
			if (parameter.is_read)
				parameter.status =
					papi_func_tbl->GetOffsetCalibrationDataMicroMeter(
						vl53l0x_dev, &parameter.value);
			else
				parameter.status =
					papi_func_tbl->SetOffsetCalibrationDataMicroMeter(
						vl53l0x_dev, parameter.value);
			vl53l0x_dbgmsg("get parameter value as %d\n", parameter.value);
			break;
			
	   	case (REFERENCESPADS_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetReferenceSpads(vl53l0x_dev,
						(uint32_t*)&(parameter.value), (uint8_t*)&(parameter.value2));
					if (data->enableDebug)
						vl53l0x_dbgmsg("Get RefSpad : Count:%u, Type:%u\n",parameter.value,
																		(uint8_t)parameter.value2);
			} else {
				if (data->enableDebug)
					vl53l0x_dbgmsg("Set RefSpad : Count:%u, Type:%u\n",parameter.value,
																	(uint8_t)parameter.value2);

				parameter.status =
					papi_func_tbl->SetReferenceSpads(vl53l0x_dev,
						(uint32_t)(parameter.value), (uint8_t)(parameter.value2));
			}
			break;
			
	   	case (REFCALIBRATION_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetRefCalibration(vl53l0x_dev,
						(uint8_t *)&(parameter.value), (uint8_t *)&(parameter.value2));
					if (data->enableDebug)
						vl53l0x_dbgmsg("Get Ref : Vhv:%u, PhaseCal:%u\n",(uint8_t)parameter.value,
																	(uint8_t)parameter.value2);
			} else {
				if (data->enableDebug)
					vl53l0x_dbgmsg("Set Ref : Vhv:%u, PhaseCal:%u\n",(uint8_t)parameter.value,
																(uint8_t)parameter.value2);
				parameter.status =
					papi_func_tbl->SetRefCalibration(vl53l0x_dev,
						(uint8_t)(parameter.value), (uint8_t)(parameter.value2));
			}
			break;
		case (XTALKRATE_PAR):
			if (parameter.is_read)
				parameter.status =
					papi_func_tbl->GetXTalkCompensationRateMegaCps(
						vl53l0x_dev, (FixPoint1616_t *)
						&parameter.value);
			else
				parameter.status =
					papi_func_tbl->SetXTalkCompensationRateMegaCps(
						vl53l0x_dev,
						(FixPoint1616_t)
							parameter.value);

			break;
		case (XTALKENABLE_PAR):
			if (parameter.is_read)
				parameter.status =
					papi_func_tbl->GetXTalkCompensationEnable(
						vl53l0x_dev,
						(uint8_t *) &parameter.value);
			else
				parameter.status =
					papi_func_tbl->SetXTalkCompensationEnable(
						vl53l0x_dev,
						(uint8_t) parameter.value);
			break;
		case (GPIOFUNC_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetGpioConfig(vl53l0x_dev, 0, &deviceMode,
						&data->gpio_function,
						&data->gpio_polarity);
				parameter.value = data->gpio_function;
			} else {
				data->gpio_function = parameter.value;
				parameter.status =
					papi_func_tbl->SetGpioConfig(vl53l0x_dev, 0, 0,
						data->gpio_function,
						data->gpio_polarity);
			}
			break;
		case (LOWTHRESH_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetInterruptThresholds(vl53l0x_dev, 0,
					&(data->low_threshold), &(data->high_threshold));
				parameter.value = data->low_threshold >> 16;
			} else {
				data->low_threshold = parameter.value << 16;
				parameter.status =
					papi_func_tbl->SetInterruptThresholds(vl53l0x_dev, 0,
					data->low_threshold, data->high_threshold);
			}
			break;
		case (HIGHTHRESH_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetInterruptThresholds(vl53l0x_dev, 0,
					&(data->low_threshold), &(data->high_threshold));
				parameter.value = data->high_threshold >> 16;
			} else {
				data->high_threshold = parameter.value << 16;
				parameter.status =
					papi_func_tbl->SetInterruptThresholds(vl53l0x_dev, 0,
					data->low_threshold, data->high_threshold);
			}
			break;
		case (DEVICEMODE_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetDeviceMode(vl53l0x_dev,
						(VL53L0X_DeviceModes *)&(parameter.value));
			} else {
				parameter.status =
					papi_func_tbl->SetDeviceMode(vl53l0x_dev,
						(VL53L0X_DeviceModes)(parameter.value));
				data->deviceMode = (VL53L0X_DeviceModes)(parameter.value);
			}
			break;



		case (INTERMEASUREMENT_PAR):
			if (parameter.is_read) {
				parameter.status =
					papi_func_tbl->GetInterMeasurementPeriodMilliSeconds(vl53l0x_dev,
						(uint32_t *)&(parameter.value));
			} else {
				parameter.status =
					papi_func_tbl->SetInterMeasurementPeriodMilliSeconds(vl53l0x_dev,
						(uint32_t)(parameter.value));
				data->interMeasurems = parameter.value;
			}
			break;

		}

		if (copy_to_user((struct stmvl53l0x_parameter *)p, &parameter,
				sizeof(struct stmvl53l0x_parameter))) {
			vl53l0x_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int stmvl53l0x_open(struct inode *inode, struct file *file)
{
	return 0;
}

#if 1
static int stmvl53l0x_flush(struct file *file, fl_owner_t id)
{
	struct stmvl53l0x_data *data = container_of(file->private_data,
					struct stmvl53l0x_data, miscdev);
	(void) file;
	(void) id;

	if (data) {
		if (data->enable_ps_sensor == 1) {
			/* turn off tof sensor if it's enabled */
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0x_stop(data);
		}
	}
	return 0;
}
#endif
static long stmvl53l0x_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	long ret;
	struct stmvl53l0x_data *data =
			container_of(file->private_data,
					struct stmvl53l0x_data, miscdev);
	mutex_lock(&data->work_mutex);
	ret = stmvl53l0x_ioctl_handler(file, cmd, arg, (void __user *)arg);
	mutex_unlock(&data->work_mutex);

	return ret;
}

/*
 * Initialization function
 */
static int stmvl53l0x_init_client(struct stmvl53l0x_data *data)
{

	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	VL53L0X_DeviceInfo_t DeviceInfo;
	VL53L0X_DEV vl53l0x_dev = data;

	uint32_t refSpadCount;
	uint8_t isApertureSpads;
	uint8_t VhvSettings;
	uint8_t PhaseCal;

	vl53l0x_dbgmsg("Enter\n");

	vl53l0x_dev->I2cDevAddr      = 0x52;
	vl53l0x_dev->comms_type      =  1;
	vl53l0x_dev->comms_speed_khz =  400;

	/* Setup API functions based on revision */
	stmvl53l0x_setupAPIFunctions();

	if (Status == VL53L0X_ERROR_NONE && data->reset) {
		pr_err("Call of VL53L0X_DataInit\n");
		Status = papi_func_tbl->DataInit(vl53l0x_dev); /* Data initialization */
		//data->reset = 0;
	}

	if (Status == VL53L0X_ERROR_NONE) {
		pr_err("VL53L0X_GetDeviceInfo:\n");
		Status = papi_func_tbl->GetDeviceInfo(vl53l0x_dev, &DeviceInfo);
		if (Status == VL53L0X_ERROR_NONE) {
			pr_err("Device Name : %s\n", DeviceInfo.Name);
			pr_err("Device Type : %s\n", DeviceInfo.Type);
			pr_err("Device ID : %s\n", DeviceInfo.ProductId);
			pr_err("Product type: %d\n", DeviceInfo.ProductType);
			pr_err("ProductRevisionMajor : %d\n",
				DeviceInfo.ProductRevisionMajor);
			pr_err("ProductRevisionMinor : %d\n",
				DeviceInfo.ProductRevisionMinor);
		}
	}

	if (Status == VL53L0X_ERROR_NONE) {
		pr_err("Call of VL53L0X_StaticInit\n");
		Status = papi_func_tbl->StaticInit(vl53l0x_dev);
		/* Device Initialization */
	}

	if (Status == VL53L0X_ERROR_NONE && data->reset) {
		if (papi_func_tbl->PerformRefCalibration != NULL) {
			pr_err("Call of VL53L0X_PerformRefCalibration\n");
			Status = papi_func_tbl->PerformRefCalibration(vl53l0x_dev,
					&VhvSettings, &PhaseCal); /* Ref calibration */
		}
	}

	if (Status == VL53L0X_ERROR_NONE && data->reset) {
		if (papi_func_tbl->PerformRefSpadManagement != NULL) {
			pr_err("Call of VL53L0X_PerformRefSpadManagement\n");
			Status = papi_func_tbl->PerformRefSpadManagement(vl53l0x_dev,
					&refSpadCount, &isApertureSpads); /* Ref Spad Management */
		}
		data->reset = 0; //needed, even the function is NULL
	}

	if (Status == VL53L0X_ERROR_NONE) {

		pr_err("Call of VL53L0X_SetDeviceMode\n");
		Status = papi_func_tbl->SetDeviceMode(vl53l0x_dev,
					VL53L0X_DEVICEMODE_SINGLE_RANGING);
		/* Setup in	single ranging mode */
	}

#if 0
	if (Status == VL53L0X_ERROR_NONE) {
		pr_err("set LimitCheckValue SIGMA_FINAL_RANGE\n");
		LimitValue = 30 << 16;
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
					VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE,
					LimitValue);
	}

	if (Status == VL53L0X_ERROR_NONE) {
		pr_err("set LimitCheckValue SIGNAL_RATE_FINAL_RANGE\n");
		//SigmaLimitValue = 94743; /* 1.44567500 * 65536 */
		LimitValue = 553495; /* 8.44567500 * 65536 */
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
					VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
					LimitValue);
	}

	/*  Enable/Disable Sigma and Signal check */
	if (Status == VL53L0X_ERROR_NONE)
		Status = papi_func_tbl->SetLimitCheckEnable(vl53l0x_dev,
					VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1);

	if (Status == VL53L0X_ERROR_NONE)
		Status = papi_func_tbl->SetLimitCheckEnable(vl53l0x_dev,
					VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 0);
#endif
	if (Status == VL53L0X_ERROR_NONE)
		Status = papi_func_tbl->SetWrapAroundCheckEnable(vl53l0x_dev, 1);

#ifdef CALIBRATION_FILE
	/*stmvl53l0x_read_calibration_file(data);*/
#endif

	vl53l0x_dbgmsg("End\n");

	return 0;
}

static int stmvl53l0x_start(struct stmvl53l0x_data *data, uint8_t scaling,
	init_mode_e mode)
{
	int rc = 0;
	VL53L0X_DEV vl53l0x_dev = data;
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	vl53l0x_dbgmsg("Enter\n");

	/* Power up */
	rc = pmodule_func_tbl->power_up(data->client_object, &data->reset);
	if (rc) {
		vl53l0x_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}
	/* init */
	rc = stmvl53l0x_init_client(data);
	if (rc) {
		vl53l0x_errmsg("%d, error rc %d\n", __LINE__, rc);
		pmodule_func_tbl->power_down(data->client_object);
		return -EINVAL;
	}

	/* check mode */
	if (mode != NORMAL_MODE)
		papi_func_tbl->SetXTalkCompensationEnable(vl53l0x_dev, 1);

	if (mode == OFFSETCALIB_MODE) {
		/*VL53L0X_SetOffsetCalibrationDataMicroMeter(vl53l0x_dev, 0);*/
		FixPoint1616_t OffsetMicroMeter;
		papi_func_tbl->PerformOffsetCalibration(vl53l0x_dev,
			(data->offsetCalDistance<<16),
			&OffsetMicroMeter);
		pr_err("Offset calibration:%u\n", OffsetMicroMeter);
		return rc;
	} else if (mode == XTALKCALIB_MODE) {
		FixPoint1616_t XTalkCompensationRateMegaCps;
		/*caltarget distance : 100mm and convert to
		* fixed point 16 16 format
		*/
		papi_func_tbl->PerformXTalkCalibration(vl53l0x_dev,
			(data->xtalkCalDistance<<16),
			&XTalkCompensationRateMegaCps);
		pr_err("Xtalk calibration:%u\n", XTalkCompensationRateMegaCps);
		return rc;
	}
	/* set up device parameters */
	data->gpio_polarity = VL53L0X_INTERRUPTPOLARITY_LOW;

	/* Following two calls are made from IOCTL as well */
	papi_func_tbl->SetGpioConfig(vl53l0x_dev, 0, 0,
		data->gpio_function,
		VL53L0X_INTERRUPTPOLARITY_LOW);

	papi_func_tbl->SetInterruptThresholds(vl53l0x_dev, 0,
		data->low_threshold, data->high_threshold);

	if (data->deviceMode == VL53L0X_DEVICEMODE_CONTINUOUS_TIMED_RANGING ) {
	papi_func_tbl->SetInterMeasurementPeriodMilliSeconds(vl53l0x_dev,
			data->interMeasurems);
	}

	pr_err("DeviceMode:0x%x, interMeasurems:%d==\n", data->deviceMode,
			data->interMeasurems);
	papi_func_tbl->SetDeviceMode(vl53l0x_dev,
			data->deviceMode);
	papi_func_tbl->ClearInterruptMask(vl53l0x_dev,
							0);



	if (vl53l0x_dev->useLongRange) {		
		pr_err("Configure Long Ranging\n");
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
						VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
						(FixPoint1616_t)(65536/10)); /* 0.1 * 65536 */
		if (Status == VL53L0X_ERROR_NONE) {
			Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
						VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE,
						(FixPoint1616_t)(60*65536));
		} else {
			pr_err("SIGNAL_RATE_FINAL_RANGE failed with errcode = %d\n", Status);
		}

		if (Status == VL53L0X_ERROR_NONE) {
			pr_err("Set Timing Budget = %u\n", vl53l0x_dev->timingBudget);
			Status =
			papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds(vl53l0x_dev, 
																vl53l0x_dev->timingBudget);
		} else {
			pr_err("SetLimitCheckValue failed with errcode = %d\n", Status);
		}
		

		if (Status == VL53L0X_ERROR_NONE) {
			Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0x_dev,
						VL53L0X_VCSEL_PERIOD_PRE_RANGE, 18);
		} else {
			pr_err("SetMeasurementTimingBudget failed with errcode = %d\n", Status);
		}

		if (Status == VL53L0X_ERROR_NONE) {
			Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0x_dev,
				VL53L0X_VCSEL_PERIOD_FINAL_RANGE, 14);
		} else {
			pr_err("SetVcselPulsePeriod(PRE, 18) failed with errcode = %d\n", Status);
		}

		if (Status != VL53L0X_ERROR_NONE) {
			pr_err("SetVcselPulsePeriod(FINAL, 14) failed with errcode = %d\n", Status);
		}
	} else {
		pr_err("Configure High Accuracy\n");
		Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
						VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
						(FixPoint1616_t)(25 * 65536 / 100)); /* 0.25 * 65536 */
		if (Status == VL53L0X_ERROR_NONE) {
			Status = papi_func_tbl->SetLimitCheckValue(vl53l0x_dev,
						VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE,
						(FixPoint1616_t)(18*65536));
		} else {
			pr_err("SIGNAL_RATE_FINAL_RANGE failed with errcode = %d\n", Status);
		}

		if (Status == VL53L0X_ERROR_NONE) {
			pr_err("Set Timing Budget = %u\n", vl53l0x_dev->timingBudget);
			Status =
			papi_func_tbl->SetMeasurementTimingBudgetMicroSeconds(vl53l0x_dev, 
																vl53l0x_dev->timingBudget);
		} else {
			pr_err("SetLimitCheckValue failed with errcode = %d\n", Status);
		}
		

		if (Status == VL53L0X_ERROR_NONE) {
			Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0x_dev,
						VL53L0X_VCSEL_PERIOD_PRE_RANGE, 14);
		} else {
			pr_err("SetMeasurementTimingBudget failed with errcode = %d\n", Status);
		}

		if (Status == VL53L0X_ERROR_NONE) {
			Status = papi_func_tbl->SetVcselPulsePeriod(vl53l0x_dev,
				VL53L0X_VCSEL_PERIOD_FINAL_RANGE, 10);
		} else {
			pr_err("SetVcselPulsePeriod(PRE, 14) failed with errcode = %d\n", Status);
		}

		if (Status != VL53L0X_ERROR_NONE) {
			pr_err("SetVcselPulsePeriod(FINAL, 10) failed with errcode = %d\n", Status);
		}

	}

	/* start the ranging */
	papi_func_tbl->StartMeasurement(vl53l0x_dev);
	data->enable_ps_sensor = 1;

#ifndef USE_INT
	/* Unblock the thread execution */
	wake_up(&vl53l0x_dev->poll_thread_wq);
#endif

	vl53l0x_dbgmsg("End\n");

	return rc;
}

static int stmvl53l0x_stop(struct stmvl53l0x_data *data)
{
	int rc = 0;
	VL53L0X_DEV vl53l0x_dev = data;

	vl53l0x_dbgmsg("Enter\n");

	/* stop - if continuous mode */
	if (data->deviceMode == VL53L0X_DEVICEMODE_CONTINUOUS_RANGING ||
		data->deviceMode == VL53L0X_DEVICEMODE_CONTINUOUS_TIMED_RANGING)
		papi_func_tbl->StopMeasurement(vl53l0x_dev);

	/* clean interrupt */
	papi_func_tbl->ClearInterruptMask(vl53l0x_dev, 0);

	/* cancel work handler */
	stmvl53l0x_cancel_handler(data);
	/* power down */
	rc = pmodule_func_tbl->power_down(data->client_object);
	if (rc) {
		vl53l0x_errmsg("%d, error rc %d\n", __LINE__, rc);
		return rc;
	}
	vl53l0x_dbgmsg("End\n");

	return rc;
}

/*
 * I2C init/probing/exit functions
 */
static const struct file_operations stmvl53l0x_ranging_fops = {
	.owner =			THIS_MODULE,
	.unlocked_ioctl =	stmvl53l0x_ioctl,
	.open =				stmvl53l0x_open,
	.flush =			stmvl53l0x_flush,
};

/*
static struct miscdevice stmvl53l0x_ranging_dev = {
	.minor =	MISC_DYNAMIC_MINOR,
	.name =		"stmvl53l0x_ranging",
	.fops =		&stmvl53l0x_ranging_fops
};
*/

int stmvl53l0x_setup(struct stmvl53l0x_data *data)
{
	int rc = 0;

#ifdef USE_INT
	int irq = 0;
#endif

	vl53l0x_dbgmsg("Enter\n");

	/* init mutex */
	mutex_init(&data->update_lock);
	mutex_init(&data->work_mutex);

#ifdef USE_INT
	/* init interrupt */
	gpio_request(IRQ_NUM, "vl53l0x_gpio_int");
	gpio_direction_input(IRQ_NUM);
	irq = gpio_to_irq(IRQ_NUM);
	if (irq < 0) {
		vl53l0x_errmsg("filed to map GPIO: %d to interrupt:%d\n",
			IRQ_NUM, irq);
	} else {
		vl53l0x_dbgmsg("register_irq:%d\n", irq);
		/* IRQF_TRIGGER_FALLING- poliarity:0 IRQF_TRIGGER_RISNG -
		poliarty:1 */
		rc = request_threaded_irq(irq, NULL,
				stmvl53l0x_interrupt_handler,
				IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
				"vl53l0x_interrupt",
				(void *)data);
		if (rc) {
			vl53l0x_errmsg(
"%d, Could not allocate STMVL53L0X_INT ! result:%d\n",  __LINE__, rc);
			free_irq(irq, data);
			goto exit_free_irq;
		}
	}
	data->irq = irq;
	vl53l0x_errmsg("interrupt is hooked\n");
#else

	init_waitqueue_head(&data->poll_thread_wq);

	data->poll_thread = kthread_run(&stmvl53l0x_poll_thread,
									(void *)data,
									"STM-VL53L0");
	if (data->poll_thread == NULL) {
		pr_err("%s(%d) - Failed to create Polling thread\n", __FUNCTION__, __LINE__);
		goto exit_free_irq;
	}
#endif

	/* init work handler */
	INIT_DELAYED_WORK(&data->dwork, stmvl53l0x_work_handler);

	/* Register to Input Device */
	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		rc = -ENOMEM;
		vl53l0x_errmsg("%d error:%d\n", __LINE__, rc);
/*
#ifdef USE_INT
		free_irq(irq, data);
#endif
*/
		goto exit_free_irq;
	}
	set_bit(EV_ABS, data->input_dev_ps->evbit);
	/* range in cm*/
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 76, 0, 0);
	/* tv_sec */
	input_set_abs_params(data->input_dev_ps, ABS_HAT0X, 0, 0xffffffff,
		0, 0);
	/* tv_usec */
	input_set_abs_params(data->input_dev_ps, ABS_HAT0Y, 0, 0xffffffff,
		0, 0);
	/* range in_mm */
	input_set_abs_params(data->input_dev_ps, ABS_HAT1X, 0, 765, 0, 0);
	/* error code change maximum to 0xff for more flexibility */
	input_set_abs_params(data->input_dev_ps, ABS_HAT1Y, 0, 0xff, 0, 0);
	/* rtnRate */
	input_set_abs_params(data->input_dev_ps, ABS_HAT2X, 0, 0xffffffff,
		0, 0);
	/* rtn_amb_rate */
	input_set_abs_params(data->input_dev_ps, ABS_HAT2Y, 0, 0xffffffff,
		0, 0);
	/* rtn_conv_time */
	input_set_abs_params(data->input_dev_ps, ABS_HAT3X, 0, 0xffffffff,
		0, 0);
	/* dmax */
	input_set_abs_params(data->input_dev_ps, ABS_HAT3Y, 0, 0xffffffff,
		0, 0);

	input_set_abs_params(data->input_dev_ps, ABS_PRESSURE, 0, 0xffffffff,
		0, 0);

	input_set_abs_params(data->input_dev_ps, ABS_WHEEL , 0, 0xffffffff,
		0, 0);

	data->input_dev_ps->name = "STM VL53L0 proximity sensor";

	rc = input_register_device(data->input_dev_ps);
	if (rc) {
		rc = -ENOMEM;
		vl53l0x_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_free_dev_ps;
	}
	/* setup drv data */
	input_set_drvdata(data->input_dev_ps, data);

	/* Register sysfs hooks */
	data->range_kobj = kobject_create_and_add("range", kernel_kobj);
	if (!data->range_kobj) {
		rc = -ENOMEM;
		vl53l0x_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_unregister_dev_ps;
	}
	rc = sysfs_create_group(&data->input_dev_ps->dev.kobj,
			&stmvl53l0x_attr_group);
	if (rc) {
		rc = -ENOMEM;
		vl53l0x_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_unregister_dev_ps_1;
	}

	/* to register as a misc device */
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name = "stmvl53l0x_ranging";
	data->miscdev.fops = &stmvl53l0x_ranging_fops;
	vl53l0x_errmsg("Misc device registration name:%s\n", data->dev_name);
	if (misc_register(&data->miscdev) != 0)
		vl53l0x_errmsg(
"Could not register misc. dev for stmvl53l0	ranging\n");

	/* init default device parameter value */
	data->enable_ps_sensor = 0;
	data->reset = 1;
	data->delay_ms = 30;	/* delay time to 30ms */
	data->enableDebug = 0;
	data->gpio_polarity = VL53L0X_INTERRUPTPOLARITY_LOW;
	data->gpio_function = VL53L0X_GPIOFUNCTIONALITY_NEW_MEASURE_READY;
	data->low_threshold = 60;
	data->high_threshold = 200;
	data->deviceMode = VL53L0X_DEVICEMODE_SINGLE_RANGING;
	data->interMeasurems = 30;
	data->timingBudget = 26000;
	data->useLongRange = 1;

	vl53l0x_dbgmsg("support ver. %s enabled\n", DRIVER_VERSION);
	vl53l0x_dbgmsg("End");

	return 0;
exit_unregister_dev_ps_1:
	kobject_put(data->range_kobj);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
exit_free_irq:
#ifdef USE_INT
	free_irq(irq, data);
#endif
	kfree(data);
	return rc;
}

void stmvl53l0x_cleanup(struct stmvl53l0x_data *data)
{
#ifndef USE_INT
	pr_err("%s(%d) : Stop poll_thread\n", __FUNCTION__, __LINE__);
	poll_thread_exit = 1;
	kthread_stop(data->poll_thread);
#endif
}
static int __init stmvl53l0x_init(void)
{
	int ret = -1;

	vl53l0x_dbgmsg("Enter\n");

	/* assign function table */
	pmodule_func_tbl = &stmvl53l0x_module_func_tbl;
	papi_func_tbl = &stmvl53l0x_api_func_tbl;

	/* client specific init function */
	ret = pmodule_func_tbl->init();

	if (ret)
		vl53l0x_errmsg("%d failed with %d\n", __LINE__, ret);

	vl53l0x_dbgmsg("End\n");

	return ret;
}

static void __exit stmvl53l0x_exit(void)
{
	vl53l0x_dbgmsg("Enter\n");

	vl53l0x_dbgmsg("End\n");
}


MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(stmvl53l0x_init);
module_exit(stmvl53l0x_exit);

