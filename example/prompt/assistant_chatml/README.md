# ChatML Assistant

This example should be run with [ChatML](https://github.com/openai/openai-python/blob/main/chatml.md)-style models that are tuned to behave like an instruction-following assistant chatbot.

The model typically should have special `<|im_start|>` and `<|im_end|>` tokens, but `setting.sxpb` configures fallbacks that attempt to support any model.
Gemma is basically the same format but without a `system` role, so we specifically look for Gemma-style `<start_of_turn>` and `<end_of_turn>` tokens as fallbacks.
When no other special tokens are found, we fall back to using the BOS and EOS tokens that all models have.
This is how jondurbin's [bagel-7b-v0.1](https://huggingface.co/jondurbin/bagel-7b-v0.1) finetune supported ChatML, and other instruct-tuned models tend to figure it out.
