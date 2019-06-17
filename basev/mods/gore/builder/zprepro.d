import iv.strex;
import iv.vfs.io;

string[string] vars;
bool[string] defs;


string[] processFile (string fname) {
  static struct IfState {
    bool inelse;
    bool skip; // skipping current branch
    bool active; // if not active -- just skipping it
  }
  IfState[] ifstack;
  string[] res;

  foreach (string line; VFile(fname).byLineCopy) {
    // returns `true` if line should be skipped
    // also, fixes `ifstack`
    bool doPrepro () {
      string ln = line.xstrip;
      // directive
      if (ln.length && ln[0] == '#') {
        ln = ln[1..$].xstrip;
        if (ln.length == 0) assert(0, "preprocessor?");
        auto spp = ln.indexOf(' ');
        if (spp < 0) spp = ln.length;
        string cmd = ln[0..spp];
        string arg = ln[spp..$].xstrip;
        // ifdef/ifndef
        if (cmd == "ifdef" || cmd == "ifndef") {
          // in ignore part?
          if (ifstack.length > 0 && (ifstack[$-1].skip || !ifstack[$-1].active)) {
            ifstack ~= IfState(false, true, false);
            return true;
          }
          if (arg.length == 0) assert(0, "#ifdef arg?");
          ifstack ~= IfState(false, (arg in defs ? (cmd == "ifndef") : (cmd == "ifdef")), true);
          return true;
        }
        // else
        if (cmd == "else") {
          // in ignore part?
          if (ifstack.length == 0) assert(0, "#else without #if");
          if (ifstack[$-1].inelse) assert(0, "duplicate #else");
          ifstack[$-1].inelse = true;
          ifstack[$-1].skip = !ifstack[$-1].skip;
          return true;
        }
        // endif
        if (cmd == "endif") {
          if (ifstack.length == 0) assert(0, "#endif without #if");
          ifstack.length -= 1;
          return true;
        }
        // define
        if (cmd == "define") {
          // in ignore part?
          if (ifstack.length > 0 && (ifstack[$-1].skip || !ifstack[$-1].active)) return true; // skip it
          if (arg.length == 0) assert(0, "#define arg?");
          defs[arg] = true;
          return true;
        }
        // undef
        if (cmd == "undef") {
          // in ignore part?
          if (ifstack.length > 0 && (ifstack[$-1].skip || !ifstack[$-1].active)) return true; // skip it
          if (arg.length == 0) assert(0, "#undef arg?");
          defs.remove(arg);
          return true;
        }
        assert(0, "invalid preprocessor command: "~cmd);
      }
      // normal line
      if (ifstack.length == 0) return false;
      auto st = ifstack[$-1];
      if (!st.active) return true; // skip it
      if (st.skip) return true; // skip it
      return false; // don't skip
    }

    if (doPrepro()) continue;

    // replace vars
    string s;
    for (;;) {
      auto stp = line.indexOf('$');
      if (stp < 0) { s ~= line; break; }
      if (line.length-stp < 3 || (line[stp+1] != '(' && line[stp+1] != '{')) {
        s ~= line[0..stp+1];
        line = line[stp+1..$];
        continue;
      }
      char ech = (line[stp+1] == '(' ? ')' : '}');
      s ~= line[0..stp];
      line = line[stp+2..$];
      auto cpos = line.indexOf(ech);
      if (cpos < 0) assert(0, "wtf?! "~line);
      string name = line[0..cpos].xstrip;
      line = line[cpos+1..$];
      if (name !in vars) assert(0, "variable '"~name~"' is not defined");
      s ~= vars[name];
    }
    res ~= s;
  }

  if (ifstack.length) assert(0, "unbalanced preprocessor");

  return res;
}


void main (string[] args) {
  bool append = false;
  string[] infiles;
  bool nomore = false;
  for (usize f = 1; f < args.length; ) {
    string arg = args[f++];
    if (!arg.length) continue;
    if (!nomore) {
      if (arg == "--") { nomore = true; continue; }
      // -D
      if (arg.startsWith("-D")) {
        if (arg.length == 2) {
          if (f >= args.length) assert(0, "wtf -D?!");
          arg = args[f++];
        } else {
          arg = arg[2..$];
        }
        defs[arg] = true;
        continue;
      }
      // name=value
      if (arg.indexOf('=') >= 0) {
        auto epos = arg.indexOf('=');
        string name = arg[0..epos].xstrip;
        string value = arg[epos+1..$].xstrip;
        if (name.length == 0) assert(0, "wtf name?!");
        vars[name] = value;
        continue;
      }
      if (arg == "--append" || arg == "-a") { append = true; continue; }
      if (arg[0] == '-') assert(0, "invalid argument: "~arg);
    }
    bool found = false;
    foreach (string fn; infiles) if (fn == arg) { found = true; break; }
    if (!found) infiles ~= arg;
  }

  if (infiles.length != 2) assert(0, "you should specify input and output files");
  auto res = processFile(infiles[0]);
  auto fo = VFile(infiles[1], (append ? "r+" : "w"));
  if (append) fo.seek(fo.size);
  foreach (string s; res) fo.writeln(s);
  fo.close();
}
