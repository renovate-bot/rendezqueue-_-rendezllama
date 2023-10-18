#include "src/language/vocabulary.hh"

#include <cassert>

#include <fildesh/string.hh>

#include "llama.h"

using rendezllama::Vocabulary;


static void size_test() {
  assert(sizeof(llama_token) == sizeof(Vocabulary::Token_id));
}


static void tokenize_test(const char* model_filename)
{
  llama_model_params model_params = llama_model_default_params();
  model_params.vocab_only = true;
  llama_model* model = llama_load_model_from_file(model_filename, model_params);
  assert(model);
  llama_context_params ctx_params = llama_context_default_params();
  llama_context* ctx = llama_new_context_with_model(model, ctx_params);
  assert(ctx);

  const rendezllama::Vocabulary vocabulary(ctx);
  // Should have a large vocabulary. Many more than 64 different tokens.
  assert(vocabulary.cardinality() > 64);

  std::string s = "The quick brown fox jumps over the lazy dog.\n";
  std::vector<Vocabulary::Token_id> tokens;
  vocabulary.tokenize_to(tokens, s);
  assert(!tokens.empty());
  assert(tokens.back() == vocabulary.newline_token_id());
  assert(vocabulary.last_char_of(tokens.back()) == '\n');

  fildesh::ostringstream oss;
  for (auto token_id : tokens) {
    vocabulary.detokenize_to(oss, token_id);
  }
  assert(oss.view() == s);

  llama_free(ctx);
  llama_free_model(model);
}


int main(int argc, char** argv)
{
  assert(argc == 2 && "need model filename");
  size_test();
  rendezllama::GlobalScope rendezllama_global_scope;
  tokenize_test(argv[1]);
  return 0;
}
