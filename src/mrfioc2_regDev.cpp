/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** drvmrfioc2_regDev.cpp
**
** RegDev device support for Distributed Buffer on MRF EVR and EVG cards using
** mrfioc2 driver.
**
** -- DOCS ----------------------------------------------------------------
** Driver is registered via iocsh command:
**         mrfioc2_regDevConfigure <regDevName> <mrfName> <protocol ID>
**
**             -regDevName: name of device as seen from regDev. E.g. this
**                         name must be the same as parameter 1 in record OUT/IN
**
**             -mrfName: name of mrf device
**
**             -protocol ID: protocol id (32 bit int). If set to 0, than receiver
**                         will accept all buffers. This is useful for
**                         debugging. If protocol != 0 then only received buffers
**                         with same protocol id are accepted. If you need to work
**                         with multiple protocols you can register multiple instances
**                         of regDev using the same mrfName but different regDevNames and
**                         protocols.
**
**
**         example:    mrfioc2_regDevConfigure EVGDBUFF EVG1 42
**
**
** EPICS use:
**
**         - records behave the same as for any other regDev device with one exception:
**
**         - offset 0x00-0x04 can not be written to, since it is occupied by protocolID
**         - writing to offset 0x00 will flush the buffer
**         - input records are automatically processed (if scan == IO) when a valid buffer
**             is received
**
**
**
** -- SUPPORTED DEVICES -----------------------------------------------------
**
** VME EVG-230 (tx only, EVG does not support databuffer rx)
** VME EVR-230 (tx and rx)
** PCI EVR-230 (rx only, firmware version 3 does not support databuffer tx)
** PCIe EVR-300 (tx and rx)
**
**
** -- IMPLEMENTATION ---------------------------------------------------------
**
** In order to sync endianess across different devices and buses a following
** convention is followed.
**         - Data in distributed buffer is always BigEndian (4321). This also includes
**         data-types that are longer than 4bytes (e.g. doubles are 7654321)
**
**         - Data in scratch buffers (device->txBuffer, device->rxBuffer) is in
**         the same format as in hw buffer (always BigEndian).
**
**
** Device access routines mrfioc2_regDev_flush, mrmEvrDataRxCB, implement
** correct conversions and data reconstructions. E.g. data received over PCI/
** PCIe will be in littleEndian, but the littleEndian conversion will be 4 bytes
** wide (this means that if data in HW is 76543210 the result will be 45670123)
**
**
** -- MISSING ---------------------------------------------------------------
**
** Explicit setting of DataBuffer MODE (whether DataBuffer is shared with DBUS)
**
**
** Author: Tom Slejko
** -------------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
//#include <math.h>

/*
 * mrfIoc2 headers
 */

#include <mrmDataBufferUser.h>

/*
 *  EPICS headers
 */
#include <iocsh.h>
#include <drvSup.h>
#include <epicsEndian.h>
#include <regDev.h>
#include <errlog.h>
#include <epicsThread.h>
#include <epicsExport.h>

/*                                        */
/*        DEFINES                         */
/*                                        */

int mrfioc2_regDevDebug = 0;
epicsExportAddress(int, mrfioc2_regDevDebug);
#define dbgPrintf(level,M, ...) if(mrfioc2_regDevDebug >= level) fprintf(stderr, "mrfioc2_regDevDebug: (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#if defined __GNUC__
#define _unused(x) x __attribute__((unused))
#else
#define _unused(x)
#endif

#define dataBuffer_userOffset 16

/*
 * mrfDev reg driver private
 */
struct regDevice{
    char*                   name;            //regDevName of device
    mrmDataBufferUser*      dataBufferUser;
    IOSCANPVT               ioscanpvt;
    epicsUInt8*             rxBuffer;       // pointer to rx buffer
    epicsUInt8*             txBuffer;       // pointer to tx buffer
    epicsUInt32             proto;          // protocol ID
    epicsUInt32             receivedProtocolID; // last received protocol ID
    size_t                  maxLength;      // underlying data buffer capacity
    size_t                  invalidOffset;    // write to [0, invalidOffset] to send out the data buffer
    //epicsInt32              dataLossCounter;    // used to detect possible data loss: multiple data buffer updates can occure before record (initiated by ioscan) was processed
    //size_t                  numOfRecords;   // number of registered records

};


