// enoch_i2s.c - Driver I2S nativo para MicroPython (ESP32-S3)
// Genera MCLK real por hardware via driver/i2s_std.h (ESP-IDF).
// Solo maneja el bus I2S. Los códecs ES7210/ES8311 se configuran por I2C en Python.

#include "py/runtime.h"
#include "py/obj.h"
#include "driver/i2s_std.h"

// ---- Pinout fijo (LAFVIN ESP32-S3 AIChatBot) ----
#define ENOCH_MCLK_PIN   38
#define ENOCH_BCLK_PIN   14
#define ENOCH_WS_PIN     13
#define ENOCH_DIN_PIN    12   // mic (in)
#define ENOCH_DOUT_PIN   45   // bocina (out)

static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;
static bool enoch_i2s_ready = false;

// ---- limpieza interna del canal (usada por init() y deinit()) ----
static void enoch_i2s_cleanup(void) {
    if (enoch_i2s_ready) {
        i2s_channel_disable(tx_handle);
        i2s_channel_disable(rx_handle);
        i2s_del_channel(tx_handle);
        i2s_del_channel(rx_handle);
        tx_handle = NULL;
        rx_handle = NULL;
        enoch_i2s_ready = false;
    }
}

// ---- init(sample_rate=16000) ----
static mp_obj_t enoch_i2s_init(size_t n_args, const mp_obj_t *args) {
    uint32_t sample_rate = (n_args > 0) ? mp_obj_get_int(args[0]) : 16000;

    // Idempotente: si ya habia un canal abierto (de una llamada anterior
    // en la misma sesion), lo cerramos primero en vez de fallar con
    // RuntimeError. Asi init() se puede llamar las veces que haga falta
    // sin que quien lo usa tenga que acordarse de llamar deinit() antes.
    enoch_i2s_cleanup();

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle) != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("fallo i2s_new_channel"));
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = ENOCH_MCLK_PIN,
            .bclk = ENOCH_BCLK_PIN,
            .ws   = ENOCH_WS_PIN,
            .dout = ENOCH_DOUT_PIN,
            .din  = ENOCH_DIN_PIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };

    if (i2s_channel_init_std_mode(tx_handle, &std_cfg) != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("fallo init std tx"));
    }
    if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("fallo init std rx"));
    }

    i2s_channel_enable(tx_handle);
    i2s_channel_enable(rx_handle);
    enoch_i2s_ready = true;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(enoch_i2s_init_obj, 0, 1, enoch_i2s_init);

// ---- read(buf) -> bytes leidos ----
static mp_obj_t enoch_i2s_read(mp_obj_t buf_obj) {
    if (!enoch_i2s_ready) mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("no inicializado"));
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_obj, &bufinfo, MP_BUFFER_WRITE);
    size_t bytes_read = 0;
    if (i2s_channel_read(rx_handle, bufinfo.buf, bufinfo.len, &bytes_read, portMAX_DELAY) != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("fallo read"));
    }
    return mp_obj_new_int(bytes_read);
}
static MP_DEFINE_CONST_FUN_OBJ_1(enoch_i2s_read_obj, enoch_i2s_read);

// ---- write(buf) -> bytes escritos ----
static mp_obj_t enoch_i2s_write(mp_obj_t buf_obj) {
    if (!enoch_i2s_ready) mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("no inicializado"));
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_obj, &bufinfo, MP_BUFFER_READ);
    size_t bytes_written = 0;
    if (i2s_channel_write(tx_handle, bufinfo.buf, bufinfo.len, &bytes_written, portMAX_DELAY) != ESP_OK) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("fallo write"));
    }
    return mp_obj_new_int(bytes_written);
}
static MP_DEFINE_CONST_FUN_OBJ_1(enoch_i2s_write_obj, enoch_i2s_write);

// ---- deinit() ----
static mp_obj_t enoch_i2s_deinit(void) {
    enoch_i2s_cleanup();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(enoch_i2s_deinit_obj, enoch_i2s_deinit);

// ---- registro del modulo ----
static const mp_rom_map_elem_t enoch_i2s_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_enoch_i2s) },
    { MP_ROM_QSTR(MP_QSTR_init),     MP_ROM_PTR(&enoch_i2s_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),     MP_ROM_PTR(&enoch_i2s_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),    MP_ROM_PTR(&enoch_i2s_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit),   MP_ROM_PTR(&enoch_i2s_deinit_obj) },
};
static MP_DEFINE_CONST_DICT(enoch_i2s_module_globals, enoch_i2s_module_globals_table);

const mp_obj_module_t enoch_i2s_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&enoch_i2s_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_enoch_i2s, enoch_i2s_user_cmodule);
