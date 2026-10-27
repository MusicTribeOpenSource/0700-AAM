#include "kick_HardwareMode.h"

/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "kick_UsbFunctionFS.h"

#if( JUCE_LINUX || __linux__ )
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <pthread.h>
    #include <errno.h>
    #include <stdint.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <string.h>
    #include <sys/wait.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <dirent.h>
    #include <limits.h>

#endif

namespace mt
{
#define MAX_FRAME_SIZE      64
#define MAX_FRAME_DATA_SIZE 60

    union FSdata
    {
        uint8_t raw[MAX_FRAME_SIZE];
        struct
        {
            uint32_t key;
            uint8_t  data[MAX_FRAME_SIZE - sizeof( uint32_t )];
        };
    };

    USBFunctionFS::USBFunctionFS()
        : juce::Thread( "USBFS" )
    {
        init();
    }

    USBFunctionFS::~USBFunctionFS()
    {
        signalThreadShouldExit();
        stopThread( 2000 );

        closeFileDescriptor( g_ep_write );
        closeFileDescriptor( g_ep_read );
    }

    void USBFunctionFS::init()
    {
        if (!hardwareEnabled()) return;
        const juce::String& usbFilePath = "/dev/usb-utp0/ep0";
        juce::String        epSinkPath  = usbFilePath.substring( 0, usbFilePath.length() - 1 ) + "1";
        g_ep_write                      = openFileDescriptor( epSinkPath );
        if( g_ep_write < 0 )
            handleOpenFileError( epSinkPath );

        juce::String epSourcePath = usbFilePath.substring( 0, usbFilePath.length() - 1 ) + "2";
        g_ep_read                 = openFileDescriptor( epSourcePath );
        if( g_ep_read < 0 )
            handleOpenFileError( epSourcePath );
    }

    void USBFunctionFS::run()
    {
        constexpr int timeoutSeconds = 15; // Adjust the timeout value as needed
	isConnected.store(false);
        while( !threadShouldExit() )
        {
            if( g_ep_read > 0 && g_ep_write > 0 )
            {
                if( !timerRunning )
                {
                    DBG( "STARting Timer" );
                    startTimer( timeoutSeconds * 1000 ); // Convert seconds to milliseconds
                    timerRunning = true;
                }

                juce::String command;
                readCommandFromUSB( command );
                DBG( "After read Command From USB" );
                if( !command.isEmpty() )
                {
                    DBG( "Command Recieved." );
                    responseReceived = true;
                    isConnected.store( true );
                    handleCommand( command );
                    stopTimer();
                    timerRunning = false;
                }
            }
            else
            {
                init();
            }
            DBG( "making response Recieved False" );
            responseReceived = false; // Reset flag for the next iteration

            // Sleep for a short duration to avoid busy-waiting
            juce::Thread::sleep( 100 );
        }
    }

    void USBFunctionFS::timerCallback()
    {
        DBG( "Timer Callback Called" );
        if( !responseReceived )
        {
            DBG( "No response from PC app within timeout period" );
            isConnected.store( false );
            signalThreadShouldExit();
            usbmsg_t data;
            data.command_status   = mt ::USB_STATUS::U_TIMEOUT;
            data.downloadfilesize = 0;
            data.percentage       = 0;
            if( usbCallback )
            {
                usbCallback( data );
            }
            timerRunning = false;
            stopTimer();
        }
        // Handle timeout here
    }

    void USBFunctionFS::start()
    {
        if( g_ep_write > 0 && g_ep_read > 0 )
        {
            startThread();
        }
    }

    void USBFunctionFS::stop()
    {
	    DBG("USB FUnction STop Called");
        if( g_ep_write > 0 && g_ep_read > 0 )
        {
            signalThreadShouldExit();
        }
    }
    int USBFunctionFS::openFileDescriptor( const juce::String& filePath )
    {
        int fd = -1;
#if( JUCE_LINUX || __linux__ )
        fd = open( filePath.toRawUTF8(), O_RDWR );
#endif
        return fd;
    }

