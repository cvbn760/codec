#include "m2mb_types.h"
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "app_cfg.h"
#include "m2mb_types.h"
#include "azx_utils.h"
#include "azx_log.h"
#include <stdio.h>
#include <string.h>
#include "m2mb_os_api.h"
#include "m2mb_ati.h"
#include "at_utils.h"
#include "azx_log.h"
#include "app_cfg.h"

static UINT8 sendAT(char *cmd);
static BOOLEAN on_codec(void);
static BOOLEAN send_to_codec(char *str);
static int char2int(char input);
static int hex2bin(const char* src, char* target);
static INT16 instanceID = 0; /*AT0, bound to UART by default config*/
static CHAR rsp[100];
static M2MB_RESULT_E retVal;

static int char2int(char input) {
    if(input >= '0' && input <= '9') return input - '0';
    if(input >= 'A' && input <= 'F') return input - 'A' + 10;
    if(input >= 'a' && input <= 'f') return input - 'a' + 10;
    return -1;
}

// This function assumes src to be a zero terminated sanitized string with
// an even number of [0-9a-f] characters, and target to be sufficiently large
static int hex2bin(const char* src, char* target) {
    while(src[0] && src[1]) {
        int a = char2int(src[0]);
        int b = char2int(src[1]);
        if (a < 0 || b < 0) return -1;
        *(target++) = a*16 + b;
        src += 2;
    }
    return 0;
}

static BOOLEAN on_codec(void) {
    const char *req = "AT#DVI=1,2,1\r";
    retVal= send_async_at_command(instanceID,req,rsp, sizeof(rsp)); // эту команду нужно отправлять всегда после включения, даже если модем уже так сконфигурирован
    if ( retVal != M2MB_RESULT_SUCCESS )
    {
        AZX_LOG_ERROR( "Error sending command <%s>r\n", req);
        return FALSE;
    }
    else
    {
        AZX_LOG_INFO("Command response: <%s>\r\n\r\n", rsp);
        return TRUE;
    }
}

static BOOLEAN send_to_codec(char *str) {
    const int addr = 0x10;
    int i2c_fd = 0;
    char data[128];
    memset(data,0,128);
    if (hex2bin(str, data) < 0) {
        return FALSE;
    }
    // Open a connection to the I2C userspace control file.
    if ((i2c_fd = open("/dev/i2c-4", O_RDWR)) < 0) {
        AZX_LOG_ERROR("[I2C] Unable to open i2c_4 control file");
        return FALSE;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
        AZX_LOG_ERROR("[I2C] Unable to set slave addr");
        close(i2c_fd);
        return FALSE;
    }
    int len = strlen(str)/2;
    AZX_LOG_DEBUG("writing to i2c %s : %d bytes", str, len);
    if (write(i2c_fd, data, len) != len) {
        AZX_LOG_ERROR("Failed to write to the i2c bus");
        close(i2c_fd);
        return FALSE;
    }
    close(i2c_fd);
    return TRUE;
}

static UINT8 sendAT(char *cmd) {
    retVal = send_async_at_command(instanceID, cmd, rsp, sizeof(rsp));
    if ( retVal != M2MB_RESULT_SUCCESS )
    {
        AZX_LOG_ERROR( "Error sending command <%s>\n", cmd);
        return FALSE;
    }
    else
    {
        AZX_LOG_INFO("Command response: <%s>\r\n\r\n", rsp);
        return TRUE;
    }
}

// CODEC > MAX9860
void M2MB_main( int argc, char **argv ) {
  (void)argc;
  (void)argv;

  m2mb_os_taskSleep( M2MB_OS_MS2TICKS(2000) );
//    AZX_LOG_INIT();
    AZX_LOG_INFO("Starting AT demo app. This is v%s built on %s %s.\r\n",
                 VERSION, __DATE__, __TIME__);

    retVal = at_cmd_async_init(instanceID);
    if ( retVal == M2MB_RESULT_SUCCESS )
    {
        AZX_LOG_TRACE( "at_cmd_sync_init() returned success value\r\n" );
    }
    else
    {
        AZX_LOG_ERROR( "at_cmd_sync_init() returned failure value\r\n" );
        return;
    }
    sendAT("AT#VAUX=1,1\r");
    sendAT("AT#GPIO=7,1,1\r");
    on_codec();
    send_to_codec("0220101000242000003300540000008b");
    sendAT("AT#APLAY=1,0,\"start_test.wav\"\r");
//    azx_sleep_ms(10000);
//    send_to_codec("0220000000000000000000000000000b");
}

