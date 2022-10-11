// Twi interface and macros/defines defined by you.
#include "CommonDefinesAndMacros.h"

/*
 * Note:    This was originally used with FreeRTOS so delays were doe through vTaskDelay. 
 *          These lines are commented out but replace with your own delay() call.
*/

#include "FuelGauge.h"
#include "GoldenImage.h"


/**
 *  Defines
 */
#define FUEL_GAUGE_I2C_ADDRESS              0x55 // 0xAA is the 8-bit address
#define FUEL_GAUGE_ROM_I2C_ADDRESS          0x0B // 0x16 is the 8-bit address
#define FUEL_GAUGE_TWI_SPEED                TWI_100KHZ

#define FUEL_GAUGE_REG_CONTROL_STATUS       0x00
#define FUEL_GAUGE_REG_VOLT                 0x08
#define FUEL_GAUGE_REG_BATTERY_STATUS       0x0A
#define FUEL_GAUGE_REG_CURRENT              0x0C
#define FUEL_GAUGE_REG_REMAINING_CAP        0x10
#define FUEL_GAUGE_REG_FULL_CAP             0x12
#define FUEL_GAUGE_REG_RELATIVE_SOC         0x2C
#define FUEL_GAUGE_REG_RELATIVE_SOH         0x2E
#define FUEL_GAUGE_REG_DESIGN_CAP           0x3C
#define FUEL_GAUGE_REG_ALT_MNFG_ACCESS      0x3E

#define FUEL_GAUGE_REG_MAC_DATA_SUM         0x60
#define FUEL_GAUGE_REG_VOLT_HI_SET_TH       0x62
#define FUEL_GAUGE_REG_VOLT_HI_CLR_TH       0x64
#define FUEL_GAUGE_REG_VOLT_LO_SET_TH       0x66
#define FUEL_GAUGE_REG_VOLT_LO_CLR_TH       0x68
#define FUEL_GAUGE_REG_TEMP_HI_SET_TH       0x6A
#define FUEL_GAUGE_REG_TEMP_HI_CLR_TH       0x6B
#define FUEL_GAUGE_REG_TEMP_LO_SET_TH       0x6C
#define FUEL_GAUGE_REG_TEMP_LO_CLR_TH       0x6D

#define FUEL_GAUGE_DF_POWER_CONFIG          0x4643
#define FUEL_GAUGE_IT_ENABLED_BIT           3
#define FUEL_GAUGE_LF_ENABLED_BIT           5

#define NUMBER_FUEL_GAUGE_SETUP_REGISTERS   ARRAY_COUNT(registerSetup)
#define FUEL_GAUGE_ENABLE_DELAY             1900
#define FUEL_GAUGE_I2C_DELAY                1


/**
 *  Local data
 */
typedef struct {
    const uint8_t *data;
    const uint16_t startAddress;
    const uint8_t size;
} Block;

typedef enum {
    UNSEAL_KEY,
    FULL_ACCESS_KEY,
} FuelGaugeSecurityKey;

// keys
static const uint8_t unsealKey [] = {0x04, 0x14, 0x36, 0x72};
static const uint8_t fullAccessKey [] = {0xff, 0xff, 0xff, 0xff};

// status commands (in little-endian format)
static const uint8_t chemIdCmd [] = {0x06, 0x00};
static const uint8_t operationStatusCommand [] = {0x54, 0x00};
static const uint8_t chargingStatusCommand [] = {0x55, 0x00};
static const uint8_t gaugingStatusCommand [] = {0x56, 0x00};
static const uint8_t manufacturingStatusCommand [] = {0x57, 0x00};
static const uint8_t updateStatusAddress [] = {0x8c, 0x41};
// setup commands (in little-endian format)
static const uint8_t enableImpedanceTrackingCommand [] = {0x21, 0x00};
static const uint8_t disableImpedanceTrackingCommand [] = {0x21, 0x00};
static const uint8_t lifetimeTrackingCommand [] = {0x23, 0x00};
static const uint8_t resetCommand [] = {0x41, 0x00};
static const uint8_t sealCommand [] = {0x30, 0x00};
static const uint8_t resetLifetimeCmd [] = {0x28, 0x00};
static const uint8_t securityKeysCmd [] = {0x35, 0x00};
static const uint8_t exitRomCmd [] = {0x08};
//static const uint8_t enterRomCmd [] = {0x00, 0x0f}; // be careful!

static TwiInterface *Twi = NULL;

/**
 *  Local function prototypes
 */
static inline bool PrimedReadOperation(const uint8_t registerAddress,
                                       const uint8_t *cmd,
                                       const uint8_t sizeOfCmd,
                                       uint8_t *data,
                                       const uint8_t sizeOfData);
