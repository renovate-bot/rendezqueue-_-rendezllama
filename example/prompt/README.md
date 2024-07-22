# Prompt Examples

In order of interest:
- [assistant_plain](assistant_plain/): AI assistant with single-line replies.
  - Minimial prompt that should work for any model.
- [roshambo_kira](roshambo_kira/): Play against a Kira in roshambo.
  - Demonstrates why LLMs are hard to get right.
- [confidant_alpaca](confidant_alpaca/): A camelid that occasionally spits.
  - Demonstrates a method of prompting instruction-tuned models to fill in character dialogue.
- Instruction-following AI assistants.
  - [assistant_alpaca](assistant_alpaca/): Alpaca prompt format.
  - [assistant_chatml](assistant_chatml/): ChatML prompt format that requires special `<|im_start|>` and `<|im_end|>` tokens.
  - [assistant_gemma](assistant_gemma/): Gemma prompt format requires special `<start_of_turn>` and `<end_of_turn>` tokens.
  - [assistant_mistral](assistant_mistral/): Mistral propmt format that requires special `[INST]` and `[/INST]` tokens.
- [assistant_vicuna](assistant_vicuna/): Conversational AI assistant.
  - Minimial prompt that lets a Vicuna-style model do its thing.
  - Only works with models that end the assistant's message with an EOS token.
- [assistant_coprocess](assistant_coprocess/): A simple assistant that can be controlled as a coprocess.
  - Demonstrates the `/puts` and `/gets` commands.

