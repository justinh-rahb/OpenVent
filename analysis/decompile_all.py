import sys
import os
from ghidra.util.task import TaskMonitor
from ghidra.app.decompiler import DecompInterface
from ghidra.app.plugin.core.analysis import AutoAnalysisManager
import struct
from ghidra.program.model.symbol import SourceType

def setup_memory(bin_path):
    with open(bin_path, "rb") as f:
        data = f.read()
    magic, segments, spi_mode, spi_speed_size, entry = struct.unpack_from("<BBBBI", data, 0)
    mem = currentProgram.getMemory()
    space = currentProgram.getAddressFactory().getDefaultAddressSpace()
    for block in mem.getBlocks():
        mem.removeBlock(block, TaskMonitor.DUMMY)
    offset = 24
    for i in range(segments):
        load_addr, size = struct.unpack_from("<II", data, offset)
        offset += 8
        segment_data = data[offset:offset+size]
        offset += size
        is_exec = (0x40000000 <= load_addr < 0x40200000)
        is_read = True
        is_write = (0x3F400000 <= load_addr < 0x40000000) or (load_addr >= 0x3FF00000 and load_addr < 0x40000000)
        addr = space.getAddress(load_addr)
        block = mem.createInitializedBlock("seg_{}".format(i), addr, size, 0, TaskMonitor.DUMMY, False)
        block.setExecute(is_exec)
        block.setRead(is_read)
        block.setWrite(is_write)
        mem.setBytes(addr, segment_data)
    entry_addr = space.getAddress(entry)
    currentProgram.getSymbolTable().createLabel(entry_addr, "entry", SourceType.USER_DEFINED)
    currentProgram.getSymbolTable().addExternalEntryPoint(entry_addr)

def analyze():
    bin_path = os.environ.get("ESP32_BIN")
    setup_memory(bin_path)
    
    mgr = AutoAnalysisManager.getAnalysisManager(currentProgram)
    mgr.initializeOptions()
    mgr.reAnalyzeAll(None)
    mgr.startAnalysis(TaskMonitor.DUMMY)
    
    ifc = DecompInterface()
    ifc.openProgram(currentProgram)
    
    out_path = os.environ.get("DECOMP_OUT")
    with open(out_path, "w") as f:
        funcs = currentProgram.getFunctionManager().getFunctions(True)
        for func in funcs:
            res = ifc.decompileFunction(func, 30, TaskMonitor.DUMMY)
            if res.decompileCompleted():
                f.write(res.getDecompiledFunction().getC() + "\n\n")

analyze()