    void USBFunctionFS::closeFileDescriptor( int fd )
    {
#if( JUCE_LINUX || __linux__ )
        if( fd >= 0 )
            ::close( fd );
#endif
    }

    void USBFunctionFS::handleOpenFileError( const juce::String& filePath )
    {
        DBG( "Can't open file " << filePath );
    }

    int USBFunctionFS::writeData( void* p, size_t size )
    {
        int r = 0;
#if( JUCE_LINUX || __linux__ )
        int flags = fcntl( g_ep_write, F_GETFL, 0 );
        if( flags == -1 )
        {
            // Handle error
        }
        flags |= O_NONBLOCK;
        if( fcntl( g_ep_write, F_SETFL, flags ) == -1 )
        {
            // Handle error
        }
        r = write( g_ep_write, p, size );
        if( r < 0 )
            printf( "failure write to usb ep\n" );
#endif
        return r;
    }

    int USBFunctionFS::writeFile( int fp, void* p, size_t size )
    {
#if( JUCE_LINUX || __linux__ )
        int            flag;
        fd_set         rfds;
        struct timeval tv;
        ssize_t        sz;
        uint8_t*       buff = (uint8_t*)p;
        union FSdata   fm;
        int            flags;

        tv.tv_sec  = 0;
        tv.tv_usec = 100000;

        flags = fcntl( fp, F_GETFL );
        flags |= O_NONBLOCK;
        if( fcntl( fp, F_SETFL, flags ) )
        {
            printf( "fctl failure\n" );
            return -1;
        }

        FD_ZERO( &rfds );
        FD_SET( fp, &rfds );

        while( size > 0 )
        {
            sz = write( fp, buff, size );

            if( sz == -1 )
            {
                if( errno == EAGAIN )
                    sz = 0;
                else
                    return -errno;
            }

            buff += sz;
            size -= sz;
            fm.key = INFO;
            writeData( &fm, 4 );
            select( fp + 1, NULL, &rfds, NULL, &tv );
        }

        if( size == 0 )
            return buff - (uint8_t*)p;
        else
#endif
            return -1;
    }

    void USBFunctionFS::readCommandFromUSB( juce::String& command )
    {
#if( JUCE_LINUX || __linux__ )
        constexpr int bufferSize = 512;
        char          buffer[bufferSize];
        memset( buffer, 0, bufferSize );

        int flags = fcntl( g_ep_read, F_GETFL, 0 );
        fcntl( g_ep_read, F_SETFL, flags | O_NONBLOCK );

        ssize_t bytesRead = ::read( g_ep_read, buffer, bufferSize - 1 );
        if( bytesRead > 0 )
        {
            buffer[bytesRead] = '\0';
            command           = juce::String( buffer );
        }
        else
        {
            DBG( "Failed to read comamnd from Read FD" );
        }
#endif
    }

#if defined( JUCE_LINUX ) || defined( __linux__ )
    pid_t USBFunctionFS::popen2( const char* command, int* infp, int* outfp )
    {
        int   p_stdin[2], p_stdout[2];
        pid_t pid;

        if( pipe( p_stdin ) != 0 || pipe( p_stdout ) != 0 )
            return -1;

        pid = fork();

        if( pid < 0 )
            return pid;
        else if( pid == 0 )
        {
            close( p_stdin[WRITE] );
            if( infp == NULL )
                close( p_stdin[READ] );
            else
                dup2( p_stdin[READ], READ );
            close( p_stdout[READ] );
            if( outfp == NULL )
                close( p_stdout[WRITE] );
            else
                dup2( p_stdout[WRITE], WRITE );

            execl( "/bin/sh", "sh", "-c", command, NULL );
            perror( "execl" );
            exit( 1 );
        }

        if( infp == NULL )
            close( p_stdin[WRITE] );
        else
            *infp = p_stdin[WRITE];

        if( outfp == NULL )
            close( p_stdout[READ] );
        else
            *outfp = p_stdout[READ];

        close( p_stdin[READ] );
        close( p_stdout[WRITE] );
        return pid;
    }
#endif

