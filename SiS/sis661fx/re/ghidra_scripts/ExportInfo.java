//@category SiS
import java.io.*; import java.util.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
public class ExportInfo extends GhidraScript {
  private static final long[] W = {0x1039,0x6330,0x6325,0x6351,0x8200,0x8210,0x8214,0x8218,0x821C,0x8220,0x8224,0x8228,0x823C,0x8240,0x8300,0x85C0,0x85CC,0x8A2C,0x8B60};
  public void run() throws Exception {
    String outDir = getScriptArgs().length>0?getScriptArgs()[0]:".";
    new File(outDir).mkdirs();
    String base = currentProgram.getName().replaceAll("[^A-Za-z0-9_.-]","_");
    try(PrintWriter f=new PrintWriter(new File(outDir,base+".functions.txt"));
        PrintWriter s=new PrintWriter(new File(outDir,base+".strings.txt"));
        PrintWriter c=new PrintWriter(new File(outDir,base+".consthits.txt"))) {
      for(Function fn: currentProgram.getFunctionManager().getFunctions(true))
        f.println(String.format("%s\t%s\t%d", fn.getEntryPoint(), fn.getName(), fn.getBody().getNumAddresses()));
      for(Data d: currentProgram.getListing().getDefinedData(true)) {
        Object v=d.getValue(); if(v instanceof String && ((String)v).length()>=3) s.println(d.getAddress()+"\t"+((String)v).replace("\n","\\n"));
      }
      Set<Long> want=new HashSet<>(); for(long x:W) want.add(x&0xFFFFFFFFL);
      InstructionIterator it=currentProgram.getListing().getInstructions(true);
      while(it.hasNext()){ Instruction ins=it.next(); for(int i=0;i<ins.getNumOperands();i++) for(Object o:ins.getOpObjects(i)) if(o instanceof Scalar){ long val=((Scalar)o).getUnsignedValue()&0xFFFFFFFFL; if(want.contains(val)) c.println(String.format("%s\t%s\t0x%x",ins.getAddress(),ins, val)); }}
    }
    println("ok");
  }
}
