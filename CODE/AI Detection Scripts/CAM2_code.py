import sensor, time, pyb

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.skip_frames(time=2000)

usb = pyb.USB_VCP()

MAGIC = b'FRAME'
CALLSIGN = b'CAM2'  # Change to b'CAM2' for the second camera

while True:
    img = sensor.snapshot()
    jpg = img.compress(quality=50)
    size = len(jpg)

    usb.write(MAGIC)
    usb.write(CALLSIGN)        # 4-byte callsign after magic
    usb.write(size.to_bytes(4, 'big'))
    usb.write(jpg)
    time.sleep(0.1)
