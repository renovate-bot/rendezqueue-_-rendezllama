#include "trajectory.hh"

#include <algorithm>
#include <cassert>
#include <climits>

#include <fildesh/string.hh>

using rendezllama::ChatTrajectory;
using rendezllama::Vocabulary;

ChatTrajectory::ChatTrajectory(Token_id token_id) {
  token_ids_.push_back(token_id);
  message_prefix_ids_.push_back(this->not_a_message_prefix_id());
}

ChatTrajectory::~ChatTrajectory()
{
  close_FildeshO(this->transcript_out_);
}

  void
ChatTrajectory::push_back(Token_id token_id)
{
  token_ids_.push_back(token_id);
  message_prefix_ids_.push_back(this->not_a_message_prefix_id());
}

  void
ChatTrajectory::insert_all_at(
    size_type i, const std::vector<Token_id>& a)
{
  assert(i > 0);
  token_ids_.insert(token_ids_.begin() + i, a.begin(), a.end());
  message_prefix_ids_.insert(
      message_prefix_ids_.begin() + i,
      a.size(), this->not_a_message_prefix_id());
  if (i < display_token_count_) {
    display_token_count_ += a.size();
  }
}

static
  void
maybe_pop_for_rewrite(
    ChatTrajectory& trajectory,
    fildesh::ostringstream& oss,
    const Vocabulary& vocabulary)
{
  if (trajectory.priming_token_count() < trajectory.token_count()) {
    if (vocabulary.last_char_of(trajectory.token()) == ' ') {
      vocabulary.detokenize_to(oss, trajectory.token());
      trajectory.erase_all_at(trajectory.token_count() - 1);
    }
  }
}

  void
ChatTrajectory::tokenize_append(
    std::string_view s,
    const Vocabulary& vocabulary)
{
  fildesh::ostringstream oss;
  maybe_pop_for_rewrite(*this, oss, vocabulary);
  oss << s;

  std::vector<Token_id> tmp;
  vocabulary.tokenize_to(tmp, oss.view());
  this->insert_all_at(this->token_count(), tmp);
}

  void
ChatTrajectory::erase_range(size_type beg, size_type end)
{
  erased_since_eval_ = true;
  assert(beg <= end);
  token_ids_.erase(
      token_ids_.begin() + beg,
      token_ids_.begin() + end);
  if (context_token_count_ > beg) {
    context_token_count_ = beg;
  }
  if (context_token_count_ >= token_count()) {
    // The -1 is added to force an eval.
    context_token_count_ = token_count()-1;
  }
  message_prefix_ids_.erase(
      message_prefix_ids_.begin() + beg,
      message_prefix_ids_.begin() + end);
  if (beg < display_token_count_) {
    if (end < display_token_count_) {
      display_token_count_ -= (end - beg);
    }
    else {
      display_token_count_ = beg;
    }
  }
  message_prefix_id_ = last_message_prefix_id_at(this->token_count());
}

  void
ChatTrajectory::rollforget(size_type end, const Vocabulary& vocabulary)
{
  assert(end <= this->token_count());
  const size_type beg = priming_token_count_;
  if (transcript_out_) {
    for (size_type i = beg; i < end; ++i) {
      vocabulary.detokenize_to(transcript_out_, this->token_at(i));
    }
    flush_FildeshO(transcript_out_);
  }
  this->erase_range(beg, end);
}

/** Drop oldest lines in the rolling prompt while keeping the priming prompt.
 **/
  void
