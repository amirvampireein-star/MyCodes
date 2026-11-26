//@category SiS
import java.io.*; import java.util.*;
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
public class DecompileFunc extends GhidraScript {
  public void run() throws Exception {
    String[] args=getScriptArgs();
    File out=new File(args[0]);
    FunctionManager fm=currentProgram.getFunctionManager();
    Set<Function> targets=new LinkedHashSet<>();
    DecompInterface di=new DecompInterface(); di.openProgram(currentProgram);
    try(PrintWriter pw=new PrintWriter(out)){
      for(int i=1;i<args.length;i++){
        String a=args[i];
        try{ Address ad=toAddr(a); Function f=fm.getFunctionContaining(ad); if(f!=null) targets.add(f); }
        catch(Exception e){ for(Function f:fm.getFunctions(true)) if(f.getName().matches(a)) targets.add(f); }
      }
      for(Function f:targets){
        pw.println("/* ==== " + f.getName()+" @ "+f.getEntryPoint()+" size="+f.getBody().getNumAddresses()+" ==== */");
        DecompileResults r=di.decompileFunction(f,120,monitor);
        pw.println(r!=null && r.decompileCompleted()?r.getDecompiledFunction().getC():"/* fail */");
      }
    } finally{ di.dispose(); }
  }
}
