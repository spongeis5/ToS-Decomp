// Report what analysis produced, with denominators.
//
// Run as a postScript so the numbers describe the analysed program.  Every
// count states the population it is drawn from; a bare "12345 references" says
// nothing about coverage.
//
//@category ToS

import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.app.script.GhidraScript;

import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Set;

public class ReportAnalysis extends GhidraScript {

    @Override
    public void run() throws Exception {
        String out = "build/callgraph.txt";
        String[] args = getScriptArgs();
        if (args.length > 0) {
            out = args[0];
        }

        println("ANALYSIS-BEGIN");
        int total = currentProgram.getFunctionManager().getFunctionCount();
        println(String.format("functions            %d", total));

        long instr = 0;
        InstructionIterator ii = currentProgram.getListing().getInstructions(true);
        while (ii.hasNext()) {
            ii.next();
            instr++;
        }
        println(String.format("instructions         %d", instr));

        ReferenceManager rm = currentProgram.getReferenceManager();

        int withCallers = 0, noCallers = 0, leaf = 0;
        long callEdges = 0;
        PrintWriter w = new PrintWriter(out);
        w.println("# caller callee");

        FunctionIterator fi = currentProgram.getFunctionManager().getFunctions(true);
        while (fi.hasNext()) {
            Function f = fi.next();
            if (monitor.isCancelled()) {
                println("CANCELLED -- counts are partial and are NOT a measurement");
                break;
            }
            Set<Function> callees = f.getCalledFunctions(monitor);
            Set<Function> callers = f.getCallingFunctions(monitor);
            if (callers.isEmpty()) {
                noCallers++;
            } else {
                withCallers++;
            }
            if (callees.isEmpty()) {
                leaf++;
            }
            for (Function c : callees) {
                w.println(String.format("%08X %08X",
                        f.getEntryPoint().getOffset(),
                        c.getEntryPoint().getOffset()));
                callEdges++;
            }
        }
        w.close();

        println(String.format("call edges           %d", callEdges));
        println(String.format("  functions with >=1 caller  %d of %d", withCallers, total));
        println(String.format("  functions with NO caller   %d of %d", noCallers, total));
        println(String.format("  LEAF functions (call none) %d of %d", leaf, total));
        println("wrote " + out);
        println("ANALYSIS-END");
    }
}