ChatTrajectory::maybe_rollforget_within_limit(
    size_type token_limit,
    const Vocabulary& vocabulary)
{
  if (this->token_count() < token_limit) {
    return;
  }
  const size_type ideal_rollforget_end = (
      this->token_count() - (token_limit - priming_token_count_) / 2);
  assert(ideal_rollforget_end > priming_token_count_);

  size_type end = ideal_rollforget_end;
  for (end = rfind_message_prefix_begin_at(end);
       end > priming_token_count_;
       end = rfind_message_prefix_begin_at(end-1))
  {
    if (message_prefix_ids_[end] == 0) {
      break;
    }
  }

  // If a good rollforget point wasn't found by looking before the ideal point,
  // then choose to roll past next newline.
  if (end == priming_token_count_) {
    end = this->find_token_at(
        ideal_rollforget_end - 1,
        vocabulary.newline_token_id());
    end = (end < this->token_count() ? end+1 : end);
  }

  this->rollforget(end, vocabulary);
  assert(this->token_count() <= token_limit);
}

  ChatTrajectory::size_type
ChatTrajectory::find_token_at(size_type i, Token_id id) const
{
  auto it = std::find(token_ids_.begin() + i, token_ids_.end(), id);
  return it - token_ids_.begin();
}

  ChatTrajectory::size_type
ChatTrajectory::rfind_token_at(size_type i, Token_id id) const
{
  auto it = (
      i < this->token_count()
      ? token_ids_.rbegin() + (this->token_count() - i - 1)
      : token_ids_.rbegin());
  it = std::find(it, token_ids_.rend(), id);
  return (
      it != token_ids_.rend()
      ? token_ids_.rend() - it - 1
      : this->token_count());
}

  void
ChatTrajectory::tokenize_append_message_prefix(
    unsigned id,
    std::string_view s,
    const Vocabulary& vocabulary)
{
  std::vector<Token_id> tmp;
  vocabulary.tokenize_to(tmp, s);
  size_t i = this->token_count();
  this->insert_all_at(this->token_count(), tmp);
  for (; i < this->token_count(); ++i) {
    message_prefix_ids_[i] = id;
  }
  message_prefix_id_ = id;
}

  bool
ChatTrajectory::endswith_nonempty(
    std::string_view suffix,
    const Vocabulary& vocabulary)
{
  assert(!suffix.empty());
  fildesh::ostringstream oss;
  std::string carry;
  size_type token_index = this->token_count();
  while (token_index > priming_token_count_ && carry.size() < suffix.size()) {
    token_index -= 1;
    vocabulary.detokenize_to(oss.c_struct(), this->token_at(token_index));
    carry.insert(0, oss.view());
    oss.truncate();
  }
  if (carry.size() >= suffix.size()) {
    if (carry.substr(carry.size()-suffix.size()) == suffix) {
      return true;
    }
  }
  return false;
}

  void
ChatTrajectory::trim_message_suffix(
    std::string_view suffix,
    const Vocabulary& vocabulary)
{
  auto pos = suffix.find_last_not_of(" \n");
  if (pos != std::string_view::npos) {
    suffix = suffix.substr(0, pos+1);
  }

  fildesh::ostringstream oss;
  while (this->token_count() > priming_token_count_) {
    size_type token_index = this->token_count()-1;
    if (oss.view().empty()) {
      Token_id token_id = this->token_at(token_index);
      if (token_id == vocabulary.newline_token_id() ||
          token_id == vocabulary.eos_token_id()) {
        this->erase_all_at(token_index);
        continue;
      }
      vocabulary.detokenize_to(oss.c_struct(), token_id);
    }
    else {
      token_index = this->token_count();
    }

    auto pos = oss.view().find_last_not_of(" \n");
    if (pos == std::string_view::npos) {
      oss.truncate();
      this->erase_all_at(token_index);
      continue;
    }

    std::string carry;
    if (pos+1 == oss.view().size() && token_index < this->token_count()) {
      token_index += 1;
    }
    else {
      carry = oss.view().substr(0, pos+1);
      this->erase_all_at(token_index);
    }
    oss.truncate();
    const size_t carry_rindex = carry.size();

    assert(token_index <= this->token_count());
    size_t sufficient_size = suffix.size();
    const std::string_view eos_token_alias = vocabulary.eos_token_alias();
    if (sufficient_size < eos_token_alias.size()) {
      sufficient_size = eos_token_alias.size();
    }
    while (token_index > priming_token_count_ && carry.size() < sufficient_size) {
      token_index -= 1;
      vocabulary.detokenize_to(oss.c_struct(), this->token_at(token_index));
      carry.insert(0, oss.view());
      oss.truncate();
    }
    if (!eos_token_alias.empty() && carry.size() >= eos_token_alias.size()) {
      size_t lhs_size = carry.size()-eos_token_alias.size();
      if (carry.substr(lhs_size) == eos_token_alias) {
        this->erase_all_at(token_index);
        oss << carry.substr(0, lhs_size);
        continue;
      }
    }
    if (!suffix.empty() &&
        carry.size() >= suffix.size())
    {
      size_t lhs_size = carry.size()-suffix.size();
      if (carry.substr(lhs_size) == suffix) {
        this->erase_all_at(token_index);
        oss << carry.substr(0, lhs_size);
        continue;
      }
    }
    oss << carry.substr(carry.size()-carry_rindex);
    break;
  }
  this->tokenize_append(oss.view(), vocabulary);
}

  void
