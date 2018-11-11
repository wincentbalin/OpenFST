// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
//  Encode transducer labels and/or weights.

#include <cstring>

#include <memory>
#include <string>

#include <fst/script/decode.h>
#include <fst/script/encode.h>
#include <fst/script/getters.h>

/// EncodeMain specific flag definitions
DEFINE_bool(encode_labels, false, "Encode output labels");
DEFINE_bool(encode_weights, false, "Encode weights");
DEFINE_bool(encode_reuse, false, "Re-use existing codex");
DEFINE_bool(decode, false, "Decode labels and/or weights");

int main(int argc, char **argv) {
  namespace s = fst::script;
  using fst::script::FstClass;
  using fst::script::MutableFstClass;

  string usage = "Encodes transducer labels and/or weights.\n\n  Usage: ";
  usage += argv[0];
  usage += " in.fst codex [out.fst]\n";

  std::set_new_handler(FailedNewHandler);
  SET_FLAGS(usage.c_str(), &argc, &argv, true);
  if (argc < 3 || argc > 4) {
    ShowUsage();
    return 1;
  }

  const string in_name = (strcmp(argv[1], "-") != 0) ? argv[1] : "";
  const string codex_name = argv[2];
  const string out_name = argc > 3 ? argv[3] : "";

  std::unique_ptr<MutableFstClass> fst(MutableFstClass::Read(in_name, true));
  if (!fst) return 1;

  if (FLAGS_decode) {
    s::Decode(fst.get(), codex_name);
    fst->Write(out_name);
  } else {
    const auto flags =
        s::GetEncodeFlags(FLAGS_encode_labels, FLAGS_encode_weights);
    s::Encode(fst.get(), flags, FLAGS_encode_reuse, codex_name);
    fst->Write(out_name);
  }

  return 0;
}
