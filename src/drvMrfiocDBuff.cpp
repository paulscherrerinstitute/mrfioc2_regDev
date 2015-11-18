/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** drvmrfiocRegDev.cpp
**
** RegDev device support for Distributed Buffer on MRF EVR and EVG cards using
** mrfioc2 driver.
**
** -- DOCS ----------------------------------------------------------------
** Driver is registered via iocsh command:
**         mrfiocRegDevConfigure <regDevName> <mrfName> <protocol ID>
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
**         example:    mrfiocRegDevConfigure EVGDBUFF EVG1 42
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
** Device access routines mrfiocRegDev_flush, mrmEvrDataRxCB, implement
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
#include <epicsExport.h>

/*                                        */
/*        DEFINES                         */
/*                                        */

int mrfioc2_regDevDebug = 0;
epicsExportAddress(int, mrfioc2_regDevDebug);
#define dbgPrintf(level,M, ...) if(mrfioc2_regDevDebug >= level) fprintf(stderr, "mrfioc_RegDev_DEBUG: (%s:%d) " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#if defined __GNUC__
#define _unused(x) x __attribute__((unused))
#else
#define _unused(x)
#endif

#define dataBufferOffset 16

/*
 * mrfDev reg driver private
 */
struct regDevice{
    char*                   name;            //regDevName of device
    mrmDataBufferUser*      dataBuffer;
    IOSCANPVT               ioscanpvt;
    epicsUInt8*             rxBuffer;       // pointer to rx buffer
    epicsUInt8*             txBuffer;       // pointer to tx buffer
    epicsUInt32             proto;          // protocol ID
    size_t                  maxLength;      // underlying data buffer capacity
    size_t                  flushOffset;    // write to [0, flushOffset] to send out the data buffer
};


/****************************************/
/*        DEVICE ACCESS FUNCIONS             */
/****************************************/

/*
 * Function will flush software scratch buffer.
 * Buffer will be copied and (if needed) its contents
 * will be converted into appropriate endiannes
 */
static void mrfiocRegDev_flush(regDevice* device)
{
    /* Copy protocol ID (big endian) */
    if (device->proto != 0) {
        device->dataBuffer->put(0, sizeof(device->proto), &device->proto);
    }

    /* Send out the data */
    device->dataBuffer->send(false);
}


/*
 *    This callback is attached to EVR rx buffer logic.
 *    Byte order is big endian as on network.
 *    Callback then compares received protocol ID
 *    with desired protocol. If there is a match then buffer
 *    is copied into device private rxBuffer and scan IO
 *    is requested.
 */
void mrmEvrDataRxCB(size_t updated_offset, size_t length, void* pvt) {
    regDevice* device = (regDevice *)pvt;

    if (device->proto != 0)
    {
        // Extract protocol ID
        epicsUInt32 receivedProtocolID;

        device->dataBuffer->get(0, sizeof(device->proto), &receivedProtocolID);
        dbgPrintf(1,"%s: protocol ID = %d\n", device->name, receivedProtocolID);

        if (device->proto != receivedProtocolID) return;
    }

    dbgPrintf(1,"Received new DATA at %d+%d\n", updated_offset, length);
    device->dataBuffer->get(updated_offset, length, &device->rxBuffer[updated_offset]);

    scanIoRequest(device->ioscanpvt);
}


/****************************************/
/*            REG DEV FUNCIONS              */
/****************************************/

void mrfiocRegDev_report(regDevice* device, int _unused(level))
{
    printf("%s dataBuffer max length %u\n", device->name, device->maxLength);
}

/*
 * Read will make sure that the data is correctly copied into the records.
 * Since data in MRF DBUFF is big endian (since all EVGs are running on BE
 * systems) the data may need to be converted to LE. Data in rxBuffer is
 * always BE.
 */
int mrfiocRegDev_read(
        regDevice* device,
        size_t offset,
        unsigned int datalength,
        size_t nelem,
        void* pdata,
        int _unused(priority),
        regDevTransferComplete _unused(callback),
        char* user)
{
    dbgPrintf(1,"%s: from %s:0x%x len: 0x%x\n",
            user, device->name, (int)offset, (int)(datalength*nelem));


    /* Data in buffer is in big endian byte order */
    regDevCopy(datalength, nelem, &device->rxBuffer[offset], pdata, NULL, REGDEV_LE_SWAP);

    return 0;
}

int mrfiocRegDev_write(
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
    if (offset > device->maxLength) { // out of bounds check
        errlogPrintf("Offset %zu not in [0, %zu]\n", offset, device->maxLength);
        return 1;
    }

    if (offset <= device->flushOffset) {
        mrfiocRegDev_flush(device);
        return 0;
    }

    /* Copy into the scratch buffer */
    /* Data in buffer is in big endian byte order */
    regDevCopy(datalength, nelem, pdata, &device->txBuffer[offset], pmask, REGDEV_LE_SWAP);
    device->dataBuffer->put(offset, datalength, &device->txBuffer[offset]);

    return 0;
}


IOSCANPVT mrfiocRegDev_getInIoscan(regDevice* device, size_t _unused(offset))
{
    return device->ioscanpvt;
}


//RegDev device definition
static const regDevSupport mrfiocRegDevSupport = {
    mrfiocRegDev_report,
    mrfiocRegDev_getInIoscan,
    NULL,
    mrfiocRegDev_read,
    mrfiocRegDev_write
};