ChatTrajectory::tokenize_append_message_suffix(
    std::string_view suffix,
    const Vocabulary& vocabulary)
{
  if (suffix.empty()) {
    suffix = "\n";
  }
  const size_type old_display_token_count = display_token_count_;
  this->trim_message_suffix(suffix, vocabulary);
  const bool display_move_on = (
      old_display_token_count >= this->token_count());
  this->tokenize_append(suffix, vocabulary);
  if (display_move_on) {
    display_token_count_ = this->token_count();
  }
}

  ChatTrajectory::size_type
ChatTrajectory::rfind_message_prefix_at(size_type i) const
{
  assert(i < this->token_count());
  assert(0 < priming_token_count_);
  while (i >= priming_token_count_) {
    if (message_prefix_ids_[i] != this->not_a_message_prefix_id()) {
      return i;
    }
    i -= 1;
  }
  assert(i == priming_token_count_-1);
  return priming_token_count_-1;
}

  ChatTrajectory::size_type
ChatTrajectory::rfind_message_prefix_begin_at(size_type i) const
{
  i = this->rfind_message_prefix_at(i);
  while (i > priming_token_count_) {
    if (message_prefix_ids_[i-1] != message_prefix_ids_[i]) {
      return i;
    }
    i -= 1;
  }
  if (i == priming_token_count_) {
    if (message_prefix_ids_[i] != this->not_a_message_prefix_id()) {
      return priming_token_count_;
    }
  }
  assert(i == priming_token_count_-1);
  return priming_token_count_-1;;
}

  ChatTrajectory::size_type
ChatTrajectory::rfind_last_message_prefix_end_at(size_type i) const
{
  size_type e;
  if (i < this->token_count()) {
    e = rfind_message_prefix_at(i);
  }
  else {
    e = rfind_message_prefix_at(this->token_count()-1);
  }

  if (e < i) {
    i = e;
  }
  else {
    i = rfind_message_prefix_begin_at(i);
    if (i >= priming_token_count_) {
      i = rfind_message_prefix_at(i-1);
    }
  }
  assert(i + 1 >= priming_token_count_);
  return i + 1;
}

  ChatTrajectory::size_type
ChatTrajectory::last_message_prefix_id_at(size_type i) const
{
  i = rfind_last_message_prefix_end_at(i);
  if (i <= priming_token_count_) {
    return this->not_a_message_prefix_id();
  }
  return message_prefix_ids_[i-1];
}

  void
ChatTrajectory::assign_range_message_prefix_id(
    message_prefix_id id,
    size_type beg, size_type end)
{
  for (size_type i = beg; i < end; ++i) {
    message_prefix_ids_[i] = id;
  }
  message_prefix_id_ = last_message_prefix_id_at(this->token_count());
}

