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


void convert (string fnameIn, string fnameOut) {
  writeln("converting '", fnameIn, "'...");
  auto tc = MemoryImage.fromImage(fnameIn).getAsTrueColorImage;
  tc = imageResize(tc, tc.width/4, tc.height/4);
  int ofsx = 0, ofsy = 0;
  /*
  {
    auto png = loadPng(fnameIn);
    if (auto grab = png.getChunk("grAb")) {
      if (grab.size != 8) assert(0);
      ofsx = grab.payload[3]|(grab.payload[2]<<8)|(grab.payload[1]<<16)|(grab.payload[0]<<24);
      ofsy = grab.payload[4+3]|(grab.payload[4+2]<<8)|(grab.payload[4+1]<<16)|(grab.payload[4+0]<<24);
      writeln("  offset: (", ofsx, ", ", ofsy, ")");
    }
  }
  */
  ofsx = tc.width/2;
  ofsy = tc.height/2;
  // convert
  foreach (immutable y; 0..tc.height) {
    foreach (immutable x; 0..tc.width) {
      Color c = tc.getPixel(x, y);
      /*
      if (c.a == 0) c.r = 0;
      c.g = c.r;
      c.b = c.r;
      */
      c.a = c.r;
      //c.r = 255;
      c.r = 0x88;
      c.g = 0;
      c.b = 0;
      if (c.a < 127) c.r = 0xff;
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
    std.file.write(fnameOut, writePng(dpng));
  }
}


void main (string[] args) {
//bpdla0.png
//blood1.png
  foreach (auto idx; 0..4) {
    import std.format : format;
    string fin = "blood%d.png".format(idx+1);
    string fout = "bpdl%c0.png".format(cast(char)('a'+idx));
    convert(fin, fout);
  }
}
