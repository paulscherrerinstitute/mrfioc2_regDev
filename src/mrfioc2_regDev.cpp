/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** mrfioc2_regDev.cpp
**
** RegDev device support for Distributed Buffer on MRF EVR and EVG cards using
** mrfioc2 driver.
**
** -- DOCS ----------------------------------------------------------------
** Driver is registered via iocsh command:
**         mrfioc2_regDevConfigure <name> <device> <protocol> <userOffset>
**
**             -name: name of device as seen from regDev. E.g. this
**                         name must be the same as parameter 1 in record OUT/IN
**
**             -device: name of timing card (EVG0, EVR0, EVR1, ...)
**
**             -type: data buffer type. 230 or 300
**
**             -protocol: protocol (32 bit int). Useful when using 230 series
**                         hardware. 300 series uses segmented data buffer, which
**                         makes the protocol redundant.
**                         If protocol is set to 0, than receiver
**                         will accept all buffers. This is useful for
**                         debugging. If protocol != 0 then only received buffers
**                         with same protocol are accepted. If you need to work
**                         with multiple protocols you can register multiple instances
**                         of regDev using the same mrfName but different regDevNames and
**                         protocols.
**
**             -userOffset: offset from the start of the data buffer that we are using. When not provided defaults to 0.
**
**
**         example:    mrfioc2_regDevConfigure EVG0DBUF EVG0 300 0 16
**
**
** EPICS use:
**
**         - records behave the same as for any other regDev device with one exception:
**
**         - when using protocol, offset 0x00-0x04 can not be written to (because size of protocol is 4 bytes)
**         - writing to offset 0x00 will flush the buffer
**         - input records are automatically processed (if scan == IO) when a valid buffer is received
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
** VME EVM-300 (tx)
** VME EVR-300 (tx and rx)
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
**
**
** Author: Cosylab
** -------------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/*
 * mrfIoc2 headers
 */

#ifdef _WIN32
#include <dataBuffer/mrmDataBufferUser.h>
#include <dataBuffer/mrmDataBufferType.h>
#else
#include <mrmDataBufferUser.h>
#include <mrmDataBufferType.h>
#endif

/*
 *  EPICS headers
 */
#include <iocsh.h>
#include <drvSup.h>
#include <regDev.h>
#include <errlog.h>
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

// printf formatting for size_t differs on windows
#ifdef _WIN32
    #define FORMAT_SIZET_U "Iu"
    #define FORMAT_SIZET_X "Ix"
#else
    #define FORMAT_SIZET_U "zu"
    #define FORMAT_SIZET_X "zx"
#endif

#define dataBuffer_userOffset 0

/*
 * mrfDev reg driver private
 */
struct regDevice{
    char*                   name;           // regDevName of device
    mrmDataBufferUser*      dataBufferUser; // exposes access to underlying data buffer
    IOSCANPVT               ioscanpvt;
    epicsUInt8*             rxBuffer;       // pointer to rx buffer
    epicsUInt8*             txBuffer;       // pointer to tx buffer
    epicsUInt32             protocol;       // protocol ID
    size_t                  invalidOffset;  // writing to [0, invalidOffset] fails. The offset is dependant on protocol (its size, and if it is used or not)
    size_t                  interestID;     // ID used to update registered interest on the data buffer
};


/****************************************/
/*        DEVICE ACCESS FUNCIONS             */
/****************************************/

/*
 * Function will flush software scratch buffer.
 * If protocol is used it is send out too.
 */
static void mrfioc2_regDev_flush(regDevice* device)
{
    /* Copy protocol (big endian) */
    if (device->protocol != 0) {
        device->txBuffer = device->dataBufferUser->requestTxBuffer();
        regDevCopy(sizeof(device->protocol), 1, &device->protocol, &device->txBuffer[0], NULL, REGDEV_LE_SWAP);  // Extract protocol
        device->dataBufferUser->releaseTxBuffer(0, sizeof(device->protocol));
    }

    /* Send out the data */
    device->dataBufferUser->send(false);
    dbgPrintf(2,"Flushed\n");
}


/*
 * This callback is attached to EVR Rx buffer logic.
 * It informs the records that there is new data to be read.
 */
void mrmEvrDataRxCB(size_t updated_offset, size_t length, void* pvt) {
    regDevice* device = (regDevice *)pvt;

    dbgPrintf(2,"Received new DATA at %"FORMAT_SIZET_U"+%"FORMAT_SIZET_U"\n", updated_offset, length);

    scanIoRequest(device->ioscanpvt);
}


/****************************************/
/*            REG DEV FUNCIONS              */
/****************************************/

