#ifndef RENDEZLLAMA_LANGUAGE_INFERENCE_SCHEMA_HH_
#define RENDEZLLAMA_LANGUAGE_INFERENCE_SCHEMA_HH_

#include <variant>

#include <fildesh/sxproto.h>

namespace rendezllama {
namespace inference {

struct Mirostat {
  unsigned version = 2;
  float tau = 5.0f;
  float eta = 0.1f;
};

struct Probability {};

typedef std::variant<
  std::monostate,
  Mirostat,
  Probability
> PickVia;

struct Sampling {
  PickVia pick_via;
  int seed = -1;
};

typedef std::variant<
  std::monostate,
  Sampling
> InferVia;

bool
populate_PickVia(
    PickVia& pick_via,
    const FildeshSxpb* sxpb,
    FildeshSxpbIT it);
bool
populate_InferVia(
    InferVia& infer_via,
    FildeshSxpb* sxpb,
    FildeshSxpbIT it);

}  // namespace inference

const FildeshSxprotoField* inference_sxproto_schema();

}  // namespace rendezllama
#endif
