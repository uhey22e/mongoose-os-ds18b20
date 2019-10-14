let DS18B20 = {
    init: ffi('void ds18b20_init(int, int)'),
    deinit: ffi('void ds18b20_deinit(void)'),
    read_all: ffi('void ds18b20_read_all_wrap( void (*)(userdata), userdata )'),

    // _read_all_callback_js: null,
    // read_all: function(callback) {
    //     this._read_all_callback_js = callback;
    //     this._read_all(function (result_c) {
    //         // let result_js = [];
    //         // while ( result_c != null ) {
    //         //     result_js.append({
    //         //         // rom: result_c.rom,
    //         //         mac: result_c.mac,
    //         //         temp: result_c.temp,
    //         //     });
    //         //     result_c = result_c.next;
    //         // }
    //         // _read_all_callback_js(result_js);
    //         _read_all_callback_js(result_c);
    //     }, null);
    // },

// struct ds18b20_result {
//     uint8_t rom[8];
//     char mac[24];
//     float temp;
//     struct ds18b20_result *next;
// };
}