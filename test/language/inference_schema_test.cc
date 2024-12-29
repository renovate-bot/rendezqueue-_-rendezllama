#include "src/language/inference_schema.hh"

#include <cassert>

#include <fildesh/ostream.hh>

#include "src/chat/opt.hh"
#include "src/chat/opt_schema.hh"

using rendezllama::slurp_sxpb_dynamic_options_close_FildeshX;

static
  void
inference_parse_test()
{
  using rendezllama::inference::Mirostat;
  using rendezllama::inference::Probability;
  using rendezllama::inference::Sampling;

  rendezllama::ChatOptions opt;
  FildeshX in[1];
  bool all_good;

  *in = FildeshX_of_strlit(
      "(language ((infer_via sampling) ((pick_via mirostat))))");
  all_good = slurp_sxpb_dynamic_options_close_FildeshX(in, opt);
  assert(all_good);
  assert(std::holds_alternative<Sampling>(opt.infer_via));
  auto& sampling = std::get<Sampling>(opt.infer_via);
  assert(std::holds_alternative<Mirostat>(sampling.pick_via));
  auto& mirostat = std::get<Mirostat>(sampling.pick_via);
  assert(mirostat.version == 2);

  *in = FildeshX_of_strlit(
      "(language ((infer_via sampling) ((pick_via mirostat) (version 1))))");
  all_good = slurp_sxpb_dynamic_options_close_FildeshX(in, opt);
  assert(all_good);
  assert(std::holds_alternative<Sampling>(opt.infer_via));
  sampling = std::get<Sampling>(opt.infer_via);
  assert(std::holds_alternative<Mirostat>(sampling.pick_via));
  mirostat = std::get<Mirostat>(sampling.pick_via);
  assert(mirostat.version == 1);

  *in = FildeshX_of_strlit(
      "(language ((infer_via sampling) ((pick_via probability))))");
  all_good = slurp_sxpb_dynamic_options_close_FildeshX(in, opt);
  assert(all_good);
  assert(std::holds_alternative<Sampling>(opt.infer_via));
  sampling = std::get<Sampling>(opt.infer_via);
  assert(std::holds_alternative<Probability>(sampling.pick_via));

  *in = FildeshX_of_strlit(
      "(language ((infer_via sampling)))");
  all_good = slurp_sxpb_dynamic_options_close_FildeshX(in, opt);
  assert(all_good);
  assert(std::holds_alternative<Sampling>(opt.infer_via));
  sampling = std::get<Sampling>(opt.infer_via);
  assert(std::holds_alternative<Probability>(sampling.pick_via));
}

static
  void
seed_parse_test()
{
  using rendezllama::inference::Sampling;

  rendezllama::ChatOptions opt;
  FildeshX in[1];
  bool all_good;

  *in = FildeshX_of_strlit(
      "(language ((infer_via sampling) (seed 123)))");
  all_good = slurp_sxpb_dynamic_options_close_FildeshX(in, opt);
  assert(all_good);
  assert(std::holds_alternative<Sampling>(opt.infer_via));
  auto& sampling = std::get<Sampling>(opt.infer_via);
  assert(sampling.seed == 123);
}

int main()
{
  inference_parse_test();
  seed_parse_test();
  return 0;
}
