/****************************************************************************
**
** Copyright (C) 2015 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef PEAKCAN_SYMBOLS_P_H
#define PEAKCAN_SYMBOLS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#ifdef LINK_LIBPCANBASIC

extern "C"
{
#include <pcanbasic.h>
}

#else

#include <QtCore/qlibrary.h>
#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>

#ifdef Q_OS_WIN32
#include <windows.h>
#define DRV_CALLBACK_TYPE WINAPI
#else
#define DRV_CALLBACK_TYPE
#endif

// Currently defined and supported PCAN channels
#define PCAN_NONEBUS             0x00  // Undefined/default value for a PCAN bus

#define PCAN_ISABUS1             0x21  // PCAN-ISA interface, channel 1
#define PCAN_ISABUS2             0x22  // PCAN-ISA interface, channel 2
#define PCAN_ISABUS3             0x23  // PCAN-ISA interface, channel 3
#define PCAN_ISABUS4             0x24  // PCAN-ISA interface, channel 4
#define PCAN_ISABUS5             0x25  // PCAN-ISA interface, channel 5
#define PCAN_ISABUS6             0x26  // PCAN-ISA interface, channel 6
#define PCAN_ISABUS7             0x27  // PCAN-ISA interface, channel 7
#define PCAN_ISABUS8             0x28  // PCAN-ISA interface, channel 8

#define PCAN_DNGBUS1             0x31  // PCAN-Dongle/LPT interface, channel 1

#define PCAN_PCIBUS1             0x41  // PCAN-PCI interface, channel 1
#define PCAN_PCIBUS2             0x42  // PCAN-PCI interface, channel 2
#define PCAN_PCIBUS3             0x43  // PCAN-PCI interface, channel 3
#define PCAN_PCIBUS4             0x44  // PCAN-PCI interface, channel 4
#define PCAN_PCIBUS5             0x45  // PCAN-PCI interface, channel 5
#define PCAN_PCIBUS6             0x46  // PCAN-PCI interface, channel 6
#define PCAN_PCIBUS7             0x47  // PCAN-PCI interface, channel 7
#define PCAN_PCIBUS8             0x48  // PCAN-PCI interface, channel 8

#define PCAN_USBBUS1             0x51  // PCAN-USB interface, channel 1
#define PCAN_USBBUS2             0x52  // PCAN-USB interface, channel 2
#define PCAN_USBBUS3             0x53  // PCAN-USB interface, channel 3
#define PCAN_USBBUS4             0x54  // PCAN-USB interface, channel 4
#define PCAN_USBBUS5             0x55  // PCAN-USB interface, channel 5
#define PCAN_USBBUS6             0x56  // PCAN-USB interface, channel 6
#define PCAN_USBBUS7             0x57  // PCAN-USB interface, channel 7
#define PCAN_USBBUS8             0x58  // PCAN-USB interface, channel 8

#define PCAN_PCCBUS1             0x61  // PCAN-PC Card interface, channel 1
#define PCAN_PCCBUS2             0x62  // PCAN-PC Card interface, channel 2

// Represent the PCAN error and status codes
#define PCAN_ERROR_OK            0x00000  // No error
#define PCAN_ERROR_XMTFULL       0x00001  // Transmit buffer in CAN controller is full
#define PCAN_ERROR_OVERRUN       0x00002  // CAN controller was read too late
#define PCAN_ERROR_BUSLIGHT      0x00004  // Bus error: an error counter reached the 'light' limit
#define PCAN_ERROR_BUSHEAVY      0x00008  // Bus error: an error counter reached the 'heavy' limit
#define PCAN_ERROR_BUSOFF        0x00010  // Bus error: the CAN controller is in bus-off state
#define PCAN_ERROR_ANYBUSERR     (PCAN_ERROR_BUSLIGHT | PCAN_ERROR_BUSHEAVY | PCAN_ERROR_BUSOFF) // Mask for all bus errors
#define PCAN_ERROR_QRCVEMPTY     0x00020  // Receive queue is empty
#define PCAN_ERROR_QOVERRUN      0x00040  // Receive queue was read too late
#define PCAN_ERROR_QXMTFULL      0x00080  // Transmit queue is full
#define PCAN_ERROR_REGTEST       0x00100  // Test of the CAN controller hardware registers failed (no hardware found)
#define PCAN_ERROR_NODRIVER      0x00200  // Driver not loaded
#define PCAN_ERROR_HWINUSE       0x00400  // Hardware already in use by a Net
#define PCAN_ERROR_NETINUSE      0x00800  // A Client is already connected to the Net
#define PCAN_ERROR_ILLHW         0x01400  // Hardware handle is invalid
#define PCAN_ERROR_ILLNET        0x01800  // Net handle is invalid
#define PCAN_ERROR_ILLCLIENT     0x01C00  // Client handle is invalid
#define PCAN_ERROR_ILLHANDLE     (PCAN_ERROR_ILLHW | PCAN_ERROR_ILLNET | PCAN_ERROR_ILLCLIENT)  // Mask for all handle errors
#define PCAN_ERROR_RESOURCE      0x02000  // Resource (FIFO, Client, timeout) cannot be created
#define PCAN_ERROR_ILLPARAMTYPE  0x04000  // Invalid parameter
#define PCAN_ERROR_ILLPARAMVAL   0x08000  // Invalid parameter value
#define PCAN_ERROR_UNKNOWN       0x10000  // Unknow error
#define PCAN_ERROR_ILLDATA       0x20000  // Invalid data, function, or action
#define PCAN_ERROR_INITIALIZE    0x40000  // Channel is not initialized
#define PCAN_ERROR_ILLOPERATION  0x80000  // Invalid operation

// PCAN devices
#define PCAN_NONE                0x00  // Undefined, unknown or not selected PCAN device value
#define PCAN_PEAKCAN             0x01  // PCAN Non-Plug&Play devices. NOT USED WITHIN PCAN-Basic API
#define PCAN_ISA                 0x02  // PCAN-ISA, PCAN-PC/104, and PCAN-PC/104-Plus
#define PCAN_DNG                 0x03  // PCAN-Dongle
#define PCAN_PCI                 0x04  // PCAN-PCI, PCAN-cPCI, PCAN-miniPCI, and PCAN-PCI Express
#define PCAN_USB                 0x05  // PCAN-USB and PCAN-USB Pro
#define PCAN_PCC                 0x06  // PCAN-PC Card

// PCAN parameters
#define PCAN_DEVICE_NUMBER       0x01  // PCAN-USB device number parameter
#define PCAN_5VOLTS_POWER        0x02  // PCAN-PC Card 5-Volt power parameter
#define PCAN_RECEIVE_EVENT       0x03  // PCAN receive event handler parameter
#define PCAN_MESSAGE_FILTER      0x04  // PCAN message filter parameter
#define PCAN_API_VERSION         0x05  // PCAN-Basic API version parameter
#define PCAN_CHANNEL_VERSION     0x06  // PCAN device channel version parameter
#define PCAN_BUSOFF_AUTORESET    0x07  // PCAN Reset-On-Busoff parameter
#define PCAN_LISTEN_ONLY         0x08  // PCAN Listen-Only parameter
#define PCAN_LOG_LOCATION        0x09  // Directory path for log files
#define PCAN_LOG_STATUS          0x0A  // Debug-Log activation status
#define PCAN_LOG_CONFIGURE       0x0B  // Configuration of the debugged information (LOG_FUNCTION_***)
#define PCAN_LOG_TEXT            0x0C  // Custom insertion of text into the log file
#define PCAN_CHANNEL_CONDITION   0x0D  // Availability status of a PCAN-Channel
#define PCAN_HARDWARE_NAME       0x0E  // PCAN hardware name parameter
#define PCAN_RECEIVE_STATUS      0x0F  // Message reception status of a PCAN-Channel
#define PCAN_CONTROLLER_NUMBER   0x10  // CAN-Controller number of a PCAN-Channel
#define PCAN_TRACE_LOCATION      0x11  // Directory path for PCAN trace files
#define PCAN_TRACE_STATUS        0x12  // CAN tracing activation status
#define PCAN_TRACE_SIZE          0x13  // Configuration of the maximum file size of a CAN trace
#define PCAN_TRACE_CONFIGURE     0x14  // Configuration of the trace file storing mode (TRACE_FILE_***)
#define PCAN_CHANNEL_IDENTIFYING 0x15  // Phisical identification of a USB based PCAN-Channel by blinking its associated LED

// PCAN parameter values
#define PCAN_PARAMETER_OFF       0x00  // The PCAN parameter is not set (inactive)
#define PCAN_PARAMETER_ON        0x01  // The PCAN parameter is set (active)
#define PCAN_FILTER_CLOSE        0x00  // The PCAN filter is closed. No messages will be received
#define PCAN_FILTER_OPEN         0x01  // The PCAN filter is fully opened. All messages will be received
#define PCAN_FILTER_CUSTOM       0x02  // The PCAN filter is custom configured. Only registered messages will be received
#define PCAN_CHANNEL_UNAVAILABLE 0x00  // The PCAN-Channel handle is illegal, or its associated hadware is not available
#define PCAN_CHANNEL_AVAILABLE   0x01  // The PCAN-Channel handle is available to be connected (Plug&Play Hardware: it means furthermore that the hardware is plugged-in)
#define PCAN_CHANNEL_OCCUPIED    0x02  // The PCAN-Channel handle is valid, and is already being used

#define LOG_FUNCTION_DEFAULT     0x00    // Logs system exceptions / errors
#define LOG_FUNCTION_ENTRY       0x01    // Logs the entries to the PCAN-Basic API functions
#define LOG_FUNCTION_PARAMETERS  0x02    // Logs the parameters passed to the PCAN-Basic API functions
#define LOG_FUNCTION_LEAVE       0x04    // Logs the exits from the PCAN-Basic API functions
#define LOG_FUNCTION_WRITE       0x08    // Logs the CAN messages passed to the CAN_Write function
#define LOG_FUNCTION_READ        0x10    // Logs the CAN messages received within the CAN_Read function
#define LOG_FUNCTION_ALL         0xFFFF  // Logs all possible information within the PCAN-Basic API functions

#define TRACE_FILE_SINGLE        0x00  // A single file is written until it size reaches PAN_TRACE_SIZE
#define TRACE_FILE_SEGMENTED     0x01  // Traced data is distributed in several files with size PAN_TRACE_SIZE
#define TRACE_FILE_DATE          0x02  // Includes the date into the name of the trace file
#define TRACE_FILE_TIME          0x04  // Includes the start time into the name of the trace file
#define TRACE_FILE_OVERWRITE     0x80  // Causes the overwriting of available traces (same name)

// PCAN message types
#define PCAN_MESSAGE_STANDARD    0x00  // The PCAN message is a CAN Standard Frame (11-bit identifier)
#define PCAN_MESSAGE_RTR         0x01  // The PCAN message is a CAN Remote-Transfer-Request Frame
#define PCAN_MESSAGE_EXTENDED    0x02  // The PCAN message is a CAN Extended Frame (29-bit identifier)
#define PCAN_MESSAGE_STATUS      0x80  // The PCAN message represents a PCAN status message

// Frame Type / Initialization Mode
#define PCAN_MODE_STANDARD       PCAN_MESSAGE_STANDARD
#define PCAN_MODE_EXTENDED       PCAN_MESSAGE_EXTENDED

// Baud rate codes = BTR0/BTR1 register values for the CAN controller.
// You can define your own Baud rate with the BTROBTR1 register.
// Take a look at www.peak-system.com for our free software "BAUDTOOL"
// to calculate the BTROBTR1 register for every baudrate and sample point.
#define PCAN_BAUD_1M             0x0014  //   1 MBit/s
#define PCAN_BAUD_800K           0x0016  // 800 kBit/s
#define PCAN_BAUD_500K           0x001C  // 500 kBit/s
#define PCAN_BAUD_250K           0x011C  // 250 kBit/s
#define PCAN_BAUD_125K           0x031C  // 125 kBit/s
#define PCAN_BAUD_100K           0x432F  // 100 kBit/s
#define PCAN_BAUD_95K            0xC34E  //  95,238 kBit/s
#define PCAN_BAUD_83K            0x852B  //  83,333 kBit/s
#define PCAN_BAUD_50K            0x472F  //  50 kBit/s
#define PCAN_BAUD_47K            0x1414  //  47,619 kBit/s
#define PCAN_BAUD_33K            0x8B2F  //  33,333 kBit/s
#define PCAN_BAUD_20K            0x532F  //  20 kBit/s
#define PCAN_BAUD_10K            0x672F  //  10 kBit/s
#define PCAN_BAUD_5K             0x7F7F  //   5 kBit/s

#define PCAN_TYPE_ISA            0x01  // PCAN-ISA 82C200
#define PCAN_TYPE_ISA_SJA        0x09  // PCAN-ISA SJA1000
#define PCAN_TYPE_ISA_PHYTEC     0x04  // PHYTEC ISA
#define PCAN_TYPE_DNG            0x02  // PCAN-Dongle 82C200
#define PCAN_TYPE_DNG_EPP        0x03  // PCAN-Dongle EPP 82C200
#define PCAN_TYPE_DNG_SJA        0x05  // PCAN-Dongle SJA1000
#define PCAN_TYPE_DNG_SJA_EPP    0x06  // PCAN-Dongle EPP SJA1000

// Type definitions
#define TPCANHandle              quint8  // Represents a PCAN hardware channel handle
#define TPCANStatus              quint32 // Represents a PCAN status/error code
#define TPCANParameter           quint8  // Represents a PCAN parameter to be read or set
#define TPCANDevice              quint8  // Represents a PCAN device
#define TPCANMessageType         quint8  // Represents the type of a PCAN message
#define TPCANType                quint8  // Represents the type of PCAN hardware to be initialized
#define TPCANMode                quint8  // Represents a PCAN filter mode
#define TPCANBaudrate            quint16  // Represents a PCAN Baud rate register value

// Represents a PCAN message
typedef struct tagTPCANMsg
{
    quint32             ID;      // 11/29-bit message identifier
    TPCANMessageType    MSGTYPE; // Type of the message
    quint8              LEN;     // Data Length Code of the message (0..8)
    quint8              DATA[8]; // Data of the message (DATA[0]..DATA[7])
} TPCANMsg;

// Represents a timestamp of a received PCAN message
// Total Microseconds = micros + 1000 * millis + 0xFFFFFFFF * 1000 * millis_overflow
typedef struct tagTPCANTimestamp
{
    quint32   millis;             // Base-value: milliseconds: 0.. 2^32-1
    quint16   millis_overflow;    // Roll-arounds of millis
    quint16   micros;             // Microseconds: 0..999
} TPCANTimestamp;


#define GENERATE_SYMBOL_VARIABLE(returnType, symbolName, ...) \
    typedef returnType (DRV_CALLBACK_TYPE *fp_##symbolName)(__VA_ARGS__); \
    static fp_##symbolName symbolName;

#define RESOLVE_SYMBOL(symbolName) \
    symbolName = (fp_##symbolName)resolveSymbol(pcanLibrary, #symbolName); \
    if (!symbolName) \
        return false;

GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_Initialize, TPCANHandle, TPCANBaudrate, TPCANType, quint32, quint16)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_Uninitialize, TPCANHandle)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_Reset, TPCANHandle)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_GetStatus, TPCANHandle)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_Read, TPCANHandle, TPCANMsg *, TPCANTimestamp *)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_Write, TPCANHandle, TPCANMsg *)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_FilterMessages, TPCANHandle, quint32, quint32, TPCANMode)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_GetValue, TPCANHandle, TPCANParameter, void *, quint32)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_SetValue, TPCANHandle, TPCANParameter, void *, quint32)
GENERATE_SYMBOL_VARIABLE(TPCANStatus, CAN_GetErrorText, TPCANStatus, quint16, char *)

inline QFunctionPointer resolveSymbol(QLibrary *pcanLibrary, const char *symbolName)
{
    QFunctionPointer symbolFunctionPointer = pcanLibrary->resolve(symbolName);
    if (!symbolFunctionPointer)
        qWarning("Failed to resolve the pcanbasic symbol: %s", symbolName);

    return symbolFunctionPointer;
}

inline bool resolveSymbols(QLibrary *pcanLibrary)
{
    if (!pcanLibrary->isLoaded()) {
        pcanLibrary->setFileName(QStringLiteral("pcanbasic"));
        if (!pcanLibrary->load()) {
            qWarning("Failed to load the library: %s", qPrintable(pcanLibrary->fileName()));
            return false;
        }
    }

    RESOLVE_SYMBOL(CAN_Initialize)
    RESOLVE_SYMBOL(CAN_Uninitialize)
    RESOLVE_SYMBOL(CAN_Reset)
    RESOLVE_SYMBOL(CAN_GetStatus)
    RESOLVE_SYMBOL(CAN_Read)
    RESOLVE_SYMBOL(CAN_Write)
    RESOLVE_SYMBOL(CAN_FilterMessages)
    RESOLVE_SYMBOL(CAN_GetValue)
    RESOLVE_SYMBOL(CAN_SetValue)
    RESOLVE_SYMBOL(CAN_GetErrorText)

    return true;
}

#endif

#endif // PEAKCAN_SYMBOLS_P_H
