#include "src/language/inference.hh"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <thread>

#include <fildesh/fildesh.h>
#include <fildesh/ostream.hh>

#include "src/chat/display.hh"
#include "src/chat/guide.hh"
#include "src/chat/opt.hh"
#include "src/chat/trajectory.hh"
#include "src/language/vocabulary.hh"

using rendezllama::ChatDisplay;
using rendezllama::ChatGuide;
using rendezllama::ChatOptions;
using rendezllama::ChatTrajectory;
using rendezllama::Inference;
using rendezllama::Vocabulary;
using rendezllama::inference::AdjustViaKind;

Inference::Inference(const Vocabulary& vocabulary)
  : vocabulary_(vocabulary)
{}
Inference::~Inference() {
  if (smpl_) {llama_sampler_free(smpl_);}
}

  const std::string&
rendezllama::antiprompt_suffix(
    std::string_view text,
    const std::set<std::string>& antiprompts)
{
  static const std::string empty_string;
  for (const std::string& s : antiprompts) {
    if (text.size() >= s.size()) {
      const size_t offset = text.size() - s.size();
      if (0 == memcmp(&text[offset], &s[0], s.size())) {
        return s;
      }
    }
  }
  return empty_string;
}

static bool maybe_trim_endspace(std::string& s)
{
  bool result = false;
  while (!s.empty() && s.back() == ' ') {
    s.pop_back();
    result = true;
  }
  return result;
}

  void
rendezllama::augment_tokenize_chat_input(
    ChatGuide& chat_guide,
    ChatTrajectory& chat_traj,
    bool& prevent_subsequent_newline,
    std::string s,
    const Vocabulary& vocabulary,
    const ChatOptions& opt)
{
  prevent_subsequent_newline = false;
  if (s.size() >= 2 && s[0] == '\\' && s[1] == 'n') {
    chat_guide.end_turn();
    chat_guide.begin_turn(opt.message_opts.size()-1);
    s.erase(0, 2);
    prevent_subsequent_newline = maybe_trim_endspace(s);
    if (opt.message_opts.back().prefix.back() == '\n' && opt.linespace_on) {
      if (!s.empty() && s.front() != ' ') {
        s.insert(0, " ");
      }
    }
    chat_traj.tokenize_append(s, vocabulary);
  }
  else if (s.front() == '\n') {
    // This is from /yield.
    chat_guide.yield_turn(s.substr(1));
  }
  else if (s.front() == ' ') {
    prevent_subsequent_newline = maybe_trim_endspace(s);
    chat_traj.tokenize_append(s, vocabulary);
  }
  else {
    chat_guide.yield_turn(0);
    if (opt.message_opts[0].prefix.back() == '\n' && opt.linespace_on) {
      if (!s.empty() && s.front() != ' ') {
        s.insert(0, " ");
      }
    }
    chat_traj.tokenize_append(s, vocabulary);
    chat_guide.yield_turn();
    chat_traj.display_token_count_ = chat_traj.rfind_message_prefix_begin_at(
        chat_traj.token_count()-1);
    prevent_subsequent_newline = true;
  }
}

  std::tuple<struct llama_model*, struct llama_context*>
rendezllama::make_llama_context(rendezllama::ChatOptions& opt)
{
  llama_model_params model_params = llama_model_default_params();
  model_params.use_mlock = opt.mlock_on;
  model_params.use_mmap = opt.mmap_on;

  struct llama_model* model = llama_model_load_from_file(
      opt.model_filename.c_str(), model_params);
  if (!model) {
    fildesh_log_error("Failed to open model.");
    return std::make_tuple(nullptr, nullptr);
  }

  if (opt.model_token_limit == 0) {
    opt.model_token_limit = llama_model_n_ctx_train(model);
  }
  if (opt.context_token_limit == 0) {
    opt.context_token_limit = opt.model_token_limit;
  }

  model_params = llama_model_default_params();
  model_params.use_mlock = opt.mlock_on;
  model_params.use_mmap = opt.mmap_on;

  llama_context_params ctx_params = llama_context_default_params();
  ctx_params.n_ctx = opt.context_token_limit;
  ctx_params.n_threads = opt.thread_count;
  ctx_params.n_batch = opt.batch_count;
  ctx_params.rope_freq_scale = llama_model_rope_freq_scale_train(model);
  assert(ctx_params.rope_freq_scale > 0.0);
  while (
      (unsigned)(opt.model_token_limit / ctx_params.rope_freq_scale)
      <
      opt.context_token_limit)
  {
    ctx_params.rope_freq_scale /= 2;
  }

  struct llama_context* ctx = llama_init_from_model(model, ctx_params);
  if (!ctx) {
    llama_model_free(model);
    fildesh_log_error("Failed to create context.");
    return std::make_tuple(nullptr, nullptr);
  }
  return std::make_tuple(model, ctx);
}

