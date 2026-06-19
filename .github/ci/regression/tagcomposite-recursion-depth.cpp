// Regression for #1437: uncontrolled recursion / stack-exhaustion DoS in the
// composite tag read path (CWE-674).
//
// CIccTagArray::Read and CIccTagStruct::Read each create their element tags and
// call pTag->Read() on them. An element may itself be a composite tag, so the
// two functions mutually recurse (an array of structs of arrays of ...) -- one
// native C++ stack frame per nesting level, ~24 attacker bytes/level. Before the
// fix a deeply-nested tagArrayType / tagStructType chain crashed the process
// (stack-overflow at IccTagComposite.cpp:1350); under ASan this surfaces as a
// "stack-overflow" finding, which fails this gate.
//
// After the fix a shared CompositeDepthGuard (cap 30) makes both Read paths
// return false at the cap without recursing into stack exhaustion, while a
// legitimate shallow chain still reads successfully. This helper feeds crafted
// blobs straight into the Read methods via CIccMemIO -- the same call the
// profile loader makes -- and verifies:
//   1. a shallow (depth 5) tagArrayType chain reads successfully (no false reject)
//   2. a deeply-nested (depth 200000) tagArrayType chain is rejected, no crash
//   3. a deeply-nested (depth 200000) tagStructType chain is rejected, no crash
//
// Returns 0 on success, non-zero on any failure.

#include "IccTagComposite.h"
#include "IccIO.h"

#include <cstdio>
#include <vector>

static const icUInt32Number kTagArrayType  = 0x74617279; // 'tary'
static const icUInt32Number kTagStructType = 0x74737472; // 'tstr'

static void be32(std::vector<icUInt8Number> &v, icUInt32Number x)
{
  v.push_back((icUInt8Number)((x >> 24) & 0xff));
  v.push_back((icUInt8Number)((x >> 16) & 0xff));
  v.push_back((icUInt8Number)((x >> 8) & 0xff));
  v.push_back((icUInt8Number)(x & 0xff));
}

// Linear chain of `depth` nested tagArrayType tags: each level is a 1-element
// array whose single element is the next level; the innermost is a 0-count
// terminator. Each wrapper level is 24 bytes (16-byte header + one 8-byte
// position entry pointing at offset 24); the terminator is 20 bytes.
static std::vector<icUInt8Number> buildArrayChain(int depth)
{
  const icUInt32Number total = 24u * (icUInt32Number)(depth - 1) + 20u;
  std::vector<icUInt8Number> v;
  v.reserve(total);
  for (int i = 0; i < depth - 1; ++i) {
    icUInt32Number remaining = total - (icUInt32Number)i * 24u - 24u;
    be32(v, kTagArrayType);  // sig
    be32(v, 0);              // reserved
    be32(v, 0);              // array type
    be32(v, 1);              // count
    be32(v, 24);             // element offset (relative to this tag start)
    be32(v, remaining);      // element size (the child subtree)
  }
  be32(v, kTagArrayType);    // terminator: 0-count array
  be32(v, 0);
  be32(v, 0);
  be32(v, 0);                // count = 0
  be32(v, 0);                // pad to the 20-byte header bound
  return v;
}

// Linear chain of `depth` nested tagStructType tags: each level is a 1-element
// struct whose single element is the next level; the innermost is a 0-count
// terminator. Each wrapper level is 28 bytes (16-byte header + one 12-byte tag
// dir entry pointing at offset 28); the terminator is 16 bytes.
static std::vector<icUInt8Number> buildStructChain(int depth)
{
  const icUInt32Number total = 28u * (icUInt32Number)(depth - 1) + 16u;
  std::vector<icUInt8Number> v;
  v.reserve(total);
  for (int i = 0; i < depth - 1; ++i) {
    icUInt32Number remaining = total - (icUInt32Number)i * 28u - 28u;
    be32(v, kTagStructType); // sig
    be32(v, 0);              // reserved
    be32(v, 0);              // struct type
    be32(v, 1);              // count
    be32(v, kTagStructType); // dir entry: element type signature
    be32(v, 28);             // dir entry: element offset (relative to tag start)
    be32(v, remaining);      // dir entry: element size (the child subtree)
  }
  be32(v, kTagStructType);   // terminator: 0-count struct
  be32(v, 0);
  be32(v, 0);
  be32(v, 0);                // count = 0
  return v;
}

static bool readArray(std::vector<icUInt8Number> &data, bool &ok)
{
  CIccMemIO io;
  if (!io.Attach(data.data(), data.size()))
    return false;
  CIccTagArray tag;
  ok = tag.Read((icUInt32Number)data.size(), &io);
  return true;
}

static bool readStruct(std::vector<icUInt8Number> &data, bool &ok)
{
  CIccMemIO io;
  if (!io.Attach(data.data(), data.size()))
    return false;
  CIccTagStruct tag;
  ok = tag.Read((icUInt32Number)data.size(), &io);
  return true;
}

int main()
{
  const int kDeep = 200000; // far beyond the depth cap; pre-fix this overflows the stack
  bool ok = false;

  // 1. Legitimate shallow array must still read.
  std::vector<icUInt8Number> shallow = buildArrayChain(5);
  if (!readArray(shallow, ok)) {
    std::printf("FAIL: could not attach shallow array blob\n");
    return 1;
  }
  if (!ok) {
    std::printf("FAIL: legitimate depth-5 tagArrayType chain was rejected\n");
    return 1;
  }

  // 2. Deeply-nested array must be rejected at the cap, not crash.
  std::vector<icUInt8Number> deepArr = buildArrayChain(kDeep);
  if (!readArray(deepArr, ok)) {
    std::printf("FAIL: could not attach deep array blob\n");
    return 1;
  }
  if (ok) {
    std::printf("FAIL: depth-%d tagArrayType chain was accepted (depth cap missing)\n", kDeep);
    return 1;
  }

  // 3. Deeply-nested struct must be rejected at the cap, not crash.
  std::vector<icUInt8Number> deepStruct = buildStructChain(kDeep);
  if (!readStruct(deepStruct, ok)) {
    std::printf("FAIL: could not attach deep struct blob\n");
    return 1;
  }
  if (ok) {
    std::printf("FAIL: depth-%d tagStructType chain was accepted (depth cap missing)\n", kDeep);
    return 1;
  }

  std::printf("composite tag recursion-depth guard OK\n");
  return 0;
}
