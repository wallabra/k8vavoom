import arsd.image;
import iv.vfs.io;


PNG *loadPng (string fname) {
  import std.file;
  return readPng(cast(ubyte[])read(fname));
}


Chunk clone (Chunk *cc) {
  Chunk res;
  res.size = cc.size;
  res.type[] = cc.type[];
  res.payload = cc.payload.dup;
  res.checksum = cc.checksum;
  return res;
}

void main (string[] args) {
  assert(args.length == 3);
  auto png = loadPng(args[1]);
  Chunk grab = png.getChunk("grAb").clone;
  if (grab.size != 8) assert(0);
  {
    int ofsx = grab.payload[3]|(grab.payload[2]<<8)|(grab.payload[1]<<16)|(grab.payload[0]<<24);
    int ofsy = grab.payload[4+3]|(grab.payload[4+2]<<8)|(grab.payload[4+1]<<16)|(grab.payload[4+0]<<24);
    writeln("  offset: (", ofsx, ", ", ofsy, ")");
  }
  auto destpng = loadPng(args[2]);
  foreach (ref const cc; destpng.chunks) {
    if (cast(const(char)[])cc.type[] == "grAb") {
      //writeln("  ", cc.payload);
      int ofsx = cc.payload[3]|(cc.payload[2]<<8)|(cc.payload[1]<<16)|(cc.payload[0]<<24);
      int ofsy = cc.payload[4+3]|(cc.payload[4+2]<<8)|(cc.payload[4+1]<<16)|(cc.payload[4+0]<<24);
      writeln("  offset: (", ofsx, ", ", ofsy, ")");
      assert(0);
    }
  }
  destpng.insertChunk(&grab, true);
  destpng.length = 0;
  import std.file;
  std.file.write(args[2], writePng(destpng));
}