static
  int
new_sampling_seed()
{
  return static_cast<int>(INT_MAX & time(NULL));
}

static
  void
apply_sampler_chain(
    struct llama_sampler* smpl,
    const rendezllama::inference::AdjustVia& adjust_via,
    const struct llama_model* model,
    unsigned seed,
    std::ostream& eout)
{
  const unsigned keep_one = 1;

  if (const auto* dry = std::get_if<AdjustViaKind::dry>(&adjust_via)) {
    static const char* seq_breakers[] = {
      "\n", ":",
    };
    llama_sampler_init_dry(
        llama_model_get_vocab(model),
        llama_model_n_ctx_train(model),
        dry->multiplier,
        dry->base,
        dry->allowed_length,
        dry->window_length,
        seq_breakers,
        sizeof(seq_breakers)/sizeof(*seq_breakers));
    eout << "dry:"
      << "\n  multiplier: " << dry->multiplier
      << "\n  base: " << dry->base
      << "\n  allowed_length: " << dry->allowed_length
      << "\n  window_length: " << dry->window_length
      << "\n";
  }
  if (const auto* min_p = std::get_if<AdjustViaKind::min_p>(&adjust_via)) {
    llama_sampler_chain_add(smpl, llama_sampler_init_min_p(*min_p, keep_one));
    eout << "min_p: " << *min_p << "\n";
  }
  if (const auto* penalize_with = std::get_if<AdjustViaKind::penalize_with>(&adjust_via)) {
    llama_sampler_init_penalties(
        penalize_with->window_length,
        penalize_with->repetition,
        penalize_with->frequency,
        penalize_with->presence);
    eout << "penalties:"
      << "\n  window_length: " << penalize_with->window_length
      << "\n  repetition: " << penalize_with->repetition
      << "\n  frequency: " << penalize_with->frequency
      << "\n  presence: " << penalize_with->presence
      << "\n";
  }
  if (const auto* temperature = std::get_if<AdjustViaKind::temperature>(&adjust_via)) {
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(*temperature));
    eout << "temperature: " << *temperature << "\n";
  }
  if (const auto* top_k = std::get_if<AdjustViaKind::top_k>(&adjust_via)) {
    llama_sampler_chain_add(smpl, llama_sampler_init_top_k(*top_k));
    eout << "top_k: " << *top_k << "\n";
  }
  if (const auto* top_p = std::get_if<AdjustViaKind::top_p>(&adjust_via)) {
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(*top_p, keep_one));
    eout << "top_p: " << *top_p << "\n";
  }
  if (const auto* typical_p = std::get_if<AdjustViaKind::typical_p>(&adjust_via)) {
    llama_sampler_chain_add(smpl, llama_sampler_init_typical(*typical_p, keep_one));
    eout << "typical_p: " << *typical_p << "\n";
  }
  if (const auto* xtc = std::get_if<AdjustViaKind::xtc>(&adjust_via)) {
    llama_sampler_chain_add(smpl, llama_sampler_init_xtc(xtc->probability, xtc->threshold, keep_one, seed));
    eout << "xtc: "
      << "\n  probability: " << xtc->probability
      << "\n  threshold: " << xtc->threshold
      << "\n";
  }
}

static
  void
mirostat_sample(
    struct llama_sampler* smpl,
    const rendezllama::inference::Mirostat& mirostat,
    unsigned seed,
    const rendezllama::Vocabulary& vocabulary)
{
  if (mirostat.version == 1) {
    const int mirostat_m = 100;
    llama_sampler_chain_add(
        smpl,
        llama_sampler_init_mirostat(
            vocabulary.cardinality(), seed,
            mirostat.tau, mirostat.eta, mirostat_m));
  }
  else if (mirostat.version == 2) {
    llama_sampler_chain_add(
        smpl,
        llama_sampler_init_mirostat_v2(
            seed, mirostat.tau, mirostat.eta));
  }
}

  void