static inline bool ReadFlashBlock(const uint8_t fgAddress,
                                  const uint8_t registerAddress,
                                  uint8_t *value,
                                  const uint8_t size);
static inline bool WriteFlashBlock(const uint8_t registerAddress,
                                   const uint8_t *value,
                                   const uint8_t size);
static inline bool WriteFlashBlockSafe(const uint8_t fgAddress,
                                       const uint8_t registerAddress,
                                       const uint8_t *value,
                                       const uint8_t size);
static inline bool GetCommon(const uint8_t registerAddress,
                             uint16_t *value);
static inline bool IsImpedanceTrackingEnabled(void);
static inline bool IsLifetimeTrackingEnabled(void);
static inline void GetKey(FuelGaugeSecurityKey desiredKey, uint8_t *key);


void FuelGaugeInitTwi(TwiInterface *twi)
{
    configASSERT(twi != NULL);

    Twi = twi;
}

/**
* \brief Gets control status from the BQ27Z561.
*/
bool FuelGaugeGetControlStatus(uint16_t *controlStatus)
{
    return GetCommon(FUEL_GAUGE_REG_CONTROL_STATUS, controlStatus);
}

/**
* \brief Gets voltage from the BQ27Z561.
*/
bool FuelGaugeGetVoltage(uint16_t *voltage)
{
    return GetCommon(FUEL_GAUGE_REG_VOLT, voltage);
}

/**
* \brief Gets status of battery from the BQ27Z561.
*/
bool FuelGaugeGetBatteryStatus(uint16_t *status)
{
    return GetCommon(FUEL_GAUGE_REG_BATTERY_STATUS, status);
}

/**
* \brief Gets manufacturing status of battery from the BQ27Z561.
*/
bool FuelGaugeGetManufacturingStatus(uint16_t *status)
{
    uint8_t values [] = {0xff, 0xff, 0xff, 0xff};

    bool result = PrimedReadOperation(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                      manufacturingStatusCommand,
                                      sizeof(manufacturingStatusCommand),
                                      values,
                                      sizeof(values));

    (*status) = ((values[3] << 8) | values[2]);

    return result;
}

/**
* \brief Gets operation status for the BQ27Z561.
*/
bool FuelGaugeGetOperationStatus(uint32_t *opStatus)
{
    uint8_t values [] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    bool result = PrimedReadOperation(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                      operationStatusCommand,
                                      sizeof(operationStatusCommand),
                                      values,
                                      sizeof(values));

    (*opStatus) = ((values[3] << 24) | (values[2] << 16) | (values[5] << 8) | values[4]);

    return result;
}

/**
* \brief Gets gauging status for the BQ27Z561.
*/
bool FuelGaugeGetGaugingStatus(uint32_t *gaugingStatus)
{
    uint8_t values [] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    bool result = PrimedReadOperation(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                      gaugingStatusCommand,
                                      sizeof(gaugingStatusCommand),
                                      values,
                                      sizeof(values));

    (*gaugingStatus) = ((values[3] << 24) | (values[2] << 16) | (values[5] << 8) | values[4]);

    return result;
}

/**
* \brief Gets charging status for the BQ27Z561.
*/
bool FuelGaugeGetChargingStatus(uint32_t *chargingStatus)
{
    uint8_t values [] = {0xff, 0xff, 0xff, 0xff, 0xff};

    bool result = PrimedReadOperation(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                      chargingStatusCommand,
                                      sizeof(chargingStatusCommand),
                                      values,
                                      sizeof(values));

    // Returns three bytes. Upper byte is Temp range, lower 2 bytes are charging status flags.
    (*chargingStatus) = 0x0;
    (*chargingStatus) = ((values[2] << 16) | (values[4] << 8) | values[3]);

    return result;
}

/**
* \brief Gets Chem ID of battery programmed on the BQ27Z561.
*/
bool FuelGaugeGetChemId(uint16_t *chemId)
{
    uint8_t values [] = {0xff, 0xff, 0xff, 0xff};

    bool result = PrimedReadOperation(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                      chemIdCmd,
                                      sizeof(chemIdCmd),
                                      values,
                                      sizeof(values));

    (*chemId) = ((values[3] << 8) | values[2]);

    return result;
}

bool FuelGaugeGetUpdateStatus(uint8_t *updateStatus)
{
    uint8_t value [] = {0xff, 0xff, 0xff};

    bool result = PrimedReadOperation(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                      updateStatusAddress,
                                      ARRAY_COUNT(updateStatusAddress),
                                      value,
                                      sizeof(value));

    (*updateStatus) = value[2];

    return result;
}

