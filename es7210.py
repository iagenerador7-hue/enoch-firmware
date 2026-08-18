"""
es7210.py — Driver I2C para el codec ADC ES7210 (microfono) del kit
LAFVIN ESP32-S3 AIChatBot.

Bus I2C1: SDA=GPIO1, SCL=GPIO2
Direccion I2C: 0x41

SEGUNDA VERSION (17-ago) — reescrita en base a la secuencia de
referencia verificada de ESPHome/esp-bsp para ES7210. La primera
version dejaba el chip respondiendo en I2C pero nunca terminaba
de habilitar el path analogico ni configuraba el reloj de muestreo,
por eso enoch_i2s.read() devolvia un patron fijo en vez de audio.

Asume MCLK = 256 * sample_rate generado por enoch_i2s.c (4.096MHz
para 16000Hz). Coincide con enoch_i2s.c tras el fix de
I2S_MCLK_MULTIPLE_384 -> I2S_MCLK_MULTIPLE_256.
"""

from machine import I2C, Pin
import time

ES7210_ADDR = 0x41

REG_RESET          = 0x00
REG_CLOCK_OFF      = 0x01
REG_MAINCLK        = 0x02
REG_LRCK_DIVH      = 0x04
REG_LRCK_DIVL      = 0x05
REG_POWER_DOWN     = 0x06
REG_OSR            = 0x07
REG_MODE_CONFIG    = 0x08
REG_TIME_CONTROL0  = 0x09
REG_TIME_CONTROL1  = 0x0A
REG_SDP_INTERFACE1 = 0x11
REG_SDP_INTERFACE2 = 0x12
REG_ADC34_HPF2     = 0x20
REG_ADC34_HPF1     = 0x21
REG_ADC12_HPF1     = 0x22
REG_ADC12_HPF2     = 0x23
REG_ANALOG         = 0x40
REG_MIC12_BIAS     = 0x41
REG_MIC34_BIAS     = 0x42
REG_MIC1_GAIN      = 0x43
REG_MIC2_GAIN      = 0x44
REG_MIC3_GAIN      = 0x45
REG_MIC4_GAIN      = 0x46
REG_MIC1_POWER     = 0x47
REG_MIC2_POWER     = 0x48
REG_MIC3_POWER     = 0x49
REG_MIC4_POWER     = 0x4A
REG_MIC12_POWER    = 0x4B
REG_MIC34_POWER    = 0x4C


class ES7210:
    def __init__(self, i2c_id=1, sda=1, scl=2, addr=ES7210_ADDR):
        self.i2c = I2C(i2c_id, sda=Pin(sda), scl=Pin(scl), freq=100000)
        self.addr = addr

    def _write(self, reg, val):
        self.i2c.writeto_mem(self.addr, reg, bytes([val]))

    def _read(self, reg):
        return self.i2c.readfrom_mem(self.addr, reg, 1)[0]

    def _update_bits(self, reg, mask, val):
        cur = self._read(reg)
        new = (cur & (~mask & 0xFF)) | (mask & val)
        self._write(reg, new)

    def scan_ok(self):
        return self.addr in self.i2c.scan()

    def init(self, mic_gain=0x0C, sample_rate=16000):
        if not self.scan_ok():
            raise OSError("ES7210 no responde en 0x%02X" % self.addr)

        self._write(REG_RESET, 0xFF)
        self._write(REG_RESET, 0x32)
        self._write(REG_CLOCK_OFF, 0x3F)

        self._write(REG_TIME_CONTROL0, 0x30)
        self._write(REG_TIME_CONTROL1, 0x30)

        self._write(REG_ADC12_HPF2, 0x2A)
        self._write(REG_ADC12_HPF1, 0x0A)
        self._write(REG_ADC34_HPF2, 0x0A)
        self._write(REG_ADC34_HPF1, 0x2A)

        self._update_bits(REG_MODE_CONFIG, 0x01, 0x00)

        self._write(REG_ANALOG, 0xC3)

        self._write(REG_MIC12_BIAS, 0x70)
        self._write(REG_MIC34_BIAS, 0x70)

        self._write(REG_SDP_INTERFACE1, 0x60)
        self._write(REG_SDP_INTERFACE2, 0x00)

        if sample_rate == 16000:
            adc_div, dll, doubler, osr = 0x01, 0x01, 0x01, 0x20
            lrck_h, lrck_l = 0x01, 0x00
        else:
            raise ValueError("Solo 16000Hz tiene coeficientes cargados")

        regv = adc_div | (doubler << 6) | (dll << 7)
        self._write(REG_MAINCLK, regv)
        self._write(REG_OSR, osr)
        self._write(REG_LRCK_DIVH, lrck_h)
        self._write(REG_LRCK_DIVL, lrck_l)

        self._write(REG_MIC1_GAIN, mic_gain)
        self._write(REG_MIC2_GAIN, mic_gain)
        self._write(REG_MIC3_GAIN, mic_gain)
        self._write(REG_MIC4_GAIN, mic_gain)

        self._write(REG_MIC1_POWER, 0x08)
        self._write(REG_MIC2_POWER, 0x08)
        self._write(REG_MIC3_POWER, 0x08)
        self._write(REG_MIC4_POWER, 0x08)

        self._write(REG_POWER_DOWN, 0x04)

        self._write(REG_MIC12_POWER, 0x0F)
        self._write(REG_MIC34_POWER, 0x0F)

        self._write(REG_CLOCK_OFF, 0x00)
        self._write(REG_RESET, 0x71)
        self._write(REG_RESET, 0x41)

        time.sleep_ms(50)

    def deinit(self):
        self._write(REG_RESET, 0x00)
