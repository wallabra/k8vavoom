//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
//#define Random()  ((float)(rand() & 0x7fff) / (float)0x8000)
//#define RandomFull()  ((float)(rand() & 0x7fff) / (float)0x7fff)


// this is used to compare floats like ints which is faster
#define FASI(var_) (*(const int32_t *)&(var_))


// An output device.
class FOutputDevice : public VLogListener {
public:
  // FOutputDevice interface
  virtual ~FOutputDevice () noexcept;

  // simple text printing
  void Log (const char *S) noexcept;
  void Log (EName Type, const char *S) noexcept;
  void Log (VStr S) noexcept;
  void Log (EName Type, VStr S) noexcept;
  void Logf (const char *Fmt, ...) noexcept __attribute__((format(printf, 2, 3)));
  void Logf (EName Type, const char *Fmt, ...) noexcept __attribute__((format(printf, 3, 4)));
};

// error logs
//extern FOutputDevice *GLogSysError;
//extern FOutputDevice *GLogHostError;


VVA_CHECKRESULT int superatoi (const char *s) noexcept;

//VVA_CHECKRESULT int ParseHex (const char *Str);
VVA_CHECKRESULT vuint32 M_LookupColorName (const char *Name); // returns 0 if not found (otherwise high bit is set)
// this returns color with high byte set to `0xff` (and black color for unknown names)
// but when `retZeroIfInvalid` is `true`, it returns `0` for unknown color
VVA_CHECKRESULT vuint32 M_ParseColor (const char *Name, bool retZeroIfInvalid=false);


//==========================================================================
//
//  ClipLineToRect0
//
//  Based on Cohen-Sutherland clipping algorithm but with a slightly faster
//  reject and precalculated slopes. If the speed is needed, use a hash
//  algorithm to handle the common cases.
//
//==========================================================================
#define CLTR_LEFT   (1<<0)
#define CLTR_RIGHT  (1<<1)
#define CLTR_BOTTOM (1<<2)
#define CLTR_TOP    (1<<3)

#define CLTR_DOOUTCODE(oc,mx,my) \
  (oc) = 0; \
       if ((my) < 0) (oc) |= CLTR_TOP; \
  else if ((my) >= boxhd) (oc) |= CLTR_BOTTOM; \
       if ((mx) < 0) (oc) |= CLTR_LEFT; \
  else if ((mx) >= boxwd) (oc) |= CLTR_RIGHT;

static VVA_OKUNUSED inline bool ClipLineToRect0 (float *ax, float *ay, float *bx, float *by,
                                                 const float boxw, const float boxh) noexcept
{
  if (boxw < 1 || boxh < 1) return false;

  int outcode1 = 0;
  int outcode2 = 0;

  // do trivial rejects and outcodes
  if (*ay >= boxh) outcode1 = CLTR_TOP; else if (*ay < 0) outcode1 = CLTR_BOTTOM;
  if (*by >= boxh) outcode2 = CLTR_TOP; else if (*by < 0) outcode2 = CLTR_BOTTOM;
  if (outcode1&outcode2) return false; // trivially outside

  if (*ax < 0) outcode1 |= CLTR_LEFT; else if (*ax > boxw) outcode1 |= CLTR_RIGHT;
  if (*bx < 0) outcode2 |= CLTR_LEFT; else if (*bx > boxw) outcode2 |= CLTR_RIGHT;
  if (outcode1&outcode2) return false; // trivially outside

  // use doubles here for better precision
  double x0 = (double)(*ax), y0 = (double)(*ay);
  double x1 = (double)(*bx), y1 = (double)(*by);
  const double boxwd = (double)boxw;
  const double boxhd = (double)boxh;

  CLTR_DOOUTCODE(outcode1, x0, y0);
  CLTR_DOOUTCODE(outcode2, x1, y1);

  if (outcode1&outcode2) return false;

  double tmpx = 0, tmpy = 0;

  while (outcode1|outcode2) {
    // may be partially inside box
    // find an outside point
    int outside = (outcode1 ? outcode1 : outcode2);

    // clip to each side
    if (outside&CLTR_TOP) {
      const double dy = y0-y1;
      const double dx = x1-x0;
      tmpx = x0+(dx*(y0))/dy;
      tmpy = 0;
    } else if (outside&CLTR_BOTTOM) {
      const double dy = y0-y1;
      const double dx = x1-x0;
      tmpx = x0+(dx*(y0-boxhd))/dy;
      tmpy = boxhd-1;
    } else if (outside&CLTR_RIGHT) {
      const double dy = y1-y0;
      const double dx = x1-x0;
      tmpy = y0+(dy*(boxwd-1-x0))/dx;
      tmpx = boxwd-1;
    } else if (outside&CLTR_LEFT) {
      const double dy = y1-y0;
      const double dx = x1-x0;
      tmpy = y0+(dy*(-x0))/dx;
      tmpx = 0;
    }

    if (outside == outcode1) {
      x0 = tmpx;
      y0 = tmpy;
      CLTR_DOOUTCODE(outcode1, x0, y0);
    } else {
      x1 = tmpx;
      y1 = tmpy;
      CLTR_DOOUTCODE(outcode2, x1, y1);
    }

    if (outcode1&outcode2) return false; // trivially outside
  }

  *ax = (float)x0;
  *ay = (float)y0;
  *bx = (float)x1;
  *by = (float)y1;
  return true;
}