/**
* \brief Gets current from the BQ27Z561.
*/
bool FuelGaugeGetCurrent(int16_t *current)
{
    return GetCommon(FUEL_GAUGE_REG_CURRENT, (uint16_t *) current);
}

/**
* \brief Gets remaining battery capacity from the BQ27Z561.
*/
bool FuelGaugeGetRemainingCapacity(uint16_t *capacity)
{
    return GetCommon(FUEL_GAUGE_REG_REMAINING_CAP, capacity);
}

/**
* \brief Gets predicted fully-charged battery capacity from the BQ27Z561.
*/
bool FuelGaugeGetFullChargeCapacity(uint16_t *capacity)
{
    return GetCommon(FUEL_GAUGE_REG_FULL_CAP, capacity);
}

/**
* \brief Gets relative state-of-charge (SoC) as % from the BQ27Z561.
*/
bool FuelGaugeGetRelativeSoc(uint16_t *soc)
{
    return GetCommon(FUEL_GAUGE_REG_RELATIVE_SOC, soc);
}

/**
* \brief Gets the state-of-health (SoH) information of the battery in
*   percentage of Design Capacity from the BQ27Z561.
*/
bool FuelGaugeGetSoh(uint16_t *soh)
{
    return GetCommon(FUEL_GAUGE_REG_RELATIVE_SOH, soh);
}

/**
* \brief Gets design capacity for the BQ27Z561.
*/
bool FuelGaugeGetCapacity(uint16_t *capacity)
{
    return GetCommon(FUEL_GAUGE_REG_DESIGN_CAP, capacity);
}

/**
* \brief Enable the Impedance Tracking algorithm on the BQ27Z561.
*/
bool FuelGaugeEnableImpedanceTracking(void)
{
    if (IsImpedanceTrackingEnabled() == true) // IT already enabled
        return true;

    WriteFlashBlockSafe(FUEL_GAUGE_I2C_ADDRESS,
                        FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                        enableImpedanceTrackingCommand,
                        sizeof(enableImpedanceTrackingCommand));

    return IsImpedanceTrackingEnabled();
}

/**
* \brief Disable the Impedance Tracking algorithm on the BQ27Z561.
*/
bool FuelGaugeDisableImpedanceTracking(void)
{
    if (IsImpedanceTrackingEnabled() == true) {
        WriteFlashBlockSafe(FUEL_GAUGE_I2C_ADDRESS,
                            FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                            disableImpedanceTrackingCommand,
                            sizeof(disableImpedanceTrackingCommand));
    }

    return IsImpedanceTrackingEnabled();
}

/**
* \brief Enable the Lifetime Tracking algorithm on the BQ27Z561.
*/
bool FuelGaugeEnableLifetimeTracking(void)
{
    if (IsLifetimeTrackingEnabled() == true) // Lifetime already enabled
        return true;

    WriteFlashBlockSafe(FUEL_GAUGE_I2C_ADDRESS,
                        FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                        lifetimeTrackingCommand,
                        sizeof(lifetimeTrackingCommand));

    return IsLifetimeTrackingEnabled();
}

/**
* \brief Disable the Lifetime Tracking algorithm on the BQ27Z561.
*/
bool FuelGaugeDisableLifetimeTracking(void)
{
    if (IsImpedanceTrackingEnabled() == true) {
        WriteFlashBlockSafe(FUEL_GAUGE_I2C_ADDRESS,
                            FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                            lifetimeTrackingCommand,
                            sizeof(lifetimeTrackingCommand));
    }

    return IsLifetimeTrackingEnabled();
}

/**
* \brief Resets the BQ27Z561.
*/
bool FuelGaugeReset(void)
{
    bool result = WriteFlashBlockSafe(FUEL_GAUGE_I2C_ADDRESS,
                                      FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                      resetCommand,
                                      sizeof(resetCommand));

    return result;
}

/**
* \brief Unseals the BQ27Z561.
*/
bool FuelGaugeUnseal(void)
{
    configASSERT(Twi != NULL);

    uint8_t keyFirstWord[] = {unsealKey[0], unsealKey[1]};
    uint8_t keySecondWord[] = {unsealKey[2], unsealKey[3]};

    bool result = false;

    if (Twi->open(FUEL_GAUGE_TWI_SPEED) == true) {
        result = WriteFlashBlock(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                 keyFirstWord,
                                 ARRAY_COUNT(keyFirstWord));

        result &= WriteFlashBlock(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                  keySecondWord,
                                  ARRAY_COUNT(keySecondWord));

        Twi->close();
    }

    return result;
}