/****************************************/
/*        DEVICE ACCESS FUNCIONS             */
/****************************************/

/*
 * Function will flush software scratch buffer.
 * Buffer will be copied and (if needed) its contents
 * will be converted into appropriate endiannes
 */
static void mrfioc2_regDev_flush(regDevice* device)
{
    /* Copy protocol ID (big endian) */
    if (device->proto != 0) {
        device->dataBufferUser->put(0, sizeof(device->proto), &device->proto);
    }

    /* Send out the data */
    device->dataBufferUser->send(false);
}


/*
 * This callback is attached to EVR rx buffer logic.
 * Byte order is big endian as on network.
 *
 * We check if there might be data loss (not all records read the data before we got another callback).
 * Then we issue a scanio request for records to read their data.
 */
void mrmEvrDataRxCB(size_t updated_offset, size_t length, void* pvt) {
    regDevice* device = (regDevice *)pvt;

    dbgPrintf(2,"Received new DATA at %d+%d\n", updated_offset, length);

    scanIoRequest(device->ioscanpvt);
}


/****************************************/
/*            REG DEV FUNCIONS              */
/****************************************/

void mrfioc2_regDev_report(regDevice* device, int _unused(level)) {
    printf("mrfioc2 regDev: %s\n\t" \
           "Max length: %zu\n\t" \
           "Invalid offsets: [1, %zu]\n\t" \
           "Protocol ID: %u\n\t" \
           "Supports transmission: %s\n\t" \
           "Supports reception: %s\n",
           device->name, device->maxLength, device->invalidOffset, device->proto,
           device->dataBufferUser->supportsTx() ? "yes":"no", device->dataBufferUser->supportsRx() ? "yes":"no");
}

/*
 * Read will make sure that the data is correctly copied into the records.
 * Since data in MRF DBUFF is big endian (since all EVGs are running on BE
 * systems) the data may need to be converted to LE.
 *
 * If protocol ID is used, we compare the received protocol ID
 * with desired protocol. If there is a match then the data is copied into the record.
 *
 * At the end dataLoss counter is decreased. It is used to signal that the record has read the data.
 */
int mrfioc2_regDev_read(
        regDevice* device,
        size_t offset,
        unsigned int datalength,
        size_t nelem,
        void* pdata,
        int _unused(priority),
        regDevTransferComplete _unused(callback),
        char* user)
{
    dbgPrintf(2,"%s: from %s:0x%x len: 0x%x\n",  user, device->name, (int)offset, (int)(datalength*nelem));

    epicsUInt32 receivedProtocolID = 0;

    device->rxBuffer = device->dataBufferUser->requestRxBuffer();
    if (device->proto != 0) {
        memcpy(&receivedProtocolID, device->rxBuffer, sizeof(device->proto));  // Extract protocol ID
        if (device->proto != receivedProtocolID) return 0;              // Do nothing if we are not interested in this protocol ID
    }
    regDevCopy(datalength, nelem, &device->rxBuffer[offset], pdata, NULL, REGDEV_LE_SWAP);  // Copy received data to the record
    device->dataBufferUser->releaseRxBuffer();

    if (device->proto != 0) dbgPrintf(2,"%s: protocol ID = %d\n", device->name, receivedProtocolID);

    return 0;
}

/*
 * Writes the data from the record to the data buffer.
 * If the offset to write to is 0, function will instead flush the data buffer.
 */
