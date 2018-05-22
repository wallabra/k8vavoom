import arsd.image;
import iv.vfs.io;


PNG *loadPng (string fname) {
  import std.file;
  return readPng(cast(ubyte[])read(fname));
}


void main (string[] args) {
  foreach (string fname; args[1..$]) {
    writeln("=== ", fname, " ===");
    auto png = loadPng(fname);
    foreach (ref const cc; png.chunks) {
      writeln("  chunk '", cast(const(char)[])cc.type[], "': size: ", cc.size);
      if (cast(const(char)[])cc.type[] == "grAb") {
        //writeln("  ", cc.payload);
        int ofsx = cc.payload[3]|(cc.payload[2]<<8)|(cc.payload[1]<<16)|(cc.payload[0]<<24);
        int ofsy = cc.payload[4+3]|(cc.payload[4+2]<<8)|(cc.payload[4+1]<<16)|(cc.payload[4+0]<<24);
        writeln("    offset: (", ofsx, ", ", ofsy, ")");
      }
    }
  }
}
