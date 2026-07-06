import struct
import re

with open("drom.bin", "rb") as f:
    drom_data = f.read()

def find_string_addr(target):
    idx = drom_data.find(target.encode())
    if idx == -1:
        return None
    return 0x3f400020 + idx

targets = [
    "ledc_channel_config(&fwd_chan)",
    "ledc_channel_config(&rev_chan)",
    "motor_pwm_init",
    "rmt_new_led_strip_encoder"
]

with open("irom.asm", "r") as f:
    asm_lines = f.readlines()

for t in targets:
    addr = find_string_addr(t)
    if not addr:
        print(f"Target '{t}' not found in DROM.")
        continue
    
    print(f"Target '{t}' found at DROM address: 0x{addr:X}")
    
    # Search for this address in irom.asm
    # In Xtensa, references are typically loaded via literal pool: l32r aX, <address>
    # Since we disassembled raw binary, objdump won't resolve symbols. It will just show the hex value in literal pool.
    # We need to find where the value 0x{addr:X} is stored in irom.asm, and then find where that literal is loaded!
    
    # Let's just find the hex value in the literal pools first.
    hex_val = f"{addr:08x}"
    # Sometimes it's little endian if we search raw bytes, but objdump shows words as hex.
    
    # To be simpler, let's just search the whole irom.bin for the 32-bit address (little endian)
    addr_bytes = struct.pack("<I", addr)
    with open("irom.bin", "rb") as firom:
        irom_data = firom.read()
        
    idx = irom_data.find(addr_bytes)
    while idx != -1:
        lit_addr = 0x400d0020 + idx
        print(f"  Found reference in IROM literal pool at 0x{lit_addr:X}")
        
        # Now search asm_lines for the instruction that references this literal!
        # It usually looks like: `l32r aX, 0x{lit_addr:x}`
        
        lit_hex = f"{lit_addr:x}"
        for i, line in enumerate(asm_lines):
            if f"l32r" in line and lit_hex in line:
                print(f"    Referenced at instruction: {line.strip()}")
                # Print some context before this instruction
                start = max(0, i-30)
                end = min(len(asm_lines), i+10)
                print("    --- Context ---")
                for j in range(start, end):
                    print("    " + asm_lines[j].strip())
                print("    ---------------")
        
        idx = irom_data.find(addr_bytes, idx + 1)