int mrfioc2_regDev_write(
        regDevice* device,
        size_t offset,
        unsigned int datalength,
        size_t nelem,
        void* pdata,
        void* pmask,
        int _unused(priority),
        regDevTransferComplete _unused(callback),
        char* user)
{
    // Offset 0 is always used to send-out the data buffer (flush it)
    if (offset <= 0) {
        mrfioc2_regDev_flush(device);
        return 0;
    }

    if (offset <= device->invalidOffset || offset + datalength > device->maxLength) { // out of bounds check
        errlogPrintf("Offset %zu not in [%zu, %zu]\n", offset, device->invalidOffset+1, device->maxLength - datalength);
        return 1;
    }

    // Put the data from the record to the data buffer
    device->txBuffer = device->dataBufferUser->requestTxBuffer();
    regDevCopy(datalength, nelem, pdata, &device->txBuffer[offset], pmask, REGDEV_LE_SWAP);
    device->dataBufferUser->releaseTxBuffer(offset, datalength);

    dbgPrintf(2,"Written new DATA at %d+%d\n", offset, datalength);
    return 0;
}


IOSCANPVT mrfioc2_regDev_getInIoscan(regDevice* device, size_t _unused(offset))
{
    return device->ioscanpvt;
}


//RegDev device definition
static const regDevSupport mrfioc2_regDevSupport = {
    mrfioc2_regDev_report,
    mrfioc2_regDev_getInIoscan,
    NULL,
    mrfioc2_regDev_read,
    mrfioc2_regDev_write
};


/*
 * Initialization, this is the entry point.
 * Function is called from iocsh. Function will try
 * to find desired device (mrfName) and attach mrfioc2_regDev
 * support to it.
 *
 * Args:Can not find mrf device: %s
 *         regDevName - desired name of the regDev device
 *         mrfName - name of mrfioc2 device (evg, evr, ...)
 *         protocol - protocol ID to use, or 0 to disable it. When not provided defaults to 0.
 *         userOffset- offset from the start of the data buffer that we are using. When not provided defaults to dataBuffer_userOffset.
 *         maxLength - maximum data buffer length we are interested in. Must be max(offset+length) of all records. When not provided it defaults to maximum available length.
 */

void mrfioc2_regDevConfigure(const char* regDevName, const char* mrfName, int argc, char** argv)
{
    if (!regDevName || !mrfName) {
        errlogPrintf("usage: mrfioc2_regDevConfigure \"regDevName\", \"mrfName\", [protocol] [maxLength] [userOffset]\n");
        return;
    }

    //Check if device already exists:
    if (regDevFind(regDevName)) {
        errlogPrintf("mrfioc2_regDevConfigure: FATAL ERROR! device %s already exists!\n", regDevName);
        return;
    }

    /*
     * Init regDevice structure and fill it with regDevName
     */
    regDevice* device;
    device = (regDevice*) calloc(1, sizeof(regDevice) + strlen(regDevName) + 1);
    if (!device) {
        errlogPrintf("mrfioc2_regDevConfigure %s: FATAL ERROR! Out of memory!\n", regDevName);
        return;
    }
    device->name = (char*)(device + 1);
    strcpy(device->name, regDevName);


    /*
     * Create new data buffer user and fill device (regDevice)
     */
    device->dataBufferUser = new mrmDataBufferUser();    // TODO where to put destructor??

    // Set up protocol number
    device->proto = 0;
    if (argc > 1) {
        device->proto = (epicsUInt32) strtoimax(argv[1], NULL, 10);
    }
    if (device->proto != 0) {
        dbgPrintf(1,"mrfioc2_regDevConfigure %s: Registering to protocol %d\n", regDevName, device->proto);
        device->invalidOffset = sizeof(device->proto) -1;
    } else {
        dbgPrintf(1,"mrfioc2_regDevConfigure %s: Not using protocol number (set to 0)\n", regDevName);
        device->invalidOffset = 0;
    }

    // set up user offset
    size_t userOffset = dataBuffer_userOffset;
    if (argc > 2) {
        userOffset = strtoimax(argv[2], NULL, 10);
        if (userOffset <= 0 || userOffset >= device->dataBufferUser->getMaxLength()) {
            userOffset = dataBuffer_userOffset;
            errlogPrintf("mrfioc2_regDevConfigure %s: User offset must be in the following range: [1, %zu]\n", regDevName, device->dataBufferUser->getMaxLength()-1);
        }
    }
    dbgPrintf(1,"%s: User offset set to %d\n", regDevName, userOffset);

    // Initialize the data buffer
    if (!device->dataBufferUser->init(mrfName, userOffset, true, epicsThreadPriorityHigh)) {
        delete device->dataBufferUser;
        return;
    }

    // Set max length we are interested in
    device->maxLength = device->dataBufferUser->getMaxLength();
    size_t maxLength;
    if (argc > 3) {
        maxLength = strtoimax(argv[3], NULL, 10);
        if (maxLength <= 0 || maxLength > device->maxLength) {
            device->maxLength = device->dataBufferUser->getMaxLength();
            errlogPrintf("mrfioc2_regDevConfigure %s: Maximum data length not a number or out of range: [1, %zu]\n", regDevName, device->maxLength);
        }
        else device->maxLength = maxLength;
    }
    dbgPrintf(1,"%s: Maximum data length set to %zu.\n", regDevName, device->maxLength);


    if (device->dataBufferUser->supportsTx())
    {
        dbgPrintf(1,"%s: %s supports TX buffer.\n", regDevName, mrfName);
    }

    if (device->dataBufferUser->supportsRx())
    {
        dbgPrintf(1,"%s: %s supports RX buffer. Registering interest and installing callbacks\n", regDevName, mrfName);

        if (!device->dataBufferUser->registerInterest(0, device->maxLength, mrmEvrDataRxCB, device)) {
            errlogPrintf("mrfioc2_regDevConfigure %s: FATAL ERROR! Could not register for data buffer reception!", regDevName);
            delete device->dataBufferUser;
            return;
        }

        scanIoInit(&device->ioscanpvt);
    }

    regDevRegisterDevice(regDevName, &mrfioc2_regDevSupport, device, device->maxLength);
}

