import arsd.image;
import iv.vfs.io;


PNG *loadPng (string fname) {
  import std.file;
  return readPng(cast(ubyte[])read(fname));
}


void main (string[] args) {
  foreach (string fname; args[1..$]) {
    auto png = loadPng(fname);
    foreach (ref const cc; png.chunks) {
      if (cast(const(char)[])cc.type[] == "grAb") {
        if (cc.payload.length == 0) continue;
        bool allZero = true;
        foreach (const ubyte b; cc.payload) if (b != 0) { allZero = false; break; }
        if (allZero) continue;
        auto fo = VFile(fname~".grAb", "w");
        fo.rawWriteExact(cc.payload[]);
      }
    }
  }
}
