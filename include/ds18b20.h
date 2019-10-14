struct ds18b20_result {
    uint8_t rom[8];
    char *mac;
    float temp;
    struct ds18b20_result *next;
};

typedef void (*ds18b20_read_t)(struct ds18b20_result *results);

void ds18b20_init(int pin, int res);
void ds18b20_deinit(void);
void ds18b20_read_all(int pin, int resolutionBits, ds18b20_read_t callback);
