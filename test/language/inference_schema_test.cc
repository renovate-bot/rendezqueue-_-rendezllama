#include "src/language/inference_schema.hh"

#include <cassert>

#include <fildesh/ostream.hh>
#include <fildesh/string.hh>

#include "src/chat/opt.hh"
#include "src/chat/opt_schema.hh"

using rendezllama::slurp_sxpb_dynamic_options_close_FildeshX;
using rendezllama::inference::AdjustViaKind;
using rendezllama::inference::Sampling;

static
  void
default_parse_test()
{
  rendezllama::ChatOptions opt;
  {
    FildeshX in = FildeshX_of_strlit("(language (substitution))");
    bool all_good = slurp_sxpb_dynamic_options_close_FildeshX(&in, opt);
    assert(all_good);
  }
  assert(std::holds_alternative<Sampling>(opt.infer_via));
  const auto& sampling = std::get<Sampling>(opt.infer_via);

  const auto& adjust_thru = sampling.adjust_thru;
  assert(adjust_thru.size() == 2);
  auto* min_p = std::get_if<AdjustViaKind::min_p>(&adjust_thru[0]);
  assert(min_p);
  assert(*min_p == 0.1f);
  auto* temperature = std::get_if<AdjustViaKind::temperature>(&adjust_thru[1]);
  assert(temperature);
  assert(*temperature == 0.8f);

  using rendezllama::inference::Probability;
  assert(std::holds_alternative<Probability>(sampling.pick_via));
}

static
  void
seed_parse_test()
{
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

static
  void
adjust_thru_parse_test()
{
  fildesh::ostringstream oss;
  oss
    << "(language\n"
    << " ((infer_via sampling)\n"
    << "  (adjust_thru (())\n"
    << "   (dry\n"
    << "    (multiplier 0.5)\n"
    << "    (base 0.25)\n"
    << "    (allowed_length 100)\n"
    << "    (window_length 1000)\n"
    << "   )\n"
    << "   (min_p 0.25)\n"
    << "   (penalize_with\n"
    << "    (window_length 1000)\n"
    << "    (repetition 1.5)\n"
    << "    (frequency 0.5)\n"
    << "    (presence 0.25)\n"
    << "   )\n"
    << "   (top_k 123)\n"
    << "   (top_p 0.75)\n"
    << "   (typical_p 0.5)\n"
    << "   (xtc (probability 0.75) (threshold 0.25))\n"
    << "   (temperature 0.75)\n"
    << ")))";

  rendezllama::ChatOptions opt;
  {
    FildeshX in = getslice_FildeshO(oss.c_struct());
    bool all_good = slurp_sxpb_dynamic_options_close_FildeshX(&in, opt);
    assert(all_good);
  }
  assert(std::holds_alternative<Sampling>(opt.infer_via));
  const auto& adjust_thru = std::get<Sampling>(opt.infer_via).adjust_thru;
  unsigned adjust_thru_idx = 0;

  auto* dry = std::get_if<AdjustViaKind::dry>(
      &adjust_thru[adjust_thru_idx++]);
  assert(dry);
  assert(dry->multiplier == 0.5);
  assert(dry->base == 0.25);
  assert(dry->allowed_length == 100);
  assert(dry->window_length == 1000);

  auto* min_p = std::get_if<AdjustViaKind::min_p>(
      &adjust_thru[adjust_thru_idx++]);
  assert(min_p);
  assert(*min_p == 0.25);

  auto* penalize_with = std::get_if<AdjustViaKind::penalize_with>(
      &adjust_thru[adjust_thru_idx++]);
  assert(penalize_with);
  assert(penalize_with->window_length == 1000);
  assert(penalize_with->repetition == 1.5);
  assert(penalize_with->frequency == 0.5);
  assert(penalize_with->presence == 0.25);

  auto* top_k = std::get_if<AdjustViaKind::top_k>(
      &adjust_thru[adjust_thru_idx++]);
  assert(top_k);
  assert(*top_k == 123);

  auto* top_p = std::get_if<AdjustViaKind::top_p>(
      &adjust_thru[adjust_thru_idx++]);
  assert(top_p);
  assert(*top_p == 0.75);

  auto* typical_p = std::get_if<AdjustViaKind::typical_p>(
      &adjust_thru[adjust_thru_idx++]);
  assert(typical_p);
  assert(*typical_p == 0.5);

  auto* xtc = std::get_if<AdjustViaKind::xtc>(
      &adjust_thru[adjust_thru_idx++]);
  assert(xtc);
  assert(xtc->probability == 0.75);
  assert(xtc->threshold == 0.25);

  auto* temperature = std::get_if<AdjustViaKind::temperature>(
      &adjust_thru[adjust_thru_idx++]);
  assert(temperature);
  assert(*temperature == 0.75);

  assert(adjust_thru.size() == adjust_thru_idx);
}

static
  void
pick_via_parse_test()
{
  using rendezllama::inference::Mirostat;
  using rendezllama::inference::Probability;

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

int main()
{
  default_parse_test();
  seed_parse_test();
  adjust_thru_parse_test();
  pick_via_parse_test();
  return 0;
}
