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


void putBE32 (ref ubyte[] dest, int v) {
  uint uv = cast(uint)v;
  dest ~= (v>>24)&0xff;
  dest ~= (v>>16)&0xff;
  dest ~= (v>>8)&0xff;
  dest ~= v&0xff;
}


void main (string[] args) {
  foreach (string fname; args[1..$]) {
    writeln("converting '", fname, "'...");
    auto tc = MemoryImage.fromImage(fname).getAsTrueColorImage;
    int ofsx = 0, ofsy = 0;
    {
      auto png = loadPng(fname);
      if (auto grab = png.getChunk("grAb")) {
        if (grab.size != 8) assert(0);
        ofsx = grab.payload[3]|(grab.payload[2]<<8)|(grab.payload[1]<<16)|(grab.payload[0]<<24);
        ofsy = grab.payload[4+3]|(grab.payload[4+2]<<8)|(grab.payload[4+1]<<16)|(grab.payload[4+0]<<24);
        writeln("  offset: (", ofsx, ", ", ofsy, ")");
      }
    }
    // convert
    foreach (immutable y; 0..tc.height) {
      foreach (immutable x; 0..tc.width) {
        Color c = tc.getPixel(x, y);
        if (c.a == 0) c.r = 0;
        c.g = c.r;
        c.b = c.r;
        tc.setPixel(x, y, c);
      }
    }
    // create new png
    PNG *dpng = pngFromImage(tc);
    // insert `grAb`
    if (ofsx || ofsy) {
      ubyte[] payload;
      putBE32(payload, ofsx);
      putBE32(payload, ofsy);
      assert(payload.length == 8);
      Chunk *gc = Chunk.create("grAb", payload);
      dpng.insertChunk(gc, true);
      dpng.length = 0;
    }
    {
      import std.file;
      std.file.write(fname, writePng(dpng));
    }
  }
}
