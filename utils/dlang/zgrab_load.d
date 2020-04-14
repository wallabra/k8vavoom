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
  foreach (string fname; args[1..$]) {
    ubyte[] grabpay;
    try {
      auto fo = VFile(fname~".grAb");
      if (fo.size > 1024) throw new Exception("fuck");
      grabpay = new ubyte[cast(uint)fo.size];
      fo.rawReadExact(grabpay);
    } catch (Exception e) {
      grabpay.length = 0; // just in case
    }
    if (!grabpay.length) continue;
    // load png
    auto destpng = loadPng(fname);
    // replace chunk
    Chunk *gc = Chunk.create("grAb", grabpay);
    destpng.insertChunk(gc, true); // replace
    destpng.length = 0;
    // write png
    {
      import std.file;
      std.file.write(fname, writePng(destpng));
      writeln(fname, " ... OK");
    }
  }
}