/****************************************/
/*        EPICS IOCSH REGISTRATION        */
/****************************************/

/*         mrfioc2_regDevConfigure           */
static const iocshArg mrfioc2_regDevConfigureDefArg0 = { "regDevName", iocshArgString};
static const iocshArg mrfioc2_regDevConfigureDefArg1 = { "mrfioc2 device name", iocshArgString};
static const iocshArg mrfioc2_regDevConfigureDefArg2 = { "misc", iocshArgArgv}; // protocol, user offset, max interested length
static const iocshArg *const mrfioc2_regDevConfigureDefArgs[3] = {&mrfioc2_regDevConfigureDefArg0, &mrfioc2_regDevConfigureDefArg1, &mrfioc2_regDevConfigureDefArg2};

static const iocshFuncDef mrfioc2_regDevConfigureDef = {"mrfioc2_regDevConfigure", 3, mrfioc2_regDevConfigureDefArgs};

static void mrfioc2_regDevConfigureFunc(const iocshArgBuf* args) {
    mrfioc2_regDevConfigure(args[0].sval, args[1].sval, args[2].aval.ac, args[2].aval.av);
}


/*         mrfioc2_regDevFlush           */
static const iocshArg mrfioc2_regDevFlushDefArg0 = { "regDevName", iocshArgString};
static const iocshArg *const mrfioc2_regDevFlushDefArgs[1] = {&mrfioc2_regDevFlushDefArg0};

static const iocshFuncDef mrfioc2_regDevFlushDef = {"mrfioc2_regDevFlush", 1, mrfioc2_regDevFlushDefArgs};

static void mrfioc2_regDevFlushFunc(const iocshArgBuf* args) {
    regDevice* device = regDevFind(args[0].sval);
    if(!device){
        errlogPrintf("Can not find device: %s\n", args[0].sval);
        return;
    }

    mrfioc2_regDev_flush(device);
}

/*        registrar            */

static int mrfioc2_regDevRegistrar(void) {
    iocshRegister(&mrfioc2_regDevConfigureDef, mrfioc2_regDevConfigureFunc);
    iocshRegister(&mrfioc2_regDevFlushDef, mrfioc2_regDevFlushFunc);

    return 1;
}
extern "C" {
 epicsExportRegistrar(mrfioc2_regDevRegistrar);
}
static int done = mrfioc2_regDevRegistrar();
