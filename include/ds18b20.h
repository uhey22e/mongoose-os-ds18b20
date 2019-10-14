#include "mgos.h"
#include "mgos_onewire.h"

#include "mjs.h"

typedef struct ds18b20_result {
    uint8_t rom[8];
    char mac[24];
    float temp;
    struct ds18b20_result* next;
} ds18b20_result;

typedef void (*ds18b20_read_t)(ds18b20_result *results);

void ds18b20_init(int pin, int res);
void ds18b20_deinit(void);
void ds18b20_find_sensors(void);
void ds18b20_read_all(ds18b20_read_t callback);

// For mJS API
const struct mjs_c_struct_member* get_ds18b20_result_descr( void );
void ds18b20_read_all_wrap( void (*callback)(void *), void* user_data );