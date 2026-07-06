import re

with open("irom.asm", "r") as f:
    lines = f.readlines()

def parse_register(instr):
    # Extracts the source register from something like "movi a8, 12" -> "a8"
    parts = instr.split(',')
    if len(parts) > 0:
        return parts[0].split()[-1]
    return None

def find_struct_init():
    for i in range(len(lines) - 20):
        # We look for a sequence of s32i instructions writing to the same base register
        # at offsets 0, 4, 8, 12, 16, 20, 24
        writes = {}
        for j in range(20):
            line = lines[i+j].strip()
            if 's32i' in line or 's32i.n' in line:
                # e.g., s32i a8, a1, 0
                parts = line.split('\t')[-1].split(',')
                if len(parts) == 3:
                    val_reg = parts[0].strip().split()[-1]
                    base_reg = parts[1].strip()
                    offset = parts[2].strip()
                    if base_reg not in writes:
                        writes[base_reg] = set()
                    writes[base_reg].add(offset)
        
        # Check if we found writes to offset 0, 4, 8, 12, 16, 20, 24 for any base register
        for base_reg, offsets in writes.items():
            if '0' in offsets and '4' in offsets and '8' in offsets and '16' in offsets:
                print(f"Found potential ledc_channel_config struct init at line {i} (base {base_reg}):")
                for j in range(i-5, i+25):
                    print("  " + lines[j].strip())
                print("---------------------------------")
                pass

find_struct_init()
