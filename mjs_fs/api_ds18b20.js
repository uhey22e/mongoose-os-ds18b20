let DS18B20 = {
  init: ffi('void ds18b20_init(int, int)'),
  deinit: ffi('void ds18b20_deinit(void)'),
  read_all: ffi('void ds18b20_read_all_wrap(void)'),
  _getResultByIdx: ffi('void * get_result_by_idx(int)'),

  // Cの構造体をmJSのオブジェクトに変換
  getResultByIdx: function (idx) {
    let sd = ffi('void * get_ds18b20_result_descr()')();
    let s = this._getResultByIdx(idx);
    if (s !== null) {
      return s2o(s, sd);
    }
    return null;
  },
};
