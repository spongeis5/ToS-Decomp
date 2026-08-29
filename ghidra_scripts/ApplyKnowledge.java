// Apply everything this project has established, then repair the VMX128
// truncation, before analysis runs.
//
//   build/imports.txt          207 kernel/XAM thunks, named from the XDK's own
//                              import libraries (207 of 207 matched)
//   build/rtti_functions.txt   Havok virtual functions recovered from MSVC RTTI
//   build/profiler_names.txt   engine profiler scope names
//
// Then: Ghidra's PowerPC module cannot decode Xenon VMX128, so disassembly
// stops at the first such word and 1,330 functions were truncated.  VMX128
// instructions are all 4 bytes and none of them alters control flow, so a walk
// over each .pdata extent can resume disassembly at the word AFTER any gap.
//
// Every arm is counted with a denominator; a rename that fails is reported
// rather than swallowed.
//
//@category ToS

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.SourceType;

import java.io.BufferedReader;
import java.io.FileReader;

public class ApplyKnowledge extends GhidraScript {

    private static final String ROOT = "";

    @Override
    public void run() throws Exception {
        println("KNOWLEDGE-BEGIN");
        nameImports();
        nameFrom(ROOT + "build/rtti_functions.txt", "rtti", 1);
        nameFrom(ROOT + "build/profiler_names.txt", "prof", 2);
        fixVmxGaps();
        println("KNOWLEDGE-END");
    }

    // ---- 207 import thunks -------------------------------------------------

    private void nameImports() throws Exception {
        String path = ROOT + "build/imports.txt";
        BufferedReader r = new BufferedReader(new FileReader(path));
        String line;
        int rows = 0, named = 0, created = 0, failed = 0;
        while ((line = r.readLine()) != null) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] f = line.split("\\s+");
            if (f.length < 4) continue;
            rows++;
            Address a = toAddr(Long.parseLong(f[0], 16));
            String sym = f[3];
            if (getInstructionAt(a) == null) {
                disassemble(a);
            }
            Function fn = getFunctionAt(a);
            if (fn == null) {
                fn = createFunction(a, sym);
                if (fn != null) created++;
            }
            if (fn != null) {
                try {
                    fn.setName(sym, SourceType.IMPORTED);
                    named++;
                } catch (Exception e) {
                    failed++;
                }
            } else {
                try {
                    createLabel(a, sym, true, SourceType.IMPORTED);
                    named++;
                } catch (Exception e) {
                    failed++;
                }
            }
        }
        r.close();
        println(String.format("imports: %d row(s), %d named, %d function(s) created, %d failed",
                rows, named, created, failed));
    }

    // ---- names from a "<addr> <text...>" file -------------------------------

    private void nameFrom(String path, String prefix, int field) throws Exception {
        java.io.File file = new java.io.File(path);
        if (!file.exists()) {
            println(prefix + ": " + path + " absent, skipped");
            return;
        }
        BufferedReader r = new BufferedReader(new FileReader(file));
        String line;
        int rows = 0, named = 0, noFunc = 0, failed = 0;
        while ((line = r.readLine()) != null) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] f = line.split("\\s+", field + 1);
            if (f.length <= field) continue;
            rows++;
            Address a = toAddr(Long.parseLong(f[0], 16));
            Function fn = getFunctionAt(a);
            if (fn == null) { noFunc++; continue; }
            String raw = f[field].split("\\|")[0].trim();
            String sym = raw.replaceAll("[^A-Za-z0-9_]", "_");
            if (sym.isEmpty()) continue;
            if (sym.length() > 60) sym = sym.substring(0, 60);
            try {
                fn.setName(prefix + "_" + sym + "_" + f[0], SourceType.ANALYSIS);
                named++;
            } catch (Exception e) {
                failed++;
            }
        }
        r.close();
        println(String.format("%s: %d row(s), %d named, %d had no function, %d failed",
                prefix, rows, named, noFunc, failed));
    }

    // ---- resume disassembly past VMX128 words ------------------------------

    private void fixVmxGaps() throws Exception {
        String path = ROOT + "build/functions.txt";
        BufferedReader r = new BufferedReader(new FileReader(path));
        String line;
        int rows = 0, shortFns = 0, resumes = 0;
        long before = 0, after = 0;
        while ((line = r.readLine()) != null) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] f = line.split("\\s+");
            rows++;
            long va = Long.parseLong(f[0], 16);
            int size = Integer.parseInt(f[1]);
            Function fn = getFunctionAt(toAddr(va));
            if (fn == null) continue;
            long body = fn.getBody().getNumAddresses();
            before += body;
            if (body >= size) { after += body; continue; }
            shortFns++;
            // Walk the declared extent; wherever there is no instruction,
            // try to disassemble there. VMX128 words never branch, so the
            // instruction after a gap is genuinely reachable.
            for (int off = 0; off < size; off += 4) {
                Address a = toAddr(va + off);
                if (getInstructionAt(a) == null) {
                    disassemble(a);
                    if (getInstructionAt(a) != null) resumes++;
                }
            }
            after += getFunctionAt(toAddr(va)).getBody().getNumAddresses();
            if (monitor.isCancelled()) {
                println("CANCELLED -- partial, NOT a measurement");
                break;
            }
        }
        r.close();
        println(String.format("vmx gaps: %d row(s), %d short function(s), "
                + "%d resume point(s) disassembled", rows, shortFns, resumes));
        // DO NOT read the next line as "the repair did nothing".
        //
        // Ghidra computes a function's body at CREATION time and does not
        // recompute it when later disassembly fills a hole, so this delta is
        // ~0 even when the repair works. It reported +0 while the same run
        // took whole-program instructions from 1,899,447 to 2,060,734.
        //
        // The honest instrument is the program-wide instruction count in
        // ReportAnalysis, not function-body extent. This has produced a false
        // +0 twice in this project; the number is printed for continuity, not
        // as evidence.
        println(String.format("  body bytes %d -> %d  (%+d)  <- NOT a measure "
                + "of the repair; see ReportAnalysis's instruction count",
                before, after, after - before));
        println("  resume points disassembled is the count that means something "
                + "here.");
    }
}
