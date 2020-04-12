import arsd.image;
import iv.vfs.io;


PNG *loadPng (string fname) {
  import std.file;
  return readPng(cast(ubyte[])read(fname));
}


int getBE32 (const(ubyte)[] src) {
  uint v = 0;
  foreach (int f; 0..4) {
    v <<= 8;
    if (src.length > 0) {
      v |= src[0];
      src = src[1..$];
    }
  }
  return cast(int)v;
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


struct Grab {
  int x = 0;
  int y = 0;
}


bool isEmptyHLine (MemoryImage img, int y) {
  if (y < 0 || y >= img.height) return true;
  foreach (int x; 0..img.width) {
    auto clr = img.getPixel(x, y);
    if (clr.a != 0) return false;
  }
  return true;
}


bool isEmptyVLine (MemoryImage img, int x) {
  if (x < 0 || x >= img.width) return true;
  foreach (int y; 0..img.height) {
    auto clr = img.getPixel(x, y);
    if (clr.a != 0) return false;
  }
  return true;
}


void processPng (string fname) {
  auto png = loadPng(fname);
  Grab grab;
  Chunk *gchunk = png.getChunkNullable("grAb");
  if (gchunk) {
    if (gchunk.size != 8) throw new Exception("file '"~fname~"' has invalid 'grAb' chunk");
    grab.x = getBE32(gchunk.payload[0..$]);
    grab.y = getBE32(gchunk.payload[4..$]);
  } else {
    writeln("WARNING: ", fname, " has no 'grAb' chunk!");
  }

  MemoryImage img = imageFromPng(png);

  int sx = 0;
  while (sx < img.width && img.isEmptyVLine(sx)) ++sx;
  int ex = img.width-1;
  while (ex >= 0 && img.isEmptyVLine(ex)) --ex;

  int sy = 0;
  while (sy < img.height && img.isEmptyHLine(sy)) ++sy;
  int ey = img.height-1;
  while (ey >= 0 && img.isEmptyHLine(ey)) --ey;

  // leave one-pixel border for bilinear filtering
  if (sx > 0) --sx;
  if (sy > 0) --sy;
  if (ex < img.width-1) ++ex;
  if (ey < img.height-1) ++ey;

  if (sx == 0 && sy == 0 && ex == img.width-1 && ey == img.height-1) return;

  if (sx > ex || sy > ey) {
    writeln("WARNING: ", fname, " is empty, reducing to one pixel");
    sx = 0;
    sy = 0;
    ex = 0;
    ey = 0;
  }

  writeln(fname, ": original is (0,0)-(", img.width-1, ",", img.height-1, "); new is (", sx, ",", sy, ")-(", ex, ",", ey, ")");

  // fix grab
  grab.x -= sx;
  grab.y -= sy;

  // create new image (truecolor for now)
  TrueColorImage newimg = new TrueColorImage(ex-sx+1, ey-sy+1);
  foreach (int y; sy..ey+1) {
    foreach (int x; sx..ex+1) {
      newimg.setPixel(x-sx, y-sy, img.getPixel(x, y));
    }
  }

  auto newpng = pngFromImage(newimg);
  // create 'grAb' chunk if necessary
  if (grab.x || grab.y) {
    ubyte[] payload;
    putBE32(payload, grab.x);
    putBE32(payload, grab.y);
    assert(payload.length == 8);
    Chunk *newgrab = Chunk.create("grAb", payload);
    newpng.insertChunk(newgrab, true);
    newpng.length = 0;
  }
  // write png file
  import std.file;
  std.file.write(fname, writePng(newpng));
}


void main (string[] args) {
  //if (args.length == 1) args ~= "rif9a0.png";
  if (args.length < 2) {
    writeln("this utility will remove empty png pixels, and will fix 'grAb' chunk accordingly");
    writeln("usage: zshring_png.d filelist");
    return;
  }
  foreach (string fname; args[1..$]) processPng(fname);
  //destpng.insertChunk(&grab, true);
  //destpng.length = 0;
  //import std.file;
  //std.file.write(args[2], writePng(destpng));
}
