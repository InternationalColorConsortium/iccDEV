// IccToXml.cpp : Defines the entry point for the console application.
//

#include <cstdio>
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccProfileXml.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccProfLibVer.h"
#include "IccLibXMLVer.h"
#include "IccXmlConfig.h"
#include <cstdlib>
#include <cstring> /* C strings strcpy, memcpy ... */

// The usage screen, so the help path and the incomplete-invocation path print the same
// text and cannot drift apart.
//
// The STREAM is what separates the two, following the ruling already applied to
// iccSpecSepToTiff for this same defect class (#1514): a help request prints to stdout
// and succeeds, a malformed invocation prints to stderr and fails.  That is what lets a
// caller tell "the user asked for help" from "this invocation converted nothing", which
// a shared stdout cannot express whatever the exit status says.  The status itself is
// EXIT_FAILURE rather than #1514's -1/255, because that is what every other error exit
// in this main() already returns (#2387).
static void Usage(FILE *out)
{
  fprintf(out, "IccFromXml built with IccProfLib Version " ICCPROFLIBVER ", IccLibXML Version " ICCLIBXMLVER "\n\n");
  fprintf(out, "Usage: IccFromXml xml_file saved_profile_file {-noid -v{=[relax_ng_schema_file - optional]}}\n");
  fprintf(out, "       IccFromXml -h | --help\n");
}

// True for the option forms that ask for the usage screen rather than a conversion.
static bool isHelpRequest(const char *szArg)
{
  return !stricmp(szArg, "-h") || !stricmp(szArg, "--help") ||
         !stricmp(szArg, "-help") || !stricmp(szArg, "-?");
}

// Whether szPath names something that can actually be READ as a file.
//
// fopen(dir, "r") succeeds on glibc, so "it opened" does not mean "it is a schema":
// -v=<some-directory> slipped past the openability guard below and produced a
// four-line libxml2 cascade naming the XML file instead of the one-line schema error
// the guard exists to give.  Reading a byte is the portable discriminator -- a
// directory fails the read with EISDIR, while a legitimately empty file reports EOF
// with no error (#2387).
static bool isReadableFile(const char *szPath)
{
  FILE *f = fopen(szPath, "r");
  if (!f)
    return false;

  char ch;
  bool bReadable = (fread(&ch, 1, 1, f) == 1) || (feof(f) && !ferror(f));
  fclose(f);
  return bReadable;
}

// The directory the running executable lives in, with a trailing separator, or an
// empty string when it cannot be determined.
//
// The derivation this replaces could not work.  It read
//   if (path.substr(0,1) != "./")   -- one character compared with a two-character
//                                      literal, so always true
//   path = path.substr(0, path.find_last_of("//"));
// and for an argv[0] carrying no separator -- the normal case for a tool found on
// PATH -- find_last_of returns npos, substr(0, npos) returns the whole basename, and
// the candidate became "iccFromXml//SampleIccRELAX.rng", which never exists.
//
// That was invisible while a failed lookup silently disabled validation.  Once the
// lookup fails CLOSED it turns a working installation into a hard refusal, so it has
// to genuinely work: measured, `PATH=<bindir>:$PATH iccFromXml in.xml out.icc -v`
// with the schema installed beside the binary refused to convert, while the same
// invocation by absolute path succeeded.  Walking PATH for the no-separator case is
// also what makes the error message's "alongside the executable" true, rather than
// naming a directory that was never examined (#2387).
static std::string executableDir(const char *szArgv0)
{
  std::string exe = szArgv0 ? szArgv0 : "";
  if (exe.empty())
    return "";

#ifdef WIN32
  const char *szSeps = "\\/";
  const char cPathSep = ';';
#else
  const char *szSeps = "/";
  const char cPathSep = ':';
#endif

  std::string::size_type nSlash = exe.find_last_of(szSeps);
  if (nSlash != std::string::npos)
    return exe.substr(0, nSlash + 1);

  const char *szPath = getenv("PATH");
  if (!szPath)
    return "";

  std::string path(szPath);
  std::string::size_type nStart = 0;
  while (nStart <= path.size()) {
    std::string::size_type nEnd = path.find(cPathSep, nStart);
    if (nEnd == std::string::npos)
      nEnd = path.size();

    std::string dir = path.substr(nStart, nEnd - nStart);
    if (!dir.empty()) {
      char cLast = dir[dir.size() - 1];
      if (cLast != '/' && cLast != '\\')
        dir += "/";
      if (isReadableFile((dir + exe).c_str()))
        return dir;
    }

    if (nEnd == path.size())
      break;
    nStart = nEnd + 1;
  }

  return "";
}

