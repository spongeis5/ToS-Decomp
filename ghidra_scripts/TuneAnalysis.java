// Configure analysis, and RECORD what was configured.
//
// A result is a fact plus the conditions it was measured under.  An analysis
// run whose analyzer set is nowhere on the record produces cross-references
// that cannot later be trusted or reproduced, so this prints every option and
// its value before analysis starts.
//
// "Decompiler Parameter ID" is disabled: it decompiles every function to infer
// signatures, which over 21,238 functions costs hours and is not needed for
// the call graph and cross-references this run exists to build.  It can be run
// later on its own.
//
//@category ToS

import ghidra.app.script.GhidraScript;

import java.util.Map;
import java.util.TreeMap;

public class TuneAnalysis extends GhidraScript {

    @Override
    public void run() throws Exception {
        // "Non-Returning Functions - Discovered" wrongly decided that MSVC's
        // __savegprlr / __restgprlr register helpers do not return.  Those
        // routines have MANY entry points -- 10,856 functions begin
        // `mflr r12 ; bl 828A75xx` at a different offset into one body -- so
        // every caller was truncated at its second instruction.  Measured:
        // 25 helper entry points flagged no-return, 9,647 of 21,238 functions
        // with a body shorter than their .pdata extent, median coverage 11%.
        //
        // "- Known" stays ON: it uses a name list (exit, abort) and is right.
        String[] off = {
            "Decompiler Parameter ID",
            "Non-Returning Functions - Discovered",
        };

        println("TUNE-BEGIN");
        for (String name : off) {
            setAnalysisOption(currentProgram, name, "false");
            println("  disabled: " + name);
        }

        Map<String, String> opts =
                new TreeMap<>(getCurrentAnalysisOptionsAndValues(currentProgram));
        println("  analysis options in effect: " + opts.size());
        for (Map.Entry<String, String> e : opts.entrySet()) {
            // Only the top-level enable flags; the sub-options are noise here.
            if (!e.getKey().contains(".")) {
                println(String.format("    %-52s %s", e.getKey(), e.getValue()));
            }
        }
        println(String.format("  functions before analysis: %d",
                currentProgram.getFunctionManager().getFunctionCount()));
        println("TUNE-END");
    }
}
