#include "mgos_timers.h"
#include "ds18b20.h"

static struct mgos_onewire *ow;
static int _pin;
static int _res;
static int _waiting_us;
static uint8_t _cfg_reg;
static int _n_sensors;
static float result;
ds18b20_result* _list;
static volatile bool is_busy;

const uint8_t _ds18b20_family_code = 0x28;
const uint8_t _ds18b20_cmd_skip_rom = 0xCC;
const uint8_t _ds18b20_cmd_write_scratchpad = 0x4E;
const uint8_t _ds18b20_cmd_copy_scratchpad = 0x48;
const uint8_t _ds18b20_cmd_start_conversion = 0x44;

void (*js_callback)(void *) = NULL;
static void mjs_callback( ds18b20_result* result );

// Converts a uint8_t rom address to a MAC address string
inline void addr_to_mac(uint8_t *r, char *str)
{
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
}

// Helper for allocating strings
inline char* new_string(size_t len)
{
    return (char *)malloc(len * sizeof(char));
}

// Helper for allocating struct
#define new(what) (what *)malloc(sizeof(what))

void ds18b20_init(int pin, int res)
{
    int64_t start, end;
    _pin = pin;
    _res = res;
    is_busy = true;

    start = mgos_uptime_micros();

    // Ensure that the instance is not already created
    if (ow != NULL)
    {
        return;
    }

    // Determine configuration
    if (res == 9)
    {
        // 9-bit resolution (93.75ms delay)
        _cfg_reg = 0x1F;
        _waiting_us = 93750;
    }
    else if (res == 10)
    {
        // 10-bit resolution (187.5ms delay)
        _cfg_reg = 0x3F;
        _waiting_us = 187500;
    }
    else if (res == 11)
    {
        // 11-bit resolution (375ms delay)
        _cfg_reg = 0x5F;
        _waiting_us = 375000;
    }
    else /* 12-bit */
    {
        // 12-bit resolution (750ms delay)
        _cfg_reg = 0x7F;
        _waiting_us = 750000;
    } 

    // Create instance
    ow = mgos_onewire_create(pin); // Create one-wire
    if (ow == NULL)
    {
        return;
    }

    // Write the configuration
    mgos_onewire_reset(ow); // Reset
    mgos_onewire_write(ow, _ds18b20_cmd_skip_rom);
    mgos_onewire_write(ow, _ds18b20_cmd_write_scratchpad);
    mgos_onewire_write(ow, 0x00);
    mgos_onewire_write(ow, 0x00);
    mgos_onewire_write(ow, _cfg_reg);
    mgos_onewire_write(ow, _ds18b20_cmd_copy_scratchpad);

    // Find sensors
    ds18b20_find_sensors();

    end = mgos_uptime_micros();
    LOG(LL_INFO, ("Initialize time: %lld", (end - start)));

    is_busy = false;
}

void ds18b20_deinit(void)
{
    ds18b20_result *temp;

    // Cleanup
    is_busy = true;
    while (_list != NULL)
    {                       // Loop over all device results
        temp = _list->next; // Store a ref to the next device
        free(_list->mac);   // Free up the MAC address string
        free(_list);        // Free up the struct
        _list = temp;       // Cleanup next device
    }
    mgos_onewire_close(ow);
    is_busy = false;
}

void ds18b20_find_sensors(void)
{
    uint8_t rom[8];
    ds18b20_result* temp;
    _n_sensors = 0;

    // Find all the sensors
    mgos_onewire_search_clean(ow);                                  // Reset search
    mgos_onewire_target_setup(ow, _ds18b20_family_code);            // Skip devices that are not DS18B20's
    while (mgos_onewire_next(ow, rom, 1))
    {                                                               // Loop over all devices
        _n_sensors++;
        temp = new(ds18b20_result);                                 // Create a new results struct
        temp->mac = new_string(24);
        if (temp == NULL)
        {                                                           // Make sure it worked
            LOG(LL_ERROR, ("Memory allocation failure!"));          // If not, print a useful message
            exit(1);                                                // And blow up
        }
        memcpy(temp->rom, rom, 8);                                  // Copy the ROM code into the result
        addr_to_mac(rom, temp->mac);                                // Convert the rom to a MAC address string
        temp->next = _list;                                         // link to previous sensor
        _list = temp;                                               // set list point to new result
    }
}

// Read all temperatures
void ds18b20_read_all(ds18b20_read_t callback)
{
    uint8_t data[9];
    int16_t raw;
    ds18b20_result* temp;

    if (is_busy)
    {
        LOG(LL_WARN, ("Busy now"));
        return;
    }

    // Start temperature conversion
    is_busy = true;
    mgos_onewire_reset(ow);                                // Reset bus
    mgos_onewire_write(ow, _ds18b20_cmd_skip_rom);         // Skip Rom
    mgos_onewire_write(ow, _ds18b20_cmd_start_conversion); // Start conversion
    mgos_usleep(_waiting_us);                              // Wait for conversion

    // Read temperatures
    temp = _list; // Temporary results holder
    while (temp != NULL)
    {                                           // Loop over all devices
        mgos_onewire_reset(ow);                 // Reset
        mgos_onewire_select(ow, temp->rom);     // Select the device
        mgos_onewire_write(ow, 0xBE);           // Issue read command
        mgos_onewire_read_bytes(ow, data, 9);   // Read the 9 data bytes
        raw = (data[1] << 8) | data[0];         // Get the raw temperature
        _cfg_reg = (data[4] & 0x60);            // Read the config (just in case)
        if (_cfg_reg == 0x00)
            raw = raw & ~7;                     // 9-bit raw adjustment
        else if (_cfg_reg == 0x20)
            raw = raw & ~3;                     // 10-bit raw adjustment
        else if (_cfg_reg == 0x40)
            raw = raw & ~1;                     // 11-bit raw adjustment
        temp->temp = (float)raw / 16.0;         // Convert to celsius and store the temp
        temp = temp->next;                      // Switch to the next sensor in the list
    }

    LOG(LL_DEBUG, ("%lld: Done", mgos_uptime_micros()));

    // Invoke the callback
    if ( callback != NULL ) {
        mgos_invoke_cb((mgos_cb_t)callback, _list, false);
        // callback(_list);
    }

    // reset busy flag
    is_busy = false;
}

bool mgos_mongoose_os_ds18b20_init(void)
{
    return true;
}

// For mJS API
static const struct mjs_c_struct_member ds18b20_result_descr[] = {
    {"mac", offsetof(ds18b20_result, mac), MJS_STRUCT_FIELD_TYPE_CHAR_PTR, NULL},
    {"temp", offsetof(ds18b20_result, temp), MJS_STRUCT_FIELD_TYPE_FLOAT, NULL},
    {NULL, 0, MJS_STRUCT_FIELD_TYPE_INVALID, NULL},
};

const struct mjs_c_struct_member* get_ds18b20_result_descr(void)
{
    return ds18b20_result_descr;
}

void ds18b20_read_all_wrap(void)
{
    ds18b20_read_all(NULL);
}

void * get_result_by_idx(int idx)
{
    ds18b20_result* temp;

    // Get result ptr by idx
    temp = _list;
    for (int i = 0; i < idx; i++) {
        if (temp == NULL) {
            return NULL;
        }
        temp = temp->next;
    }
    return temp;
}
