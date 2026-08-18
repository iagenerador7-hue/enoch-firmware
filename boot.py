import network
import time

wlan = network.WLAN(network.STA_IF)
wlan.active(True)

if not wlan.isconnected():
    wlan.connect('POCO M5s', '123456789')
    timeout = 10
    while not wlan.isconnected() and timeout > 0:
        time.sleep(1)
        timeout -= 1

if wlan.isconnected():
    print('Wi-Fi conectado:', wlan.ifconfig()[0])
else:
    print('No se pudo conectar al Wi-Fi')

import webrepl
webrepl.start()