void mrfioc2_regDev_report(regDevice* device, int _unused(level)) {
    printf("mrfioc2 regDev: %s\n\t" \
           "protocol: %u\n\t" \
           "Supports transmission: %s\n\t" \
           "Supports reception: %s\n\t",
           device->name, device->protocol,
           device->dataBufferUser->supportsTx() ? "yes":"no", device->dataBufferUser->supportsRx() ? "yes":"no");

    if(device->invalidOffset > 0) {
        printf("Invalid offsets: [1, %"FORMAT_SIZET_U"]\n", device->invalidOffset);
    }else {
        printf("Invalid offsets: none\n");
    }

}

/*
 * Read will make sure that the data is correctly copied into the records.
 * Since data in MRF DBUF is big endian (since all EVGs are running on BE
 * systems) the data may need to be converted to LE.
 *
 * If protocol is used, we compare the received protocol
 * with desired protocol. If there is a match then the data is copied into the record.
 *
 */
int mrfioc2_regDev_read(
        regDevice* device,
        size_t offset,
        unsigned int datalength,
        size_t nelem,
        void* pdata,
        int _unused(priority),
        regDevTransferComplete _unused(callback),
        const char* user)
{
    dbgPrintf(3,"%s: from %s:0x%"FORMAT_SIZET_X" len: 0x%"FORMAT_SIZET_X"\n",  user, device->name, offset, (datalength*nelem));

    epicsUInt32 receivedProtocol = 0;

    device->rxBuffer = device->dataBufferUser->requestRxBuffer();
    if (device->protocol != 0) {
        regDevCopy(sizeof(device->protocol), 1, device->rxBuffer, &receivedProtocol, NULL, REGDEV_LE_SWAP);  // Extract protocol

        if (device->protocol != receivedProtocol) { // Do nothing if we are not interested in this protocol
            device->dataBufferUser->releaseRxBuffer();
            return 0;
        }
    }
    regDevCopy(datalength, nelem, &device->rxBuffer[offset], pdata, NULL, REGDEV_LE_SWAP);  // Copy received data to the record
    device->dataBufferUser->releaseRxBuffer();

    if (device->protocol != 0) dbgPrintf(3,"%s: protocol = %u\n", device->name, receivedProtocol);

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
        const char* user)
{
    // Offset 0 is always used to send-out the data buffer (flush it)
    if (offset <= 0) {
        mrfioc2_regDev_flush(device);
        return 0;
    }

    if (offset <= device->invalidOffset) { // out of bounds check (max is already checked by regDev)
        errlogPrintf("Invalid offset: [0, %"FORMAT_SIZET_U"]\n", device->invalidOffset);
        return 1;
    }

    // Put the data from the record to the data buffer
    device->txBuffer = device->dataBufferUser->requestTxBuffer();
    regDevCopy(datalength, nelem, pdata, &device->txBuffer[offset], pmask, REGDEV_LE_SWAP);
    device->dataBufferUser->releaseTxBuffer(offset, datalength);

    dbgPrintf(2,"Written new DATA at %"FORMAT_SIZET_U"+%u\n", offset, datalength);
    return 0;
}