/**
* \brief Gives full access to the BQ27Z561.
*/
bool FuelGaugeFullAccess(void)
{
    configASSERT(Twi != NULL);
    
    uint8_t keyFirstWord[] = {fullAccessKey[0], fullAccessKey[1]};
    uint8_t keySecondWord[] = {fullAccessKey[2], fullAccessKey[3]};

    bool result = false;

    if (Twi->open(FUEL_GAUGE_TWI_SPEED) == true) {
        result = WriteFlashBlock(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                 keyFirstWord,
                                 ARRAY_COUNT(keyFirstWord));

        result &= WriteFlashBlock(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                                  keySecondWord,
                                  ARRAY_COUNT(keySecondWord));

        Twi->close();
    }

    return result;
}

/**
* \brief Seals the BQ27Z561.
*/
bool FuelGaugeSeal(void)
{
    return WriteFlashBlockSafe(FUEL_GAUGE_I2C_ADDRESS,
                               FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                               sealCommand,
                               sizeof(sealCommand));
}

/**
* \brief Resets lifetime history of battery BQ27Z561.
*/
bool FuelGaugeResetLifetimeHistory(void)
{
    return WriteFlashBlockSafe(FUEL_GAUGE_I2C_ADDRESS,
                               FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                               resetLifetimeCmd,
                               sizeof(resetLifetimeCmd));
}

/**
* \brief Send return-to-firmware command if BQ27Z561 is in ROM mode.
*/
bool FuelGaugeExitRomMode(void)
{
    configASSERT(Twi != NULL);

    bool result = false;

    if (Twi->open(TWI_400KHZ) == true) {
        result = Twi->write(FUEL_GAUGE_ROM_I2C_ADDRESS,
                              (uint8_t *)FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                              sizeof(uint8_t),
                              exitRomCmd,
                              sizeof(exitRomCmd));

        Twi->close();
    }

    return result;
}

/**
* \brief Execute a flash stream file onto BQ27Z561.
*/
FuelGaugeConfigError FuelGaugeExecuteGoldenImage(void)
{
    configASSERT(Twi != NULL);

    uint16_t length = strlen(goldenImage);
    char buf[16];
    uint16_t index = 0;

    while (index < length) {
        switch (goldenImage[index]) {
            // writing/reading fuel gauge
            case 'W':
            case 'C': {
                bool writeCmd = (goldenImage[index++] == 'W');

                if (goldenImage[index++] != ':')
                    return ERROR_COLON;

                uint16_t byteNum = 1;
                uint8_t data[34];
                uint8_t fgAddress = 0;
                unsigned char fgRegister = 0;

                while ((length - index > 2) && (byteNum < sizeof(data) + 3) && (goldenImage[index] != '\n')) {
                    // copy two values from file
                    buf[0] = goldenImage[index++];
                    buf[1] = goldenImage[index++];
                    // set zero-terminated
                    buf[2] = 0;
                    // convert string to hex
                    char *pError;
                    uint8_t convertedData = strtoul(buf, &pError, 16);

                    // check if conversion was successful
                    if (*pError)
                        return ERROR_CONV;

                    // handle address
                    if (byteNum == 1)
                        fgAddress = convertedData;

                    // set register
                    if (byteNum == 2)
                        fgRegister = convertedData;

                    // construct data from file
                    if (byteNum > 2)
                        data[byteNum - 3] = convertedData;

                    byteNum++;
                }

                if (byteNum < 4)
                    return ERROR_COUNT;

                uint8_t dataLength = byteNum - 3;

                if (writeCmd) {
                    // the data in the golden image file is in little endian format
                    WriteFlashBlockSafe((fgAddress >> 1), fgRegister, data, dataLength);
                } else {
                    uint8_t dataFromGauge[dataLength];

                    if (Twi->open(FUEL_GAUGE_TWI_SPEED) == true) {
                        ReadFlashBlock((fgAddress >> 1), fgRegister, dataFromGauge, dataLength);
                        Twi->close();
                    }

                    if (memcmp(data, dataFromGauge, dataLength))
                        return ERROR_MEMCMP;
                }

                break;
            }

            // handle delay
            case 'X': {
                index++;

                if (goldenImage[index] != ':')
                    return ERROR_COLON;

                index++;
                uint8_t n = 0;

                while ((index < length) && (goldenImage[index] != '\n') && (n < sizeof(buf) - 1))
                    buf[n++] = goldenImage[index++];

                // set zero-terminated
                buf[n] = 0;
                n = atoi(buf);
                //vTaskDelay(n);

                break;
            }

            default:
                return ERROR_DEFAULT;
        }

        while ((index < length) && (goldenImage[index] != '\n'))
            index++; //skip to next line

        if (index < length)
            index++;
    }

    return ERROR_NONE;
}

