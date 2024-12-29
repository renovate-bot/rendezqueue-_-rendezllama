#include "src/language/inference_schema.hh"

#include <cstdio>
#include <string_view>

static FildeshSxprotoField mirostat_fields[] = {
  {"version", FILL_FildeshSxprotoField_INT(1, 2)},
  {"tau", FILL_DEFAULT_FildeshSxprotoField_FLOAT},
  {"eta", FILL_DEFAULT_FildeshSxprotoField_FLOAT},
};

static FildeshSxprotoField probability_fields[] = {
  {"none", FILL_DEFAULT_FildeshSxprotoField_STRING},
};

static FildeshSxprotoField pick_via_oneof[] = {
  {"mirostat", FILL_FildeshSxprotoField_MESSAGE(mirostat_fields)},
  {"probability", FILL_FildeshSxprotoField_MESSAGE(probability_fields)},
};

static FildeshSxprotoField sampling_fields[] = {
  {"pick_via", FILL_FildeshSxprotoField_LONEOF(pick_via_oneof)},
  {"seed", FILL_FildeshSxprotoField_INT(0, INT_MAX)},
};

static FildeshSxprotoField infer_via_oneof[] = {
  {"sampling", FILL_FildeshSxprotoField_MESSAGE(sampling_fields)},
};

const FildeshSxprotoField* rendezllama::inference_sxproto_schema() {
  DECLARE_TOPLEVEL_FildeshSxprotoField(schema, infer_via_oneof);
  if (!schema->name) {
    lone_toplevel_initialization_FildeshSxprotoField(schema);
    schema->kind = FildeshSxprotoFieldKind_LONEOF;
  }
  return schema;
}

  bool
rendezllama::inference::populate_PickVia(
    rendezllama::inference::PickVia& pick_via,
    const FildeshSxpb* sxpb,
    FildeshSxpbIT it)
{
  const FildeshSxpbIT mirostat_it = lookup_subfield_at_FildeshSxpb(sxpb, it, "mirostat");
  if (!nullish_FildeshSxpbIT(mirostat_it)) {
    rendezllama::inference::Mirostat mirostat;
    if (!lone_subfield_at_FildeshSxpb_to_unsigned(&mirostat.version, sxpb, mirostat_it, "version")) {
      mirostat.version = 2;
    }
    lone_subfield_at_FildeshSxpb_to_float(&mirostat.tau, sxpb, mirostat_it, "tau");
    lone_subfield_at_FildeshSxpb_to_float(&mirostat.eta, sxpb, mirostat_it, "eta");
    pick_via = mirostat;
    return true;
  }
  else {
    rendezllama::inference::Probability probability;
    pick_via = probability;
    return true;
  }
  return false;
}

  bool
rendezllama::inference::populate_InferVia(
    InferVia& infer_via,
    FildeshSxpb* sxpb,
    FildeshSxpbIT it)
{
  if (nullish_FildeshSxpbIT(it)) {
    return false;
  }
  const FildeshSxpbIT sampling_it = lookup_subfield_at_FildeshSxpb(sxpb, it, "sampling");
  Sampling sampling;
  if (!nullish_FildeshSxpbIT(sampling_it)) {
    unsigned seed = 0;
    if (lone_subfield_at_FildeshSxpb_to_unsigned(&seed, sxpb, sampling_it, "seed")) {
      sampling.seed = static_cast<int>(INT_MAX & seed);
    }

    FildeshSxpbIT pick_it = lookup_subfield_at_FildeshSxpb(sxpb, sampling_it, "pick_via");
    if (!nullish_FildeshSxpbIT(pick_it)) {
      populate_PickVia(sampling.pick_via, sxpb, pick_it);
    }
    else {
      Probability probability;
      sampling.pick_via = probability;
    }

    infer_via = sampling;
    return true;
  }
  return false;
}