IOSCANPVT mrfioc2_regDev_getInIoscan(
        regDevice *device,
        size_t offset,
        unsigned int dlen,
        size_t nelem,
        int _unused(intvec),
        const char* _unused(user))
{
    device->interestID = device->dataBufferUser->registerInterest(offset, dlen*nelem, mrmEvrDataRxCB, device, device->interestID);
    dbgPrintf(2,"Registering interest id %"FORMAT_SIZET_U" on offset %"FORMAT_SIZET_U" with length %"FORMAT_SIZET_U"\n", device->interestID, offset, dlen*nelem);
    if (device->interestID == 0) {
        errlogPrintf("mrfioc2_regDevConfigure %s: ERROR! Could not register for data buffer reception on offset 0x%"FORMAT_SIZET_X" with length %"FORMAT_SIZET_U"\n", device->name, offset, nelem*dlen);
    }

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
 *         name - desired name of the regDev device
 *         device - name of mrfioc2 device - timing card (evg, evr, ...)
 *         type - data buffer type: 230 or 300
 *         protocol - protocol to use, or 0 to disable it. When not provided defaults to 0.
 *         userOffset- offset from the start of the data buffer that we are using. When not provided defaults to dataBuffer_userOffset.
 */

void mrfioc2_regDevConfigure(const char* regDevName, const char* mrfName, const char* type, int argc, char** argv)
{
    if (!regDevName || !mrfName) {
        errlogPrintf("usage: mrfioc2_regDevConfigure \"name\", \"device\", [protocol] [userOffset]\n");
        return;
    }

    //Check if device already exists:
    if (regDevFind(regDevName)) {
        errlogPrintf("mrfioc2_regDevConfigure: FATAL ERROR! device %s already exists!\n", regDevName);
        return;
    }

    mrmDataBufferType::type_t dataBufferType;
    bool typeFound = false;
    for(size_t t = mrmDataBufferType::type_first; t <= mrmDataBufferType::type_last; t++) {
        if(strcmp(type, mrmDataBufferType::type_string[t]) == 0) {
            dataBufferType = (mrmDataBufferType::type_t)t;
            typeFound = true;
            break;
        }
    }
    if(!typeFound) {
        fprintf(stderr, "Wrong data buffer type selected: %s\n", type);
        return;
    }

    /*
     * Init regDevice structure and fill it with name
     */
    regDevice* device;
    device = (regDevice*) calloc(1, sizeof(regDevice) + strlen(regDevName) + 1);
    if (!device) {
        errlogPrintf("mrfioc2_regDevConfigure %s: FATAL ERROR! Out of memory!\n", regDevName);
        return;
    }
    device->name = (char*)(device + 1);
    strcpy(device->name, regDevName);
    device->interestID = 0;


    /*
     * Create new data buffer user and fill device (regDevice)
     */
    device->dataBufferUser = new mrmDataBufferUser();    // we only destroy this object on error, never in normal operation.

    // Set up protocol number
    device->protocol = 0;
    if (argc > 1) {
        device->protocol = (epicsUInt32) strtoimax(argv[1], NULL, 10);
    }
    if (device->protocol != 0) {
        device->invalidOffset = sizeof(device->protocol) -1;
        dbgPrintf(1,"mrfioc2_regDevConfigure %s: Registering to protocol %u.\n\tInvalid write offsets: [1, %"FORMAT_SIZET_U"]\n", regDevName, device->protocol, device->invalidOffset);
    } else {
        dbgPrintf(1,"mrfioc2_regDevConfigure %s: Not using protocol number (set to 0)\n", regDevName);
        device->invalidOffset = 0;
    }

    // set up user offset
    size_t userOffset = dataBuffer_userOffset;
    if (argc > 2) {
        userOffset = strtoimax(argv[2], NULL, 10);
        if (userOffset < 0 || userOffset >= device->dataBufferUser->getMaxLength()) {
            errlogPrintf("mrfioc2_regDevConfigure %s: User offset must be in the following range: [0, %"FORMAT_SIZET_U"]\n", regDevName, device->dataBufferUser->getMaxLength()-1);
            delete device->dataBufferUser;
            return;
        }
    }
    dbgPrintf(1,"%s: User offset set to %"FORMAT_SIZET_U"\n", regDevName, userOffset);

    // Initialize the data buffer
    if (!device->dataBufferUser->init(mrfName, dataBufferType, userOffset, true, epicsThreadPriorityHigh)) {
        delete device->dataBufferUser;
        return;
    }

    if (device->dataBufferUser->supportsTx())
    {
        dbgPrintf(1,"%s: %s supports TX buffer.\n", regDevName, mrfName);
    }

    if (device->dataBufferUser->supportsRx())
    {
        dbgPrintf(1,"%s: %s supports RX buffer. Registering interest and installing callbacks\n", regDevName, mrfName);
        scanIoInit(&device->ioscanpvt);
    }

    regDevRegisterDevice(regDevName, &mrfioc2_regDevSupport, device, device->dataBufferUser->getMaxLength());
}

/****************************************/
/*        EPICS IOCSH REGISTRATION        */
/****************************************/

/*         mrfioc2_regDevConfigure           */
static const iocshArg mrfioc2_regDevConfigureDefArg0 = { "name", iocshArgString};
static const iocshArg mrfioc2_regDevConfigureDefArg1 = { "device", iocshArgString};
static const iocshArg mrfioc2_regDevConfigureDefArg2 = { "type [230, 300]", iocshArgString};  // which data buffer type? is it 230 series, 300 series?
static const iocshArg mrfioc2_regDevConfigureDefArg3 = { "protocol userOffset", iocshArgArgv}; // protocol, user offset
static const iocshArg *const mrfioc2_regDevConfigureDefArgs[4] = {&mrfioc2_regDevConfigureDefArg0, &mrfioc2_regDevConfigureDefArg1, &mrfioc2_regDevConfigureDefArg2, &mrfioc2_regDevConfigureDefArg3};

static const iocshFuncDef mrfioc2_regDevConfigureDef = {"mrfioc2_regDevConfigure", 4, mrfioc2_regDevConfigureDefArgs};

static void mrfioc2_regDevConfigureFunc(const iocshArgBuf* args) {
    mrfioc2_regDevConfigure(args[0].sval, args[1].sval, args[2].sval, args[3].aval.ac, args[3].aval.av);
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