    int USBFunctionFS ::sendAck()
    {
        union FSdata fs;
        fs.key = OKAY;
        // Send the result back
        writeData( &fs, 4 );
        // Implement your Ack handling logic here
        return 0;
    }

    int USBFunctionFS::handleWFileCommand( std::string& filename, size_t size )
    {
        union FSdata fs;
        usbmsg_t     data;
        DBG( "In Wfile Command" );
        std::size_t size_f = size;

#if defined( JUCE_LINUX ) || defined( __linux__ )
        // Open the file for writing
        write_filefd = open( filename.c_str(), O_WRONLY | O_CREAT,
                             S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
#endif

        if( write_filefd < 0 )
            fs.key = FAIL;
        else
            fs.key = OKAY;

        // Send the result back
        writeData( &fs, 4 );
        tfilesize             = size;
        data.totalfilesize    = tfilesize;
        data.command_status   = mt::USB_STATUS::U_INIT;
        data.downloadfilesize = 0;

        if( usbCallback )
        {
            usbCallback( data );
        }

        // Implement your WFile handling logic here
        return 0;
    }

    int USBFunctionFS::handleWDataCommand( uint32_t data_size )
    {
        union FSdata  fs;
        usbmsg_t      data;
        int           rs;
        int           ret       = 0;
        uint32_t      status    = OKAY;
        static size_t dfilesize = 0;
        int           flags;
#if( JUCE_LINUX || __linux__ )
        void* p = malloc( data_size );
        if( p )
        {
            fs.key = DATA;
        }
        else
        {
            fs.key = FAIL;
            writeData( &fs, 4 );
            return -1;
        }
        sprintf( (char*)( fs.data ), "%08X", data_size );
        writeData( &fs, 4 + strlen( (char*)fs.data ) );

        data.command_status   = mt::USB_STATUS::U_DOWNLOAD;
        dfilesize             = dfilesize + data_size;
        data.downloadfilesize = dfilesize;

        if( tfilesize )
        {
            data.percentage = ( static_cast<double>( dfilesize ) / tfilesize ) * 100;
        }
        DBG( "PERCENTAGE " << juce::String( data.percentage ) );
        if( usbCallback )
        {
            usbCallback( data );
        }
        if( read( g_ep_read, p, data_size ) < 0 )
            status = FAIL;

        ret = writeFile( write_filefd, p, data_size );
        if( ret < 0 )
            status = FAIL;

        free( p );

        memset( &fs, 0, sizeof( fs ) );
        if( stdoutfd >= 0 )
        {
            flags = fcntl( stdoutfd, F_GETFL );
            flags |= O_NONBLOCK;
            if( fcntl( stdoutfd, F_SETFL, flags ) )
            {
                DBG( "fctl failure\n" );
                return -1;
            }
            while( ( ( rs = read( stdoutfd, fs.data, MAX_FRAME_DATA_SIZE ) ) > 0 ) && threadShouldExit() )
            {
                fs.key = INFO;
                writeData( &fs, rs + 4 );
            }
        }
        fs.key = status;
        if( ret == -EPIPE )
        {
            strcpy( (char*)fs.data, "EPIPE" );
            writeData( &fs, 4 + strlen( "EPIPE" ) );
        }
        else
        {
            writeData( &fs, 4 );
        }
#endif
        return 0;
    }

    int USBFunctionFS::handleReadFileCommand( std::string path )
    {
        DBG( "In ReadFile Command" << path );
#if( JUCE_LINUX || __linux__ )
        union FSdata  fs;
        std::ifstream file;
        size_t        size = 0;
        struct stat   st;
        int           rz = 4;
        file             = std::ifstream( path.c_str(), std::ios::binary );
        size             = calculateSizeofFile( &file );
        memset( &st, 0, sizeof( st ) );
        stat( path.c_str(), &st );
        size = st.st_size;
        sprintf( (char*)fs.data, "%016zX", size );
        rz = 4 + strlen( (char*)fs.data );
        printf( "filesize:%d\n", size );

        if( !file.is_open() )
        {
            fs.key = FAIL;
            DBG( "file doesn't exists" );
        }
        else
        {
            DBG( "file exists" );
            file.seekg( 0, std::ios::beg );
            file.close();
            fs.key = OKAY;
            writeData( &fs, rz );
        }
#endif
        return 0;
    }

    int USBFunctionFS::handleCloseFileCommand()
    {
        usbmsg_t     data;
        union FSdata fs;
#if( JUCE_LINUX || __linux__ )
        close( write_filefd );
        write_filefd = -1;
        fs.key       = OKAY;
        writeData( &fs, 4 );
#endif
        data.command_status   = mt::USB_STATUS::U_DONE;
        data.percentage       = 100.0;
        data.downloadfilesize = data.totalfilesize;
        if( usbCallback )
        {
            usbCallback( data );
        }
        return 0;
    }

    int USBFunctionFS::handlPCstartOpration()
    {
        sendAck();
        return 0;
    }

    int USBFunctionFS::handlPCendOpration()
    {
        sendAck();
        return 0;
    }

    int USBFunctionFS::handleCommand( const juce::String& command )
    {
        union FSdata fs;
        usbmsg_t     data;
        // Implement your command handling logic here
        uint32_t status;
        int      flags;
#if defined( JUCE_LINUX ) || defined( __linux__ )
        fd_set         rfds;
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 50000;
#endif
        std::ifstream file;
        const char*   buff = command.toRawUTF8();
        DBG( "Command Recieved " << command );
        if( strncmp( buff, "Ack:", 4 ) == 0 )
        {
            DBG( "ACk Recived from app" );
            const char* ptr = buff + 4; // Move the pointer to skip "WFile:"

            // Extract the filename
            std::string KickVersion( ptr );

            DBG( "KickVersion" << KickVersion );
	    
            mt::FirmwareUpdate::getSwupdateInstance().setPcSwVersion( KickVersion );
            sendAck();
        }
        else if( strncmp( buff, "PC_OPERATION_START", 18 ) == 0 )
        {
            handlPCstartOpration();
            DBG( "PC app started comm" );
        }
        else if( strncmp( buff, "PC_OPERATION_END", 16 ) == 0 )
        {
            handlPCendOpration();
            DBG( "PC app stoped comm" );
        }
        else if( strncmp( buff, "WFile:", 6 ) == 0 )
        {
            DBG( "IN Wfile Command" );
            const char* ptr = buff + 6; // Move the pointer to skip "WFile:"

            // Find the position of ':' after filename
            const char* colonPtr = strchr( ptr, ':' );
            if( colonPtr == nullptr )
            {
                DBG( "Invalid command format: Missing ':'" );
                // Handle the error or return
                return -1;
            }

            // Extract the filename
            std::string filename( ptr, colonPtr - ptr );

            // Extract the size
            std::string sizeStr( colonPtr + 1 ); // Points to the character after ':'
            std::size_t size = std::stoul( sizeStr );

            handleWFileCommand( filename, size );
        }
        else if( strncmp( buff, "Wdata:", 6 ) == 0 ) //download
        {
            DBG( "In Wdata Command " );
            uint32_t data_size; /* download:0x100000 */
            data_size = strtoul( buff + 6, NULL, 16 );
            handleWDataCommand( data_size );
        }
        else if( strncmp( buff, "FirmwareComplete", 16 ) == 0 )
        {
            handleCloseFileCommand();
        }
        else if( strncmp( buff, "Closefile", 9 ) == 0 ) //close
        {
            handleCloseFileCommand();
            DBG( "In Closefile Command " );
        }
        else if( strncmp( buff, "ReadFile:", 9 ) == 0 )
        {
            DBG( "ReadFile " << buff );
#if( JUCE_LINUX || __linux__ )

            const char* filePath = buff + 9;
            std::string path     = filePath;
            handleReadFileCommand( filePath );

#endif
        }
        else if( strncmp( buff, "CREATE_DIR", 10 ) == 0 )
        {
            DBG( "Create DIR comamnd Recieved" );
        }

        else if( strncmp( buff, "UCmd:", 5 ) == 0 )
        {
            DBG( "Ucmd" );
        }
        else if( strncmp( buff, "upload", 6 ) == 0 )
        {
            DBG( "Not Applicable for Kick" );
        }
        else if( strncmp( buff, "DeviceInfo:", 11 ) == 0 )
        {
            handleDeviceInfoCommand();
        }
        else if( strncmp( buff, "FactoryReset", 12 ) == 0 )
        {
            DBG( "Not Applicable for Kick" );
        }
        else
        {
            printf( "Unknow Cmd Recieved from PC %s\n", buff );
        }

        return 0;
    }

    size_t USBFunctionFS::calculateSizeofFile( std::ifstream* file )
    {
        size_t size;
        file->seekg( 0, std::ios::end );
        size = file->tellg();
        file->seekg( 0, std::ios::beg );
        return size;
    }

    int USBFunctionFS::handleDeviceInfoCommand()
    {
        // pc requesting device status info like ram status,internal and usb status
        union FSdata   fs;
        mt::DeviceInfo info = mt::HwPref::getHwPref().getDeviceInfo();

        writeData( info.serialNumber, sizeof( info.serialNumber ) ); // Send the DeviceInfo struct data
        printf( "Serial Number data: %s\n", info.serialNumber );

        writeData( info.firmwareVersion, sizeof( info.firmwareVersion ) );
        printf( "firmwareVersion data: %s\n", info.firmwareVersion );

        writeData( info.firmwareBuildDate, sizeof( info.firmwareBuildDate ) );
        printf( "firmwareBuildDate data: %s\n", info.firmwareBuildDate );

        writeData( info.memoryStatus.total, sizeof( info.memoryStatus.total ) ); // Send memory status struct
        printf( "memoryStatus.total data: %s\n", info.memoryStatus.total );

        writeData( info.memoryStatus.available, sizeof( info.memoryStatus.available ) ); // Send memory status struct
        printf( "memoryStatus.available data: %s\n", info.memoryStatus.available );

        writeData( info.memoryStatus.used, sizeof( info.memoryStatus.used ) ); // Send memory status struct
        printf( "memoryStatus.used data: %s\n", info.memoryStatus.used );

        writeData( info.internalStorageStatus.totalSpace, sizeof( info.internalStorageStatus.totalSpace ) ); // Send internal storage status struct
        printf( "internalStorageStatus.totalSpace data: %s\n", info.internalStorageStatus.totalSpace );

        writeData( info.internalStorageStatus.availableSpace, sizeof( info.internalStorageStatus.availableSpace ) ); // Send internal storage status struct
        printf( "internalStorageStatus.availableSpace data: %s\n", info.internalStorageStatus.availableSpace );

        writeData( info.internalStorageStatus.usedSpace, sizeof( info.internalStorageStatus.usedSpace ) ); // Send internal storage status struct
        printf( "internalStorageStatus.usedSpace data: %s\n", info.internalStorageStatus.usedSpace );

        writeData( info.usbStorageStatus.totalSpace, sizeof( info.usbStorageStatus.totalSpace ) );
        printf( "usbStorageStatus.totalSpace data: %s\n", info.usbStorageStatus.totalSpace );

        writeData( info.usbStorageStatus.availableSpace, sizeof( info.usbStorageStatus.availableSpace ) );
        printf( "usbStorageStatus.availableSpace data: %s\n", info.usbStorageStatus.availableSpace );

        fs.key = OKAY;
        writeData( &fs, 4 );

        return 0;
    }

#undef MAX_FRAME_SIZE
#undef MAX_FRAME_DATA_SIZE

}
