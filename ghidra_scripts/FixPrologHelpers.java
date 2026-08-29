// Diagnose and repair the MSVC PowerPC register save/restore helpers.
//
// 828A7590..828A75DC is __savegprlr: one routine with MANY entry points, one
// per register count, falling through to a single blr.  10,856 functions begin
//     mflr r12
//     bl   828A75CC        <- a DIFFERENT offset into the same routine
// so 10,856 calls land in the middle of an existing function.
//
// If anything marks that helper as non-returning, every one of those callers
// is truncated at its second instruction -- which is what happened: 9,647
// functions had a body shorter than their .pdata extent, median coverage 11%.
//
// This script reports the no-return flag on every function in the helper band
// BEFORE changing anything, clears it, and then re-disassembles every .pdata
// function body.  The before/after counts are printed so the repair is a
// measurement rather than a hope.
//
//@category ToS

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

public class FixPrologHelpers extends GhidraScript {

    private static final long BAND_LO = 0x828A7000L;
    private static final long BAND_HI = 0x828AA000L;

    @Override
    public void run() throws Exception {
        println("FIXHELPERS-BEGIN");

        // --- report, then clear, the no-return flag in the helper band ---
        int inBand = 0, wasNoReturn = 0;
        FunctionIterator fi = currentProgram.getFunctionManager().getFunctions(true);
        List<Function> band = new ArrayList<>();
        while (fi.hasNext()) {
            Function f = fi.next();
            long a = f.getEntryPoint().getOffset();
            if (a >= BAND_LO && a < BAND_HI) {
                inBand++;
                band.add(f);
                if (f.hasNoReturn()) {
                    wasNoReturn++;
                    println(String.format("  no-return: %s at %08X",
                            f.getName(), a));
                }
            }
        }
        println(String.format("helper band %08X..%08X: %d function(s), "
                + "%d marked NO-RETURN before the fix",
                BAND_LO, BAND_HI, inBand, wasNoReturn));

        for (Function f : band) {
            if (f.hasNoReturn()) {
                f.setNoReturn(false);
            }
        }

        // Also clear it anywhere else -- a wrongly discovered no-return
        // anywhere truncates its callers the same way.
        int clearedElsewhere = 0;
        fi = currentProgram.getFunctionManager().getFunctions(true);
        while (fi.hasNext()) {
            Function f = fi.next();
            long a = f.getEntryPoint().getOffset();
            if (a >= BAND_LO && a < BAND_HI) {
                continue;
            }
            if (f.hasNoReturn()) {
                f.setNoReturn(false);
                clearedElsewhere++;
            }
        }
        println(String.format("cleared the no-return flag on %d further "
                + "function(s) outside the band", clearedElsewhere));

        // --- coverage before ---
        long before = coveredBytes();
        println(String.format("function-body bytes before: %d", before));

        // --- re-disassemble every .pdata body ---
        String path = "C:/Users/redacted/Downloads/ToS-Decomp/build/functions.txt";
        BufferedReader r = new BufferedReader(new FileReader(path));
        String line;
        int rows = 0, redone = 0;
        while ((line = r.readLine()) != null) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) {
                continue;
            }
            String[] f = line.split("\\s+");
            rows++;
            long va = Long.parseLong(f[0], 16);
            int size = Integer.parseInt(f[1]);
            Address a = toAddr(va);
            Function fn = getFunctionAt(a);
            if (fn == null) {
                continue;
            }
            if (fn.getBody().getNumAddresses() < size) {
                disassemble(a);
                redone++;
            }
            if (monitor.isCancelled()) {
                println("CANCELLED -- partial, NOT a measurement");
                break;
            }
            if ((redone % 2000) == 0 && redone > 0) {
                println(String.format("  ... re-disassembled %d", redone));
            }
        }
        r.close();

        long after = coveredBytes();
        println(String.format("rows %d, re-disassembled %d", rows, redone));
        println(String.format("function-body bytes after:  %d  (%+d)",
                after, after - before));
        println("FIXHELPERS-END");
    }

    private long coveredBytes() {
        long n = 0;
        FunctionIterator fi = currentProgram.getFunctionManager().getFunctions(true);
        while (fi.hasNext()) {
            AddressSetView b = fi.next().getBody();
            n += b.getNumAddresses();
        }
        return n;
    }
}
