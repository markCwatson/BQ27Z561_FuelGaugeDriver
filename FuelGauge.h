#ifndef SYSTEM_MONITOR_FUEL_GAUGE_H_
#define SYSTEM_MONITOR_FUEL_GAUGE_H_

/*
 * Note:    I2C/Twi interface is defined by you.
 *          This implementation includes open, write, read, and close methods.
 *
*/
#include "TwiInterface.h"

#include <stdint.h>


typedef enum {
    ERROR_NONE,
    ERROR_COLON,
    ERROR_CONV,
    ERROR_COUNT,
    ERROR_MEMCMP,
    ERROR_DEFAULT,
} FuelGaugeConfigError;


/**
* \brief Setup an I2C/TWI interface.
*
* \param TwiInterface *twi Pointer to an I2C/TWI interface.
*/
void FuelGaugeInitTwi(TwiInterface *twi);

/**
* \brief Gets control status from the BQ27Z561.
*
* \param controlStatus.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetControlStatus(uint16_t *controlStatus);

/**
* \brief Gets voltage from the BQ27Z561.
*
* \param voltage in mV (0 to 6000 mV).
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetVoltage(uint16_t *voltage);

/**
* \brief Gets status of battery from the BQ27Z561.
*
* \param bitmask of status register.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetBatteryStatus(uint16_t *status);

/**
* \brief Gets manufacturing status of battery from the BQ27Z561.
*
* \param bitmask of status register.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetManufacturingStatus(uint16_t *status);

/**
* \brief Gets current from the BQ27Z561.
*
* \param current in mA (0 to +/- 32767 mA).
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetCurrent(int16_t *current);

/**
* \brief Gets remaining battery capacity from the BQ27Z561.
*
* \param capacity in mAh (0 to 32767 mAh).
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetRemainingCapacity(uint16_t *capacity);

/**
* \brief Gets predicted fully-charged battery capacity from the BQ27Z561.
*
* \param capacity in mAh (0 to 32767 mAh).
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetFullChargeCapacity(uint16_t *capacity);

/**
* \brief Gets relative state-of-charge (SoC) as % from the BQ27Z561.
*
* \param SoC in %.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetRelativeSoc(uint16_t *capacity);

/**
* \brief Gets the state-of-health (SoH) information of the battery in
* percentage of Design Capacity from the BQ27Z561.
*
* \param SoH in %.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetSoh(uint16_t *capacity);

/**
* \brief Gets design capacity for the BQ27Z561.
*
* \param capacity in mAh.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetCapacity(uint16_t *capacity);

/**
* \brief Gets operation status for the BQ27Z561.
*
* \param operation status.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetOperationStatus(uint32_t *opStatus);

/**
* \brief Gets gauging status for the BQ27Z561.
*
* \param operation status.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetGaugingStatus(uint32_t *gaugingStatus);

/**
* \brief Gets update status from the BQ27Z561.
*
* \param update status.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetUpdateStatus(uint8_t *updateStatus);

/**
* \brief Gets charging status for the BQ27Z561.
*
* \param charging status.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetChargingStatus(uint32_t *chargingStatus);

/**
* \brief Gets Chem ID of battery programmed on the BQ27Z561.
*
* \param the chem ID.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeGetChemId(uint16_t *chemId);

/**
* \brief Enable the Impedance Tracking algorithm on the BQ27Z561.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeEnableImpedanceTracking(void);

/**
* \brief Enable the Lifetime Tracking algorithm on the BQ27Z561.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeEnableLifetimeTracking(void);

/**
* \brief Disable the Lifetime Tracking algorithm on the BQ27Z561.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeDisableLifetimeTracking(void);

/**
* \brief Disables the Impedance Tracking algorithm on the BQ27Z561.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeDisableImpedanceTracking(void);

/**
* \brief Resets the BQ27Z561.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeReset(void);

/**
* \brief Unseals the BQ27Z561.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeUnseal(void);

/**
* \brief Gives full access to the BQ27Z561.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeFullAccess(void);

/**
* \brief Seals the BQ27Z561. Be careful using this.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeSeal(void);

/**
* \brief Resets lifetime history of battery BQ27Z561.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeResetLifetimeHistory(void);

/**
* \brief Send return-to-firmware command if BQ27Z561 is in ROM mode.
*
* \return true if successful, false otherwise.
*/
bool FuelGaugeExitRomMode(void);

/**
* \brief Execute a flash stream file onto BQ27Z561.
*
* \return FuelGaugeConfigError.
*/
FuelGaugeConfigError FuelGaugeExecuteGoldenImage(void);

#endif  // SYSTEM_MONITOR_FUEL_GAUGE_H_
