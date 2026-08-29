// Create a function at every address the compiler's own .pdata table
// declares, before the analyser is allowed to guess at any.
//
// Functions from .pdata are named sub_XXXXXXXX; anything the analyser finds
// later keeps Ghidra's FUN_ prefix.  The provenance is therefore visible in
// the name, so "how many functions do we actually have evidence for" stays
// answerable after analysis has run.
//
// Every count states its denominator, and the arms are asserted to sum to
// the number of rows read.
//
//@category ToS

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.FileReader;

public class ApplyPdata extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String path = args.length > 0 ? args[0]
                : "build/functions.txt";

        int rows = 0, created = 0, existed = 0, notInMemory = 0;
        int disasmFailed = 0, createFailed = 0;

        println("PDATA-BEGIN");
        println("reading " + path);

        BufferedReader r = new BufferedReader(new FileReader(path));
        String line;
        while ((line = r.readLine()) != null) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            String[] f = line.split("\\s+");
            rows++;

            long va;
            try {
                va = Long.parseLong(f[0], 16);
            } catch (NumberFormatException e) {
                println("UNPARSED ROW: " + line);
                continue;
            }
            Address a = toAddr(va);

            MemoryBlock b = currentProgram.getMemory().getBlock(a);
            if (b == null || !b.isInitialized()) {
                notInMemory++;
                continue;
            }

            Function existing = getFunctionAt(a);
            if (existing != null) {
                existed++;
                continue;
            }

            if (getInstructionAt(a) == null) {
                disassemble(a);
                if (getInstructionAt(a) == null) {
                    disasmFailed++;
                    continue;
                }
            }

            Function fn = createFunction(a, String.format("sub_%08X", va));
            if (fn == null) {
                createFailed++;
                continue;
            }
            created++;

            if ((created % 2000) == 0) {
                println(String.format("  ... %d created of %d row(s) read so far",
                        created, rows));
            }
            if (monitor.isCancelled()) {
                println("CANCELLED -- counts below are partial and are NOT a "
                        + "measurement of the image");
                break;
            }
        }
        r.close();

        int accounted = created + existed + notInMemory + disasmFailed + createFailed;
        println("");
        println(String.format("rows read           %d", rows));
        println(String.format("  created           %d", created));
        println(String.format("  already existed   %d", existed));
        println(String.format("  not in memory     %d", notInMemory));
        println(String.format("  disassembly failed %d", disasmFailed));
        println(String.format("  createFunction failed %d", createFailed));
        println(String.format("  ---- accounted    %d of %d %s",
                accounted, rows, accounted == rows ? "(sum agrees)" : "*** SUM DISAGREES ***"));
        println(String.format("program function count now %d",
                currentProgram.getFunctionManager().getFunctionCount()));
        println("PDATA-END");

        if (accounted != rows) {
            throw new Exception("arm counts do not sum to rows read; the "
                    + "numbers above describe an unknown population");
        }
    }
}
