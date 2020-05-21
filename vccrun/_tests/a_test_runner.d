#!/usr/bin/env rdmd

import iv.strex;
import iv.vfs.io;
import iv.vfs.util;


__gshared bool generateFiles = false;


__gshared string vccrunPath = "/home/ketmar/back/vavoom_dev/_build_vccrun/vccrun/vccrun";
__gshared string[] vccrunArgs = [
  "-stderr-backtrace",
  "-nocol",
  "-DVCCRUN_PACKAGE_CONSTANT_TEST",
  "-pakdir",
  "!packages", //"../packages"
  "-P.",
  "!source",
  "boo",
  "foo",
  "zoo",
];


struct TestResult {
  string name;
  bool passed;
  // in case it is failed
  string errormsg;
  string[] outstd;
  string[] outerr;
  // expected
  string[] checkstd;
  string[] checkerr;
}


string generateOutFile (string name) {
  string outfname = name.removeExtension;
  outfname = "outfiles/"~outfname~".out";
  //string outerrname = outfname~".err";
  return outfname;
}


TestResult runTest (string name) {
  string[] args;
  args.reserve(vccrunArgs.length+1);
  args ~= vccrunPath;
  foreach (string s; vccrunArgs) {
    if (s == "!packages") {
      s = "../packages";
    } else if (s == "!source") {
      s = name;
    }
    args ~= s;
  }

  string outfname = generateOutFile(name);
  string outerrname = outfname~".err";

  // find "// FAIL" comment
  bool shouldFail = false;
  {
    string text = readTextFile(name).xstrip;
    if (text.length && text[0] == '/' && text[1] == '/') {
      text = text[2..$].xstrip;
      if (text.length > 4) text = text[0..4];
      shouldFail = text.strEquCI("FAIL");
    }
  }

  // load result files (if we aren't generating)
  string[] checkout;
  string[] checkerr;

  if (!generateFiles) {
    foreach (string s; VFile(outfname).byLineCopy) checkout ~= s;
    // it is ok for err file to absent
    try {
      foreach (string s; VFile(outerrname).byLineCopy) checkerr ~= s;
    } catch (Exception e) {} // sorry
  }

  import std.process;
  auto pipe = pipeProcess(args, Redirect.stdout|Redirect.stderr);

  string[] output;
  foreach (auto line; pipe.stdout.byLine) output ~= line.idup;

  string[] errors;
  foreach (auto line; pipe.stderr.byLine) errors ~= line.idup;

  int res = wait(pipe.pid);

  TestResult result;
  result.name = name;
  result.passed = true;

  // check "should fail" condition
  if ((shouldFail && res == 0) || (!shouldFail && res != 0)) {
    import std.format : format;
    result.passed = false;
    result.errormsg = format("test should %sfail, but it isn't!", (shouldFail ? "" : "not "));
  }

  // check results if we are generating
  if (!generateFiles) {
    // check lengthes
    if (result.passed) {
      result.passed = (checkout.length == output.length && checkerr.length == errors.length);
      if (!result.passed) {
        import std.format : format;
        result.errormsg = format("test results are of invalid size (%s:%s) (%s:%s)!", checkout.length, output.length, checkerr.length, errors.length);
      }
    }

    // check output text
    if (result.passed) {
      foreach (auto idx, string s; checkout) {
        if (s != output[idx]) {
          import std.format : format;
          result.passed = false;
          result.errormsg = format("test output doesn't match!");
          break;
        }
      }
    }

    // check error text
    if (result.passed) {
      foreach (auto idx, string s; checkerr) {
        if (s != errors[idx]) {
          import std.format : format;
          result.passed = false;
          result.errormsg = format("test errors aren't match!");
          break;
        }
      }
    }

    // remebmer output in case of error
    if (!result.passed) {
      result.outstd = output;
      result.outerr = errors;
      result.checkstd = checkout;
      result.checkerr = checkerr;
    }
  } else {
    // just remebmer output
    result.outstd = output;
    result.outerr = errors;
  }

  //writeln("output: ", output.length, "; error: ", errors.length);

  return result;
}