/*
 * Initialization, this is the entry point.
 * Function is called from iocsh. Function will try
 * to find desired device (mrfName) and attach mrfiocRegDev
 * support to it.
 *
 * Args:Can not find mrf device: %s
 *         regDevName - desired name of the regDev device
 *         mrfName - name of mrfioc2 device (evg, evr, ...)
 *
 */

void mrfiocRegDevConfigure(const char* regDevName, const char* mrfName, int protocol)
{
    if (!regDevName || !mrfName) {
        errlogPrintf("usage: mrfiocRegDevConfigure \"regDevName\", \"mrfName\", [protocol]\n");
        return;
    }

    //Check if device already exists:
    if (regDevFind(regDevName)) {
        errlogPrintf("mrfiocRegDevConfigure: FATAL ERROR! device %s already exists!\n", regDevName);
        return;
    }

    regDevice* device;

    device = (regDevice*) calloc(1, sizeof(regDevice) + strlen(regDevName) + 1);
    if (!device) {
        errlogPrintf("mrfiocRegDevConfigure %s: FATAL ERROR! Out of memory!\n", regDevName);
        return;
    }
    device->name = (char*)(device + 1);
    strcpy(device->name, regDevName);

    /*
     * Create new data buffer user.
     */
    device->dataBuffer = new mrmDataBufferUser();    // TODO where to put destructor??

    if (device->dataBuffer->init(mrfName, dataBufferOffset, true) != 0) {
        errlogPrintf("mrfiocRegDevConfigure %s: FAILED to initialize data buffer on device %s\n", regDevName, mrfName);
        delete device->dataBuffer;
        return;
    }

    device->maxLength = device->dataBuffer->getMaxLength();


    /*
     * We use offset <= protoID length (that is illegal for normal use)
     * to flush the output buffer. This eliminates the need for extra record.
     * If protocols are disabled, offset 0 is used.
     */
    device->proto = (epicsUInt32) protocol; //protocol ID

    if (device->proto != 0) {
        epicsPrintf("mrfiocRegDevConfigure %s: registering to protocol %d\n", regDevName, device->proto);
        device->flushOffset = sizeof(device->proto) -1;
    }
    else {
        epicsPrintf("mrfiocRegDevConfigure %s: not using protocol\n", regDevName);
        device->flushOffset = 0;
    }

    if (device->dataBuffer->supportsTx())
    {
        dbgPrintf(1,"%s: %s supports TX buffer. Allocating.\n",
            regDevName, mrfName);
        // Allocate the buffer memory
        device->txBuffer = (epicsUInt8*) calloc(1, device->maxLength);
        if (!device->txBuffer) {
            errlogPrintf("%s: FATAL ERROR! Could not allocate TX buffer!", regDevName);
            return;
        }
    }

    if (device->dataBuffer->supportsRx())
    {
        dbgPrintf(1,"%s: %s supports RX buffer. Allocating and installing callback\n",
            regDevName, mrfName);
        device->rxBuffer = (epicsUInt8*) calloc(1, device->maxLength);
        if (!device->rxBuffer) {
            errlogPrintf("%s: FATAL ERROR! Could not allocate RX buffer!", regDevName);
            return;
        }
        device->dataBuffer->registerInterest(0, device->maxLength, mrmEvrDataRxCB, device);
        scanIoInit(&device->ioscanpvt);
    }

    regDevRegisterDevice(regDevName, &mrfiocRegDevSupport, device, device->maxLength);
}

/****************************************/
/*        EPICS IOCSH REGISTRATION        */
/****************************************/

/*         mrfiocRegDevConfigure           */
static const iocshArg mrfiocRegDevConfigureDefArg0 = { "regDevName", iocshArgString};
static const iocshArg mrfiocRegDevConfigureDefArg1 = { "mrfioc2 device name", iocshArgString};
static const iocshArg mrfiocRegDevConfigureDefArg2 = { "protocol", iocshArgInt};
static const iocshArg *const mrfiocRegDevConfigureDefArgs[3] = {&mrfiocRegDevConfigureDefArg0, &mrfiocRegDevConfigureDefArg1, &mrfiocRegDevConfigureDefArg2};

static const iocshFuncDef mrfiocRegDevConfigureDef = {"mrfiocRegDevConfigure", 3, mrfiocRegDevConfigureDefArgs};

static void mrfiocRegDevConfigureFunc(const iocshArgBuf* args) {
    mrfiocRegDevConfigure(args[0].sval, args[1].sval, args[2].ival);
}


/*         mrfiocRegDevFlush           */
static const iocshArg mrfiocRegDevFlushDefArg0 = { "regDevName", iocshArgString};
static const iocshArg *const mrfiocRegDevFlushDefArgs[1] = {&mrfiocRegDevFlushDefArg0};

static const iocshFuncDef mrfiocRegDevFlushDef = {"mrfiocRegDevFlush", 1, mrfiocRegDevFlushDefArgs};

static void mrfiocRegDevFlushFunc(const iocshArgBuf* args) {
    regDevice* device = regDevFind(args[0].sval);
    if(!device){
        errlogPrintf("Can not find device: %s\n", args[0].sval);
        return;
    }

    mrfiocRegDev_flush(device);
}

/*        registrar            */

static int mrfiocRegDevRegistrar(void) {
    iocshRegister(&mrfiocRegDevConfigureDef, mrfiocRegDevConfigureFunc);
    iocshRegister(&mrfiocRegDevFlushDef, mrfiocRegDevFlushFunc);

    return 1;
}
extern "C" {
 epicsExportRegistrar(mrfiocRegDevRegistrar);
}
static int done = mrfiocRegDevRegistrar();
