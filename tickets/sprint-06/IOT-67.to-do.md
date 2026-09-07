# IOT-67: LLM incident report

**Sprint:** sprint-06
**Story points:** 5
**Status:** To Do
**Depends on:** IOT-66

## Story
As an operator, I want a written incident report so that I can understand an anomaly without reading raw blocks.

## Acceptance criteria
- [ ] Calls the Anthropic API with the structured findings and drafts markdown
- [ ] No LLM call when there are no findings
- [ ] API key from environment/secret, never committed
- [ ] Failures degrade to the raw findings rather than losing the alert
- [ ] A finding about a camera references its events, not footage — nothing is recorded
- [ ] Bounded token usage and a timeout
- [ ] Tests use a stubbed client, not the live API

## Implementation notes
- Uses the latest Claude model; pin the exact model id rather than an alias so a
  model change is a deliberate commit.
- Prompt gets the findings from IOT-66, not raw blocks — the model explains what
  was detected, it does not do the detecting.
- Block contents are attacker-influenced: a device controls `description` and
  `metadata`. Treat them as untrusted data in the prompt, never as instructions.
- An LLM failure must not swallow the alert. Ship the findings unadorned.
