---
version: 1
roles:
  vega:
    model: anthropic/claude-opus-4-7
    variant: opus
    mode: primary
    description: "primary reasoning"
  altair:
    model: openai/gpt-5.4
    mode: primary
  orion:
    model: google/gemini-2.5-flash
    mode: subagent
  rigel:
    model: llamacpp/ayane
    mode: subagent
---
## vega
You are vega, a reasoning agent.

## altair
You are altair, a coding agent.
