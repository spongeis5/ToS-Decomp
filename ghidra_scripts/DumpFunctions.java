// Export every function Ghidra currently holds, so the inventory can be
// compared as a SET against .pdata rather than as a pair of counts.
//
// Two counts that disagree cannot say which population differs; two sets
// can.
//
//@category ToS

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.PrintWriter;

public class DumpFunctions extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String path = args.length > 0 ? args[0]
                : "build/ghidra_functions.txt";

        PrintWriter w = new PrintWriter(path);
        w.println("# address size name thunk external");
        int n = 0, thunks = 0;
        FunctionIterator it = currentProgram.getFunctionManager()
                .getFunctions(true);
        while (it.hasNext()) {
            Function f = it.next();
            boolean isThunk = f.isThunk();
            if (isThunk) {
                thunks++;
            }
            w.println(String.format("%08X %8d %s %s %s",
                    f.getEntryPoint().getOffset(),
                    f.getBody().getNumAddresses(),
                    f.getName(),
                    isThunk ? "thunk" : "-",
                    f.isExternal() ? "external" : "-"));
            n++;
        }
        w.close();
        println("DUMP-BEGIN");
        println(String.format("wrote %d function(s) to %s", n, path));
        println(String.format("  of which thunks: %d", thunks));
        println(String.format("  manager count:   %d",
                currentProgram.getFunctionManager().getFunctionCount()));
        println("DUMP-END");
    }
}