int main(int argc, char* argv[])
{
  // An explicit help request succeeds; an INCOMPLETE conversion does not.  Both used
  // to print usage and return 0, so `iccFromXml foo.xml` -- a real invocation missing
  // its output path -- reported success to automation while converting nothing, and a
  // bare `iccFromXml` did the same.  Splitting them is what the report asks for: the
  // screen is identical, the status is not (#2387).
  //
  // ONLY the lone-argument form is a help request.  Scanning every argv position for a
  // help flag looked harmless and was not: `iccFromXml in.xml out.icc -h` then returned
  // 0 having written no profile at all, where it previously ignored the flag and
  // converted.  That is the exact "exit 0 while doing something else" contract
  // violation this change exists to remove, reintroduced one argument to the right.  A
  // help flag mixed into a conversion now falls through to the option loop below and is
  // refused as unrecognised, which is a malformed invocation rather than a silent no-op.
  if (argc == 2 && isHelpRequest(argv[1])) {
    Usage(stdout);
    return EXIT_SUCCESS;
  }

  if (argc<=2) {
    Usage(stderr);
    fprintf(stderr, "\nError: an input XML file and an output profile path are both required\n");
    return EXIT_FAILURE;
  }

  // Profile XML on the CLI tool's filesystem is trusted by the invoking
  // user (same trust boundary as the XML file itself), so opt in to
  // <tag File="..."/> / <tag Filename="..."/> loaders. Library/WASM
  // callers leave the flag at its default-off state.
  IccXmlSetAllowFileIncludes(true);

  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  CIccProfileXml profile;
  std::string reason;  

  std::string szRelaxNGDir;
  bool bNoId = false;
  // Whether -v was given at all, kept apart from whether a schema was found.  The two
  // used to be conflated in szRelaxNGDir, which is why a failed lookup was
  // indistinguishable from "no validation asked for" (#2387).
  bool bValidateRequested = false;

  const char* szRelaxNGFileName = "SampleIccRELAX.rng";
  int i;
  for (i=3; i<argc; i++) {
    if (!stricmp(argv[i], "-noid")) {
      bNoId = true;
    }
    // Exact forms only.  This used to be strncmp(argv[i], "-v", 2), which accepted
    // "-verbose", "-vx" and anything else beginning "-v" as a validation request.
    else if (!stricmp(argv[i], "-v") ||
             !strncmp(argv[i], "-v=", 3) || !strncmp(argv[i], "-V=", 3)) {
      bValidateRequested = true;

      if (argv[i][2]=='=') {
        szRelaxNGDir = argv[i]+3;
        // An explicitly named schema that cannot be opened is a hard error.  Left
        // unchecked it reached LoadXml, which validates only against a schema it
        // could load, so a typo in the path silently disabled the validation the
        // user had just asked for.
        if (szRelaxNGDir.empty()) {
          fprintf(stderr, "Error: -v= requires a RelaxNG schema file path\n");
          return EXIT_FAILURE;
        }
        if (!isReadableFile(szRelaxNGDir.c_str())) {
          fprintf(stderr, "Error: cannot read RelaxNG schema '%s'\n", szRelaxNGDir.c_str());
          return EXIT_FAILURE;
        }
      }
      else  {
        // Bare -v.  The README says SampleIccRELAX.rng is taken "from the current
        // directory"; the code only ever derived it from argv[0], the directory the
        // BINARY lives in, so the documented invocation never found it.  Try the
        // documented location first, then the executable's own directory, so an
        // installation shipping the schema beside the binary keeps working.
        if (isReadableFile(szRelaxNGFileName)) {
          szRelaxNGDir = szRelaxNGFileName;
        }
        else {
          std::string dir = executableDir(argv[0]);
          if (!dir.empty()) {
            std::string path = dir + szRelaxNGFileName;
            if (isReadableFile(path.c_str()))
              szRelaxNGDir = path;
          }
        }
      }
    }
    else {
      // No rejecting branch existed, so an unrecognised option was skipped in
      // silence: an ordinary "-no-id" typo left bNoId false, produced a profile with
      // an ID, and still exited 0, reporting success for a run that ignored what it
      // was told to do.
      fprintf(stderr, "Error: unrecognized option '%s'\n", argv[i]);
      Usage(stderr);
      return EXIT_FAILURE;
    }
  }

  // Validation was requested and no schema could be found: fail CLOSED.  Previously
  // the lookup simply left the schema string empty, and CIccProfileXml::LoadXml
  // validates only when that string is non-empty, so bare -v was behaviourally
  // identical to passing no -v at all -- the option appeared to work while checking
  // nothing.  SampleIccRELAX.rng is not shipped in this tree, so that was the
  // outcome for every bare -v invocation here.
  if (bValidateRequested && szRelaxNGDir.empty()) {
    fprintf(stderr, "Error: -v requested schema validation but '%s' was not found in "
            "the current directory or alongside the executable\n", szRelaxNGFileName);
    return EXIT_FAILURE;
  }

  // Name the schema that was chosen.  The current directory now wins over the copy
  // installed beside the binary, so an unrelated file of that name in the invocation
  // directory becomes the validation authority -- and since the schema decides
  // accept/reject, a stray or weaker one changes the tool's verdict.  The ordering is
  // what the README documents, so it stays; the silence about which file was used is
  // the part worth fixing.
  if (bValidateRequested)
    printf("Validating against RelaxNG schema '%s'\n", szRelaxNGDir.c_str());

  // Re-indented to match its block.  The stray four-space indent was harmless while
  // nothing preceded it, but it now follows an if-statement and -Wmisleading-indentation
  // rejects it outright on the strict lanes.
  if (!profile.LoadXml(argv[1], szRelaxNGDir.c_str(), &reason)) {
    printf("%s", reason.c_str());
#ifndef WIN32
    printf("\n");
#endif
    printf("Unable to Parse '%s'\n", argv[1]);
    return EXIT_FAILURE;
  }

  // -noid has to mean the saved profile carries NO profile ID, and passing
  // icNeverWriteID alone does not achieve that.  CIccProfile::Write() emits
  // m_Header.profileID unconditionally as part of the header; the nWriteId switch only
  // decides whether CalcProfileID() then RECALCULATES it and rewrites offset 84.  So an
  // XML document that already carries a <ProfileID> -- which is exactly what iccToXml
  // emits for any v4 profile -- kept its original ID through `iccFromXml ... -noid`,
  // contradicting the README's promise that the option prevents writing one.  Measured
  // on a round trip: bytes 84-99 came back byte-identical to the source profile's ID.
  //
  // Clearing the header field here rather than changing icNeverWriteID's meaning: the
  // enum is public API used by other callers, and the report asks for library
  // compatibility to be considered separately (#2387).
  if (bNoId)
    memset(&profile.m_Header.profileID, 0, sizeof(profile.m_Header.profileID));

  std::string valid_report;

  if (profile.Validate(valid_report)<=icValidateWarning) {
    for (i=0; i<16; i++) {
      if (profile.m_Header.profileID.ID8[i])
        break;
    }
    if (SaveIccProfile(argv[2], &profile, bNoId ? icNeverWriteID : (i<16 ? icAlwaysWriteID : icVersionBasedID))) {
      printf("Profile parsed and saved correctly\n");
    }
    else {
      printf("Unable to save profile as '%s'\n", argv[2]);
      return EXIT_FAILURE;
    }
  }
  else {
    for (i=0; i<16; i++) {
      if (profile.m_Header.profileID.ID8[i])
        break;
    }
    if (SaveIccProfile(argv[2], &profile, bNoId ? icNeverWriteID : (i<16 ? icAlwaysWriteID : icVersionBasedID))) {
      printf("Profile parsed.  Profile is invalid, but saved correctly\n");
    }
    else {
      printf("Unable to save profile - profile is invalid!\n");
      return EXIT_FAILURE;
    }
    printf("%s", valid_report.c_str());
  }

  printf("\n");
  return EXIT_SUCCESS;
}