Inference::reinitialize(const ChatOptions& opt, const struct llama_model* model)
{
  fildesh::ofstream eout("/dev/stderr");

  const auto* sampling = std::get_if<rendezllama::inference::Sampling>(&opt.infer_via);
  assert(sampling);
  auto seed = sampling->seed;
  if (smpl_ || seed < 0) {
    // We're retrying or just don't have a fixed seed, so we should reseed.
    seed = new_sampling_seed();
  }
  if (smpl_) {
    llama_sampler_free(smpl_);
    eout.open("/dev/null");
  }
  token_count_ = 0;
  auto smpl_param = llama_sampler_chain_default_params();
  smpl_ = llama_sampler_chain_init(smpl_param);

  for (const auto& adjust_via : sampling->adjust_thru) {
    apply_sampler_chain(smpl_, adjust_via, model, seed, eout);
  }

  if (const auto* mirostat = std::get_if<rendezllama::inference::Mirostat>(&sampling->pick_via)) {
    mirostat_sample(smpl_, *mirostat, seed, vocabulary_);
    eout << "mirostat:"
      << "\n  version: " << mirostat->version
      << "\n";
  }
  else {
    llama_sampler_chain_add(smpl_, llama_sampler_init_dist(seed));
  }
}

  bool
Inference::commit_to_context(
    struct llama_context* ctx,
    ChatDisplay& chat_disp,
    ChatTrajectory& chat_traj,
    const ChatOptions& opt,
    const llama_model* model)
{
  assert(!chat_traj.erased_since_eval_ ||
         chat_traj.context_token_count_ < chat_traj.token_count());
  if (chat_traj.context_token_count_ < chat_traj.token_count()) {
    this->reinitialize(opt, model);
  }
  if (chat_traj.context_token_count_ == chat_traj.token_count()) {
    return true;
  }

  chat_traj.maybe_rollforget_within_limit(opt.context_token_limit, vocabulary_);

  // Reset thread count just in case the user reconfigured it.
  const unsigned thread_count = opt.thread_count;
  unsigned batch_thread_count = opt.batch_thread_count;
  if (batch_thread_count == 0) {
    batch_thread_count = std::thread::hardware_concurrency();
  }
  if (batch_thread_count == 0) {
    batch_thread_count = thread_count;
  }
  llama_set_n_threads(ctx, thread_count, batch_thread_count);

  // Clear KV cache past current position just in case the user deleted tokens.
  llama_kv_cache_seq_rm(ctx, -1, chat_traj.context_token_count_, -1);

  while (chat_traj.context_token_count_ < chat_traj.token_count()) {
    const unsigned n = std::min(
        opt.batch_count,
        chat_traj.token_count() - chat_traj.context_token_count_);

#if LLAMA_OPENBLAS_ON
    if (n < 32) {
      llama_set_n_threads(ctx, thread_count, batch_thread_count);
    }
    else {
      llama_set_n_threads(ctx, thread_count, 1);
    }
#endif
    chat_disp.show_new(chat_traj.context_token_count_ + n, chat_traj, vocabulary_);

    llama_batch batch = llama_batch_get_one(
        const_cast<int*>(&chat_traj.tokens()[chat_traj.context_token_count_]),
        n);
    const int istat = llama_decode(ctx, batch);
    if (istat != 0) {
      fildesh_log_error("Failed to eval.");
      chat_traj.context_token_count_ = 0;
      return false;
    }
    else {
      chat_traj.context_token_count_ += n;
    }
  }
  assert(chat_traj.context_token_count_ == chat_traj.token_count());
  chat_traj.erased_since_eval_ = false;
  while (token_count_ < chat_traj.token_count()) {
    Vocabulary::Token_id token_id = chat_traj.token_at(token_count_);
    llama_sampler_accept(smpl_, token_id);
    token_count_ += 1;
  }
  return true;
}

  void
Inference::sample_to_trajectory(
    ChatTrajectory& chat_traj,
    struct llama_context* ctx,
    bool preventing_newline)
{
  float* logits = llama_get_logits(ctx);
  if (preventing_newline) {
    // Zero probability for message-ending tokens when requested.
    logits[vocabulary_.eos_token_id()] = 0;
    logits[vocabulary_.newline_token_id()] = 0;
  }

  std::vector<llama_token_data> candidates;
  candidates.resize(vocabulary_.cardinality());
  for (llama_token i = 0; i < (llama_token)candidates.size(); ++i) {
    candidates[i] = llama_token_data{
      i, logits[i], 0.0f,
    };
  }
  logits = NULL;
  llama_token_data_array candidates_data[1] = {{
    candidates.data(),
    candidates.size(),
    /*selected=*/0,
    /*sorted=*/false,
  }};
  llama_sampler_apply(smpl_, candidates_data);
  chat_traj.push_back(candidates[candidates_data->selected].id);
  llama_sampler_accept(smpl_, chat_traj.token());
  token_count_ += 1;
}

