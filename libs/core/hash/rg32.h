// ////////////////////////////////////////////////////////////////////////// //
// http://www.samiam.org/rg32/
// RadioGatun 32 class
// slightly modified by Ketmar // Invisible Vector

class RG32 {
public:
  enum { BitSize = 256 };

private:
  vuint32 a[20]; // mill
  vuint32 b[40]; // belt
  vuint32 iword; // input word
  vuint32 p[3]; // input words (we add three at a time)
  int place; // place of byte in input word
  int mplace; // word from mill to use in output
  int pword; // word (we input three at a time) to add input to
  bool finished;

private:
  void init ();
  void mill ();
  void belt ();
  void input_char (vuint8 a);

public:
  RG32 ();
  RG32 (const void *data, vuint32 datalen);
  void put (const void *data, vuint32 datalen);
  void finish (vuint8 res[BitSize/8]);
};
