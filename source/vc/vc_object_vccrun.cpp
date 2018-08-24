//**************************************************************************
//
//  Basic functions
//
//**************************************************************************
IMPLEMENT_FUNCTION(VObject, get_ImmediateDelete) { RET_BOOL(GImmediadeDelete); }
IMPLEMENT_FUNCTION(VObject, set_ImmediateDelete) { P_GET_BOOL(val); GImmediadeDelete = val; }