// ////////////////////////////////////////////////////////////////////////// //
__gshared TestResult[] failed;

bool compareStringLists (const string[] list0, const string[] list1) {
  if (list0.length != list1.length) return false;
  foreach (auto idx, string s; list0) if (list1[idx] != s) return false;
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
void runAllTests (string[] tests) {
  class SyncClass {}
  SyncClass scc = new SyncClass;

  import std.parallelism;
  foreach (string testname; parallel(tests)) {
    TestResult res = runTest(testname);
    if (!res.passed) {
      // this test failed!
      synchronized(scc) {
        failed ~= res;
      }
    }
  }
  writefln("%s tests passed, %s tests failed", tests.length-failed.length, failed.length);

  // dump failed tests
  foreach (const ref TestResult res; failed) {
    writefln("=============== %s ===============", res.name);
    writefln("ERROR: %s", res.errormsg);
    if (!compareStringLists(res.checkstd, res.outstd)) {
      writeln("--- EXPECTED OUTPUT ---");
      foreach (string s; res.checkstd) writeln(s);
      writeln("--- ACTUAL OUTPUT ---");
      foreach (string s; res.outstd) writeln(s);
    }
    if (!compareStringLists(res.checkerr, res.outerr)) {
      writeln("--- EXPECTED ERRORS ---");
      foreach (string s; res.checkerr) writeln(s);
      writeln("--- ACTUAL ERRORS ---");
      foreach (string s; res.outerr) writeln(s);
    }
    writeln;
  }

  // and show all failed tests again
  if (failed.length) {
    writeln();
    writefln("%s tests passed, %s tests failed", tests.length-failed.length, failed.length);
    writeln("==== FAILED TESTS (brief) ====");
    foreach (const ref TestResult res; failed) writeln(res.name);
  }
}



// ////////////////////////////////////////////////////////////////////////// //
void genAllTests (string[] tests) {
  class SyncClass {}
  SyncClass scc = new SyncClass;

  __gshared string[] togen;

  generateFiles = true;
  import std.parallelism;
  foreach (string testname; parallel(tests)) {
    string outfname = generateOutFile(testname);
    bool fileIsHere = false;
    try { auto fl = VFile(outfname); fileIsHere = true; } catch (Exception e) {} // sorry
    if (!fileIsHere) {
      synchronized(scc) {
        togen ~= testname;
      }
    }
  }

  if (togen.length == 0) {
    writeln("no new tests found!");
  } else {
    writefln("%s new test%s found", togen.length, (togen.length == 1 ? "" : "s"));
    foreach (string testname; /*parallel*/(togen)) {
      writefln("generating data for test '%s'...", testname);
      TestResult res = runTest(testname);
      if (!res.passed) {
        import std.string : format;
        throw new Exception(format("test '%s' is not passed! <%s>", testname, res.errormsg));
      }
      // write files
      string outfname = generateOutFile(testname);
      // normal
      {
        auto fo = VFile(outfname, "w");
        foreach (string s; res.outstd) writeln(fo, s);
      }
      // error
      if (res.outerr.length) {
        auto fo = VFile(outfname~".err", "w");
        foreach (string s; res.outerr) writeln(fo, s);
      }
    }
  }
}


void main (string[] args) {
  //runTest("ztest_rostruct_00.vc");
  import std.file;
  string[] tests;
  foreach (auto de; dirEntries(".", "*.vc", SpanMode.shallow)) {
    tests ~= de.name.baseName;
  }
  writefln("%s test%s found", tests.length, (tests.length == 1 ? "" : "s"));

  if (args.length > 1) {
    if (args[1] != "generate") assert(0, "'generate' expected");
    genAllTests(tests);
  } else {
    runAllTests(tests);
  }
}
