# mrfioc2_regDev
[mrfioc2_regDev](..) enables the user to easily access (read or write) the data buffer that is exposed in the [mrfioc2](https://github.psi.ch/epics_driver_modules/mrfioc2) EPICS device support for the Micro Research Finland ([MRF](http://www.mrf.fi/)) timing system (in short mrfioc2 driver).



## Prerequisites

- [mrfioc2](https://git.psi.ch/epics_driver_modules/mrfioc2)
- [regDev](https://git.psi.ch/epics_driver_modules/regDev). Documentation is [here](https://intranet.psi.ch/Controls/RegDev).


## Quick start (PSI)
Access mrfioc2_regDev files and inspect example startup scripts:

* `example/startup_pulseId_RX.script` is an example startup script for use with pulse ID reception. 
* `example/startup_pulseId_TX.script` is an example startup script for use with pulse ID transmission.

Here is an example of mrfioc2_regDev startup script snippet used for event receiver named `EVR0` with data buffer type 300:
    
    ########################################
    #------! PULSE ID receive setup ------!#
    ########################################
    require mrfioc2_regDev
    
    # The following macros are available to set up the mrfioc2 regDev:
    # SYS       is used as a prefix for all records. It is recommended to be the same as the EVR/EVG prefix.
    # DEVICE    is the event receiver or event generator / timing card name. (default: EVR0)
    # NAME      is the regDev name that will be configured for this device. Defaults to $(DEVICE=EVR0)DBUF == EVR0DBUF
    # TYPE      is the data buffer type to search for. Values: 230 = 230 series type (variable length data buffer), 300 = 300 series type (segmented data buffer). Default: 300
    # ID        is used when multiple instances of the template are loaded. The ID is used in record names. Defaults to empty string. For example, when using 230 and 300 series data buffer at the same time
    # PROTOCOL    Useful when using 230 series hardware. 300 series uses segmented data buffer, which makes the protocol ID redundant. If protocol ID is set to 0, than receiver will accept all buffers. This is useful for debugging. If protocol != 0 then only received buffers with same protocol id are accepted. If you need to work with multiple protocols you can register multiple instances of regDev using the same DEVICE but different NAME and PROTOCOL. (default: 0)
    # USER_OFFSET   offset from the start of the data buffer that we are using. (default: 16)
    
    runScript $(mrfioc2_regDev_DIR)/mrfioc2_regDev_pulseID_RX.cmd, "SYS=STEST-VME-PULSERXTST, DEVICE=EVR0, TYPE=300"


## Register the driver

mrfioc2_regDev is registered via iocsh command:

    mrfioc2_regDevConfigure name device type protocol userOffset
where:

* `name` is the name of device as seen from regDev. E.g. this name must be the same as parameter 1 in record OUT/IN links.
* `device` is the name of timing card (EVG0, EVR0, EVR1, ...)
* `type` is the data buffer type. 230 or 300
* `protocol` is the protocol ID (32 bit int). Useful when using 230 series hardware. 300 series uses segmented data buffer, which makes the protocol ID redundant. If protocol ID is set to 0, than receiver will accept all buffers. This is useful for debugging. If protocol != 0 then only received buffers with same protocol ID are accepted. If you need to work with multiple protocols you can register multiple instances of regDev using the same `device` but different `name` and protocols.
* `userOffset` is the offset from the start of the data buffer that we are using. When not provided defaults to 16.
 
__Example:__

    mrfioc2_regDevConfigure EVG0DBUF EVG0 300 0 16
registers timing card `EVR0` with regDev name `EVR0DBUF` for data buffer type 300. No protocol is used. When records are reading and writing from offset 1, data buffer will in fact be accessed on offset 17 (because of user offset set to 16). When writing to offset 0, current data in the data buffer will be sent. Depending on addresses which records use, appropriate data buffer segments will be monitored for new data. 


## Set up the EPICS records
After mrfioc2_regDev is registered via iocsh command, records can be used to access, or write to the data buffer. Records behave the same as for any other regDev device with few exceptions:

* when using `protocol`, offsets 0x00-0x04 can not be written to (since they are occupied by protocol)
* writing to offset 0x00 will flush the buffer (send out the data that was previously written)
* input records are automatically processed (if scan field is set to I/O Intr) when valid data is received. Data is valid when sent and received protocol match (or if the protocol is not used).

__Example:__ and example of compatible `ai` and `ao` type record, that work with example from previous chapter

    record(ai, "RECORD-PREFIX-DBUF-EVR0:RX") {
      field(DTYP, "regDev")
      field(INP, "@EVR0DBUF:0x18 T=UINT32")
      field(SCAN, "I/O Intr")
    }

    record(ao, "RECORD-PREFIX-DBUF-EVR0:TX") {
      field(DTYP, "regDev")
      field(OUT, "@EVR0DBUF:0x18 T=UINT32")
    }

In this example:

* DTYP field is set to `regDev`
* first parameter of the INP / OUT fields is set to the registered regDev name `EVR0DBUF`
* second parameter of the INP / OUT fields is set to the offset the record will read from / write to (`0x18`)
* additional parameter of the INP / OUT fields tells the regDev driver that the record will read / write values with the type of unsigned 32-bit integer (`T=UINT32`)
* SCAN field of the `ai` record is set to `I/O Intr`, which means that it will be automatically processed when data arrives at this offset.

## Version compatibility
mrfioc2_regDev versions 1.0.0 and up are compatible with mrfioc2 version 2.8.0 and above and regDev version 3.4.7 (possibly newer version as well)
mrfioc2_regDev version 0.1.0 is compatible with mrfioc2 versions 2.7.4 and above. 

## Building from scratch

The mrfioc2_regDev is structured as an ordinary EPICS application. In order to build it from source:

* clone the sources from git repository by running command `git clone https://git.psi.ch/cosylab/mrfioc2_regdev.git`, which creates a top folder called `mrfioc2_regdev`.
* update files in `mrfioc2_regdev/configure` folder to match your system (eg. set paths in `configure/RELEASE`).
* run `make -f Makefile` in the `mrfioc2_regdev` folder.

### PSI
Building the driver on the PSI infrastructure is a bit different, since it leverages the driver.makefile. In order to build it:

* clone the sources from git repository by running command `git clone https://git.psi.ch/cosylab/mrfioc2_regdev.git`, which creates a top folder called `mrfioc2_regdev`.
* run `make` in the `mrfioc2_regdev` folder on the build server.
* to install the driver run `make install` in the `mrfioc2_regdev` folder on the build server.

The driver builds as a single library, which can be loaded using `require` to your IOC. Installation process also copies all the necessary support files (eg. templates) to the appropriate module folder. For more options inspect driver.makefile and require documentation available at the PSI wiki.


# Authors 

- Tom Slejko (tom.slejko@cosylab.com)
- Babak Kalantari (babak.kalantari@psi.ch)
- Saso Skube (saso.skube@cosylab.com)
