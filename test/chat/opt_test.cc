#include "src/chat/opt.hh"

#include <cassert>
#include <cstring>
#include <iostream>

#include <fildesh/ostream.hh>

#include "src/chat/opt_schema.hh"

static
  void
chat_prefixes_parse_test()
{
  rendezllama::ChatOptions opt;
  FildeshX in[1];
  bool all_good;

  *in = FildeshX_of_strlit(
      "((chat_prefixes) \
      \"{{user}}:\" \
      \"{{char}} feels:\" \
      \"{{char}} wants:\" \
      \"{{char}} plans:\" \
      \"{{char}}:\" \
      )");
  all_good = rendezllama::slurp_sxpb_dynamic_options_close_FildeshX(in, opt);
  assert(all_good);
  assert(opt.message_opts.size() == 5);
  for (const auto& message_opt : opt.message_opts) {
    assert(!message_opt.given_prefix.empty());
    assert(!message_opt.prefix.empty());
  }
  opt.protagonist = "User";
  opt.protagonist_alias = "{{user}}";
  opt.confidant_alias = "{{char}}";

  in->off = 0;
  in->size = 0;
  *in = FildeshX_of_strlit("(confidant \"Char\")");
  all_good = rendezllama::slurp_sxpb_dynamic_options_close_FildeshX(in, opt);
  assert(all_good);
  assert(opt.message_opts.size() == 5);
  assert(opt.message_opts[0].prefix == "User:");
  assert(opt.message_opts[1].prefix == "Char feels:");
  assert(opt.message_opts[2].prefix == "Char wants:");
  assert(opt.message_opts[3].prefix == "Char plans:");
  assert(opt.message_opts[4].prefix == "Char:");
}

static
  void
sentence_terminals_parse_test()
{
  rendezllama::ChatOptions opt;
  FildeshX in[1];
  *in = FildeshX_of_strlit(
      "(sentence_terminals () \
      \"\\n\" \
      \"\\\"\" \
      \".\" \
      )");
  bool all_good = rendezllama::slurp_sxpb_dynamic_options_close_FildeshX(in, opt);
  assert(all_good);
  assert(opt.sentence_terminals.size() == 3);
  // Insert 3 and expect that they add nothing new.
  opt.sentence_terminals.insert("\n");
  opt.sentence_terminals.insert("\"");
  opt.sentence_terminals.insert(".");
  assert(opt.sentence_terminals.size() == 3);
}

static
  void
substitution_parse_test()
{
  rendezllama::ChatOptions opt;
  FildeshX in[1];
  *in = FildeshX_of_strlit(
      "(substitution (special_tokens (()) (() (name <|im_start|>))))");
  bool all_good = rendezllama::slurp_sxpb_options_close_FildeshX(
      in, opt, rendezllama::options_sxproto_schema(), "");
  assert(all_good);
  assert(opt.special_tokens.size() == 1);
  assert(opt.special_tokens[0].candidates.size() == 1);
  assert(opt.special_tokens[0].alias == "<|im_start|>");
  assert(opt.special_tokens[0].candidates[0] == "<|im_start|>");

  opt.special_tokens.clear();
  *in = FildeshX_of_strlit(
      "(substitution\n"
      " (bos_token_alias <bos_token>)\n"
      " (eos_token_alias <eos_token>)\n"
      " (special_tokens (())\n"
      "  (() (alias <|im_start|>) (candidates (()) <bos_token>))\n"
      "  (() (alias <|im_end|>) (candidates (()) <eos_token>))\n"
      "))");
  all_good = rendezllama::slurp_sxpb_options_close_FildeshX(
      in, opt, rendezllama::options_sxproto_schema(), "");
  assert(all_good);
  assert(opt.special_tokens.size() == 2);
  assert(opt.special_tokens[0].candidates.size() == 1);
  assert(opt.special_tokens[1].candidates.size() == 1);
  assert(opt.special_tokens[0].alias == "<|im_start|>");
  assert(opt.special_tokens[1].alias == "<|im_end|>");
  assert(opt.special_tokens[0].candidates[0] == "<bos_token>");
  assert(opt.special_tokens[1].candidates[0] == "<eos_token>");
}

int main()
{
  chat_prefixes_parse_test();
  sentence_terminals_parse_test();
  substitution_parse_test();
  return 0;
}
