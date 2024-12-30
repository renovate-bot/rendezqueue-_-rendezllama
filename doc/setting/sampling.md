# Sampling

We use an LLM to infer the next token via random sampling.

## Default
```lisp
(language
 ((infer_via sampling)
  (adjust_thru (())
   (min_p 0.1)
   (temperature 0.8)
  )
  ((pick_via probability))
))
```

## Control Randomization
```lisp
(language
 ((infer_via sampling)
  ; Random seed (default is time-based, different every run).
  (seed 1234)
  (adjust_thru (())
   ; Divide token probabilities by 0.95 before applying softmax.
   ; A temperature of 1.0 is the same as not applying a temperature.
   ; High values like this can yield nonsensical output,
   ; while low values yield more determinism (e.g., zero is deterministic),
   ; so it is a good idea to always specify a temperature.
   (temperature 0.95)
  )
))
```

## Penalize Repetition
```lisp
(language
 ((adjust_via sampling)
  ((adjust_thru)

   ; Reduce probability of repeating any of the most recent 1200 tokens.
   (penalize_with
    ; Penalizes the most recent 1200 tokens from being generated (default off, 0).
    (repeat_window 1200)
    ; How much to penalize repetition (default off, 1.0).
    (repetition 1.05)
    ; Frequency penalty (default off, 0.0).
    (frequency_penalty 0.1)
    ; Presence penalty (default off, 0.0).
    (presence_penalty 0.1)
   )

   ; "Don't Repeat Yourself".
   (dry
    (window_length 1200)
    (multiplier 0.8)
    (base 1.75)
    (allowed_length 2)
   )

   ; Apply a temperature.
   (temperature 0.7)
)))
```

## Exclude Outlying
```lisp
(language
 ((infer_via sampling)
  (adjust_thru (())
   ; Top-K. 1 makes sampling deterministic.
   (top_k 40)
   ; Top-P. 0.0 is a no-op.
   (top_p 0.9)
   ; Locally Typical cutoff. 1.0 is a no-op.
   (typical_p 0.9)
   ; Min-P. 1.0 is a no-op.
   ; Cut out tokens whose probability is less
   ; than 5% relative to the most probable token.
   (min_p 0.05)

   ; "Exclude Top Choices".
   (xtc
    ; Probability threshold (default is 0.15).
    (threshold 0.15)
    ; Probability of performing the exclusion on this pass (defult is 1.0).
    (probability 0.5)
   )

   ; Temperature is not as necessary when tokens are expcluded.
   ; It can be >1.0 when Min-P has already filtered out the nonsensical tokens.
   (temperature 1.2)
)))
```

## Pick Token
### Mirostat
Mirostat is an alternative method of selecting a token.

```lisp
(language
 ((infer_via sampling)
  ((pick_via mirostat)
   ; Use Mirostat version 2 (default is 2).
   (version 2)
   ; Target entropy (default is 5.0).
   (tau 5.0)
   ; Learning rate (default is 0.1).
   (eta 0.1)
)))
```
