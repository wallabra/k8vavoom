//**************************************************************************
//
//  Basic functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, get_GC_ImmediateDelete) { RET_BOOL(GImmediadeDelete); }
IMPLEMENT_FUNCTION(VObject, set_GC_ImmediateDelete) { P_GET_BOOL(val); GImmediadeDelete = val; }


// native static final void ccmdClearText ();
IMPLEMENT_FUNCTION(VObject, ccmdClearText) {
  ccmdClearText();
}

// native static final void ccmdClearCommand ();
IMPLEMENT_FUNCTION(VObject, ccmdClearCommand) {
  ccmdClearCommand();
}

// native static final CCResult ccmdParseOne ();
IMPLEMENT_FUNCTION(VObject, ccmdParseOne) {
  RET_INT(ccmdParseOne());
}

// native static final int ccmdGetArgc ();
IMPLEMENT_FUNCTION(VObject, ccmdGetArgc) {
  RET_INT(ccmdGetArgc());
}

// native static final string ccmdGetArgv (int idx);
IMPLEMENT_FUNCTION(VObject, ccmdGetArgv) {
  P_GET_INT(idx);
  RET_STR(ccmdGetArgv(idx));
}

// native static final int ccmdTextSize ();
IMPLEMENT_FUNCTION(VObject, ccmdTextSize) {
  RET_INT(ccmdTextSize());
}

// native static final void ccmdPrepend (string str);
IMPLEMENT_FUNCTION(VObject, ccmdPrepend) {
  P_GET_STR(str);
  ccmdPrepend(str);
}

// native static final void ccmdPrependQuoted (string str);
IMPLEMENT_FUNCTION(VObject, ccmdPrependQuoted) {
  P_GET_STR(str);
  ccmdPrependQuoted(str);
}

// native static final void ccmdAppend (string str);
IMPLEMENT_FUNCTION(VObject, ccmdAppend) {
  P_GET_STR(str);
  ccmdAppend(str);
}

// native static final void ccmdAppendQuoted (string str);
IMPLEMENT_FUNCTION(VObject, ccmdAppendQuoted) {
  P_GET_STR(str);
  ccmdAppendQuoted(str);
}
