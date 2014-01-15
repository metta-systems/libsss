#pragma once

#include <termios.h>
#include <string>
#include "arsenal/flurry.h"

class shell_protocol
{
public:
    // Standard service and protocol names for this protocol.
    static const std::string service_name;
    static const std::string protocol_name;

    enum Command {
        Invalid = 0,
        Terminal,   // Request pseudo-terminal
        Shell,      // Start a shell process
        Exec,       // Execute a specific command
        ExitStatus, // Indicate remote process's exit status
        ExitSignal, // Indicate remote process killed by signal
    };

    static const char ControlMarker = 1;    // ASCII 'SOH'


    // Terminal input flags
    enum InputFlag {
        tBRKINT     = 0x0001,
        tICRNL      = 0x0002,
        tIGNBRK     = 0x0004,
        tIGNCR      = 0x0008,
        tIGNPAR     = 0x0010,
        tINLCR      = 0x0020,
        tINPCK      = 0x0040,
        tISTRIP     = 0x0080,
        tIXANY      = 0x0100,
        tIXOFF      = 0x0200,
        tIXON       = 0x0400,
        tPARMRK     = 0x0800,
        tIUCLC      = 0x1000,
    };

    // Terminal output flags
    enum OutputFlag {
        tOPOST      = 0x0001,
        tOLCUC      = 0x0002,
        tONLCR      = 0x0004,
        tOCRNL      = 0x0008,
        tONOCR      = 0x0010,
        tONLRET     = 0x0020,
    };

    // Terminal control flags
    enum ControlFlag {
        tCS8        = 0x0001,
        tCSTOPB     = 0x0002,
        tPARENB     = 0x0004,
        tPARODD     = 0x0008,
        tHUPCL      = 0x0010,
        tCLOCAL     = 0x0020,
    };

    // Terminal local flags
    enum LocalFlag {
        tECHO       = 0x0001,
        tECHOE      = 0x0002,
        tECHOK      = 0x0004,
        tECHONL     = 0x0008,
        tICANON     = 0x0010,
        tIEXTEN     = 0x0020,
        tISIG       = 0x0040,
        tNOFLSH     = 0x0080,
        tTOSTOP     = 0x0100,
    };

    // Special character indexes
    static const int tVEOF      = 0;
    static const int tVEOL      = 1;
    static const int tVERASE    = 2;
    static const int tVINTR     = 3;
    static const int tVKILL     = 4;
    static const int tVQUIT     = 5;
    static const int tVSTART    = 6;
    static const int tVSTOP     = 7;
    static const int tVSUSP     = 8;
    static const int tNCCS      = 9;

    // Size of input/output buffers for shell I/O forwarding
    static const int shellBufferSize = 16384;

    static void termpack(flurry::oarchive& xs, struct termios const& tios);
    static void termunpack(flurry::iarchive& xs, struct termios& tios);

private:
    static int termpackspeed(speed_t speed);
    static speed_t termunpackspeed(uint32_t speed);
};