#undef CLTR_DOOUTCODE
#undef CLTR_TOP
#undef CLTR_LEFT
#undef CLTR_RIGHT
#undef CLTR_BOTTOM


template<unsigned tileWidth, unsigned tileHeight> struct DDALineWalker {
public:
  // worker variables
  // use doubles here for better precision
  int currTileX, currTileY;
  int endTileX, endTileY;
  double deltaDistX, deltaDistY; // length of ray from one x or y-side to next x or y-side
  double sideDistX, sideDistY; // length of ray from current position to next x-side
  int stepX, stepY; // what direction to step in x/y (either +1 or -1)
  int cornerHit; // 0: no; 1: return horiz cell; 2: return vert cell; 3: do step on both dirs; 4: abort
  int maxTileX, maxTileY;

public:
  inline DDALineWalker () noexcept {}
  inline DDALineWalker (int x0, int y0, int x1, int y1, int amaxtx, int amaxty) noexcept { start(x0, y0, x1, y1, amaxtx, amaxty); }

  void start (int x0, int y0, int x1, int y1, int amaxtx, int amaxty) noexcept {
    cornerHit = 0;
    maxTileX = amaxtx;
    maxTileY = amaxty;

    #if 0
    // this seems to be better for negative coords, but negatives should not arrive here
    const int tileSX = int(floor(double(x0)/double(tileWidth)));
    const int tileSY = int(floor(double(y0)/double(tileHeight)));
    endTileX = int(floor(double(x1)/double(tileWidth)));
    endTileY = int(floor(double(y1)/double(tileHeight)));
    #else
    const int tileSX = x0/tileWidth;
    const int tileSY = y0/tileHeight;
    endTileX = x1/tileWidth;
    endTileY = y1/tileHeight;
    #endif

    currTileX = tileSX;
    currTileY = tileSY;

    //GLog.Logf(NAME_Debug, "*** start=(%d,%d)", tileSX, tileSY);
    //GLog.Logf(NAME_Debug, "*** end  =(%d,%d)", endTileX, endTileY);

    // it is ok to waste some time here
    if (tileSX == endTileX || tileSY == endTileY) {
      if (tileSX == endTileX && tileSY == endTileY) {
        // nowhere to go (but still return the starting tile)
        //GLog.Logf(NAME_Debug, "  POINT!");
        stepX = stepY = 0; // this can be used as a "stop signal"
      } else if (tileSX == endTileX) {
        // vertical
        vassert(tileSY != endTileY);
        stepX = 0;
        stepY = (y0 < y1 ? 1 : -1);
        //GLog.Logf(NAME_Debug, "  VERTICAL!");
      } else {
        // horizontal
        vassert(tileSY == endTileY);
        stepX = (x0 < x1 ? 1 : -1);
        stepY = 0;
        //GLog.Logf(NAME_Debug, "  HORIZONTAL!");
      }
      //GLog.Logf(NAME_Debug, "  step=(%d,%d)!", stepX, stepY);
      // init variables to shut up the compiler
      deltaDistX = deltaDistY = sideDistX = sideDistY = 0;
      return;
    }

    // inverse length, so we can use multiply instead of division (marginally faster)
    const double absdx = double(1)/fabs(double(x1)-double(x0));
    const double absdy = double(1)/fabs(double(y1)-double(y0));

    if (x0 < x1) {
      stepX = 1;
      sideDistX = ((tileSX+1)*tileWidth-x0)*absdx;
    } else {
      stepX = -1;
      sideDistX = (x0-tileSX*tileWidth)*absdx;
    }

    if (y0 < y1) {
      stepY = 1;
      sideDistY = (double)((tileSY+1)*tileHeight-y0)*absdy;
    } else {
      stepY = -1;
      sideDistY = (double)(y0-tileSY*tileHeight)*absdy;
    }

    deltaDistX = double(tileWidth)*absdx;
    deltaDistY = double(tileHeight)*absdy;
  }

  // returns `false` if we're done (and the coords are undefined)
  inline bool next (int &tilex, int &tiley) noexcept {
    // check for a special condition
    if (cornerHit) {
      //GLog.Logf(NAME_Debug, "CORNER: %d", cornerHit);
      switch (cornerHit++) {
        case 1: // check adjacent horizontal tile
          if (!stepX) { ++cornerHit; goto doadjy; } // this shouldn't happen, but better play safe
          tilex = currTileX+stepX;
          tiley = currTileY;
          if (tilex < 0 || tilex >= maxTileX) { ++cornerHit; goto doadjy; }
          return true;
        case 2: // check adjacent vertical tile
         doadjy:
          if (!stepY) goto donextc;
          tilex = currTileX;
          tiley = currTileY+stepY;
          if (tiley < 0 || tiley >= maxTileY) goto donextc; // this shouldn't happen, but better play safe
          return true;
        case 3: // move to the next tile
         donextc:
          // corner case check (this shouldn't happen, but better play safe)
          if (currTileX == endTileX) stepX = 0;
          if (currTileY == endTileY) stepY = 0;
          // move through the corner
          sideDistX += deltaDistX;
          currTileX += stepX;
          sideDistY += deltaDistY;
          currTileY += stepY;
          // resume normal processing
          cornerHit = 0;
          break;
        default:
          return false; // no more
      }
    }

    // return current tile coordinates
    tilex = currTileX;
    tiley = currTileY;

    //GLog.Logf(NAME_Debug, "  000: res=(%d,%d); step=(%d,%d); sideDist=(%g,%g); delta=(%g,%g)", tilex, tiley, stepX, stepY, sideDistX, sideDistY, deltaDistX, deltaDistY);
    // check if we're done (sorry for this bitop mess); checks step vars just to be sure
    if (!((currTileX^endTileX)|(currTileY^endTileY)) || !(stepX|stepY)) {
      cornerHit = 4; // no more
    } else if (stepX && stepY) {
      // jump to next map square, either in x-direction, or in y-direction
      //print("  sdxy=(%s,%s); dxy=(%s,%s); stxy=(%s,%s)", sideDistX, sideDistY, deltaDistX, deltaDistY, stepX, stepY);
      if (sideDistX == sideDistY) {
        // this will jump through a corner, so we have to process adjacent cells first (see the code above)
        cornerHit = 1;
      } else if (sideDistX < sideDistY) {
        // horizontal step
        sideDistX += deltaDistX;
        currTileX += stepX;
        // don't overshoot (sadly, this may happen with long traces)
        if (currTileX == endTileX) stepX = 0;
      } else {
        // vertical step
        sideDistY += deltaDistY;
        currTileY += stepY;
        // don't overshoot (sadly, this may happen with long traces)
        if (currTileY == endTileY) stepY = 0;
      }
    } else {
      // horizontal or vertical
      currTileX += stepX;
      currTileY += stepY;
    }
    //GLog.Logf(NAME_Debug, "  001: res=(%d,%d); step=(%d,%d); sideDist=(%g,%g); delta=(%g,%g); cc=%d", tilex, tiley, stepX, stepY, sideDistX, sideDistY, deltaDistX, deltaDistY, cornerHit);

    return true;
  }
};
