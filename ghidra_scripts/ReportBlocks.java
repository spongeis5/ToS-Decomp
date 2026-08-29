// Report what actually landed in memory, so the loader's "Import succeeded"
// can be checked against an independent read of the same file rather than
// believed.
//
// Prints, per block: name, range, permissions, and an FNV-1a hash of the
// first 256 initialised bytes.  tools/verify_ghidra.py computes the same
// hash from the file and asserts agreement.
//
//@category ToS

import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class ReportBlocks extends GhidraScript {

    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        println("BLOCKS-BEGIN");
        println(String.format("image_base %s",
                currentProgram.getImageBase().toString()));
        println(String.format("language %s",
                currentProgram.getLanguageID().getIdAsString()));
        println(String.format("function_count %d",
                currentProgram.getFunctionManager().getFunctionCount()));

        for (MemoryBlock b : mem.getBlocks()) {
            long size = b.getSize();
            String perms = (b.isRead() ? "r" : "-")
                         + (b.isWrite() ? "w" : "-")
                         + (b.isExecute() ? "x" : "-");
            String hash = "uninitialized";
            if (b.isInitialized()) {
                int n = (int) Math.min(256, size);
                byte[] buf = new byte[n];
                b.getBytes(b.getStart(), buf, 0, n);
                long h = 0xcbf29ce484222325L;
                for (byte v : buf) {
                    h ^= (v & 0xff);
                    h *= 0x100000001b3L;
                }
                hash = String.format("%016x", h);
            }
            println(String.format("BLOCK %-10s %s %s %08x %s %s",
                    b.getName(),
                    b.getStart().toString(),
                    b.getEnd().toString(),
                    size,
                    perms,
                    hash));
        }
        println("BLOCKS-END");
    }
}
