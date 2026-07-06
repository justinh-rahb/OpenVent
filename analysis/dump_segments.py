import sys

with open("../vendor/Panda-Vent/Firmware/panda_vent_v1.0.0.bin", "rb") as f:
    data = f.read()

# Segment 0: DROM
with open("drom.bin", "wb") as f:
    f.write(data[0x00000018:0x00000018+0x3a3c8])

# Segment 4: IROM
with open("irom.bin", "wb") as f:
    f.write(data[0x00040018:0x00040018+0xc0304])

print("Segments dumped.")
