import arsd.image;
import iv.vfs.io;


PNG *loadPng (string fname) {
  import std.file;
  return readPng(cast(ubyte[])read(fname));
}


void putBE32 (ref ubyte[] dest, int v) {
  uint uv = cast(uint)v;
  dest ~= (v>>24)&0xff;
  dest ~= (v>>16)&0xff;
  dest ~= (v>>8)&0xff;
  dest ~= v&0xff;
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
  if (args.length < 3) {
    writeln("usage: zgrab_set xofs yofs files+");
    return;
  }

  import std.conv : to;

  int xofs = args[1].to!int;
  int yofs = args[2].to!int;

  foreach (string fname; args[3..$]) {
    auto destpng = loadPng(fname);
    ubyte[] payload;
    putBE32(payload, xofs);
    putBE32(payload, yofs);
    assert(payload.length == 8);
    Chunk *gc = Chunk.create("grAb", payload);
    destpng.insertChunk(gc, true);
    destpng.length = 0;

    import std.file;
    std.file.write(fname, writePng(destpng));
    writeln(fname, " ... OK");
  }
}
