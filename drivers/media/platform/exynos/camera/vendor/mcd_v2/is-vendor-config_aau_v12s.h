#ifndef IS_VENDOR_CONFIG_AAU_V12S_H
#define IS_VENDOR_CONFIG_AAU_V12S_H

#define VENDER_PATH

/***** CAL ROM DEFINE *****/
#define ROM_DEBUG
#define ROM_CRC32_DEBUG
//#define SKIP_CHECK_CRC                                      /* Skip the CRC CHECK of cal data */

#define SUPPORT_SENSOR_DUALIZATION                            /* Front SR846 / S5K4HA */

#define USE_CAMERA_IOVM_BEST_FIT

/***** SUPPORT CAMERA DEFINE *****/
#define IS_VENDOR_SENSOR_COUNT 5                              /* REAR_0, FRONT_0, REAR_2, REAR_3, REAR_4 */

#define REAR_SUB_CAMERA            (SENSOR_POSITION_REAR2)    /* Supported Camera Type for rear bokeh */
#define REAR_ULTRA_WIDE_CAMERA     (SENSOR_POSITION_REAR3)    /* Supported Camera Type for rear ultra wide */
#define REAR_MACRO_CAMERA          (SENSOR_POSITION_REAR4)    /* Supported Camera Type for rear macro */

#define CAMERA_REAR2               (REAR_ULTRA_WIDE_CAMERA)   /* For Rear2 of SYSFS */
#define CAMERA_REAR3               (REAR_SUB_CAMERA)          /* For Rear3 of SYSFS */
#define CAMERA_REAR4               (REAR_MACRO_CAMERA)        /* For Rear4 of SYSFS */

#define CAMERA_REAR2_MODULEID
#define CAMERA_REAR3_MODULEID
#define CAMERA_REAR_DUAL_CAL

#define USES_STANDARD_CAL_RELOAD
#define USE_PERSISTENT_DEVICE_PROPERTIES_FOR_CAL /* For cal reload */

#define CONFIG_SEC_CAL_ENABLE
#define IS_REAR_MAX_CAL_SIZE (0x4000)
#define IS_FRONT_MAX_CAL_SIZE (0x1C90)
#define IS_REAR2_MAX_CAL_SIZE (0x11)
#define IS_REAR3_MAX_CAL_SIZE (0x1A58)
#define IS_REAR4_MAX_CAL_SIZE (0x1BA0)

#define CONFIG_CAMERA_EEPROM_DUALIZED

/***** SUPPORT FUCNTION DEFINE *****/
#define SAMSUNG_LIVE_OUTFOCUS                                 /* Allocate memory For Dual Camera */
#define ENABLE_REMOSAIC_CAPTURE                               /* Base Remosaic */
#define ENABLE_REMOSAIC_CAPTURE_WITH_ROTATION                 /* M2M and Rotation is used during Remosaic */
#define CHAIN_USE_STRIPE_PROCESSING  1                        /* Support STRIPE_PROCESSING during Remosaic */
#define USE_AP_PDAF                                           /* Support sensor PDAF SW Solution */
#define DISABLE_DUAL_SYNC
#define USE_MONO_SENSOR
#define CAMERA_STANDARD_CAL_ISP_VERSION 'E'

/***** DDK - DRIVER INTERFACE *****/
#define USE_DEBUG_LIBRARY_VER

/***** HW DEFENDANT DEFINE *****/
#define USE_CAMERA_ACT_DRIVER_SOFT_LANDING                    /* Using NRC based softlanding for FP5529 */
#define USE_COMMON_CAM_IO_PWR                                 /* CAM_VDDIO_1P8 Power is used commonly for all camera and EEPROM */

/***** SUPPORT EXTERNEL FUNCTION DEFINE *****/
#define USE_SSRM_CAMERA_INFO                                  /* Match with SAMSUNG_SSRM define of Camera Hal side */

#define USE_CAMERA_HW_BIG_DATA
#ifdef USE_CAMERA_HW_BIG_DATA
#define CSI_SCENARIO_SEN_REAR	(0)                           /* This value follows dtsi */
#define CSI_SCENARIO_SEN_FRONT	(1)
#endif

#define USE_SPECIFIC_MIPISPEED

#endif /* IS_VENDOR_CONFIG_AAU_V12S_H */