/***********************************************************************
   Static functions.
***********************************************************************/
static inline bool GetCommon(const uint8_t registerAddress,
                             uint16_t *value)
{
    configASSERT(Twi != NULL);

    bool result = false;

    if (Twi->open(FUEL_GAUGE_TWI_SPEED) == true) {
        result = Twi->read(FUEL_GAUGE_I2C_ADDRESS, &registerAddress, sizeof(uint8_t), value,
                             sizeof(uint16_t));
        Twi->close();
        // minimum 66-us delay required before next I2C transaction
        //vTaskDelay(FUEL_GAUGE_I2C_DELAY);
    }

    return result;
}

// We have the ability to read the keys, but I also hard-coded const declarations of both keys to simplify
static inline void GetKey(FuelGaugeSecurityKey desiredKey, uint8_t *key)
{
    uint8_t values [] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    PrimedReadOperation(FUEL_GAUGE_REG_ALT_MNFG_ACCESS,
                        securityKeysCmd,
                        sizeof(securityKeysCmd),
                        values,
                        sizeof(values));

    if (desiredKey == UNSEAL_KEY) {
        uint8_t unsealedKey [] = {values[2], values[3], values[4], values[5]};
        memcpy(key, unsealedKey, sizeof(unsealedKey));
    } else if (desiredKey == FULL_ACCESS_KEY) {
        uint8_t fullAccKey [] = {values[6], values[7], values[8], values[9]};
        memcpy(key, fullAccKey, sizeof(fullAccKey));
    }
}

static inline bool PrimedReadOperation(const uint8_t registerAddress,
                                       const uint8_t *cmd,
                                       const uint8_t sizeOfCmd,
                                       uint8_t *data,
                                       const uint8_t sizeOfData)
{
    configASSERT(Twi != NULL);

    bool result = false;

    if (Twi->open(FUEL_GAUGE_TWI_SPEED) == true) {
        // Configure pointer to flash location
        result = WriteFlashBlock(registerAddress, cmd, sizeOfCmd);
        // Read from desired flash location
        result &= ReadFlashBlock(FUEL_GAUGE_I2C_ADDRESS, registerAddress, data, sizeOfData);

        Twi->close();
    }

    return result;
}

static inline bool IsImpedanceTrackingEnabled(void)
{
    uint16_t manfStatus;
    FuelGaugeGetManufacturingStatus(&manfStatus);

    bool result = (BIT_IS_SET(manfStatus, FUEL_GAUGE_IT_ENABLED_BIT));

    return result;
}

static inline bool IsLifetimeTrackingEnabled(void)
{
    uint16_t manfStatus;
    FuelGaugeGetManufacturingStatus(&manfStatus);

    bool result = (BIT_IS_SET(manfStatus, FUEL_GAUGE_LF_ENABLED_BIT));

    return result;
}

static inline bool ReadFlashBlock(const uint8_t fgAddress,
                                  const uint8_t registerAddress,
                                  uint8_t *value,
                                  const uint8_t size)
{
    configASSERT(Twi != NULL);

    bool result = Twi->read(fgAddress,
                            &registerAddress,
                            sizeof(uint8_t),
                            value,
                            size);

    // vTaskDelay(FUEL_GAUGE_I2C_DELAY); // minimum 66-us delay required before next I2C transaction

    return result;
}

static inline bool WriteFlashBlock(const uint8_t registerAddress,
                                   const uint8_t *value,
                                   const uint8_t size)
{
    configASSERT(Twi != NULL);

    bool result = Twi->write(FUEL_GAUGE_I2C_ADDRESS,
                             &registerAddress,
                             sizeof(uint8_t),
                             value,
                             size);

    //vTaskDelay(FUEL_GAUGE_I2C_DELAY); // minimum 66-us delay required before next I2C transaction

    return result;
}

static inline bool WriteFlashBlockSafe(const uint8_t fgAddress,
                                       const uint8_t registerAddress,
                                       const uint8_t *value,
                                       const uint8_t size)
{
    configASSERT(Twi != NULL);

    bool result = false;

    if (Twi->open(FUEL_GAUGE_TWI_SPEED) == true) {
        result = Twi->write(fgAddress,
                            &registerAddress,
                            sizeof(uint8_t),
                            value,
                            size);

        Twi->close();
    }

    //vTaskDelay(FUEL_GAUGE_I2C_DELAY); // minimum 66-us delay required before next I2C transaction

    return result;
}
