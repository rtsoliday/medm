# QtEDM Expression Channels

An Expression Channel is a QtEDM-only logical widget that evaluates an EPICS
calc expression and publishes the numeric result as a process-local soft PV.
Use it for display-local derived values, thresholds, fan-out signals, and small
calculation chains that do not justify adding IOC-side logic.

Expression Channels are visible and selectable in EDIT mode. In EXECUTE mode
they become invisible, subscribe to their configured inputs, and publish their
results for other widgets in the same QtEDM process.

## Create and connect an Expression Channel

1. Enter EDIT mode and choose **Object Palette > Monitors > Expression
   Channel**.
2. Draw the logical widget on the display.
3. Select it and configure its fields in the Resource Palette.
4. Give **Variable** a unique, explicit name, such as `expr:temperatureDelta`.
5. Enter an EPICS calc expression using `A` through `D`, then assign the
   corresponding source PVs to **Channel A** through **Channel D**.
6. Set another widget's normal channel field to the Variable name.
7. Enter EXECUTE mode. The consuming widget first receives **Initial Value**,
   then receives calculated updates according to **Event Signal**.

The published variable is not an IOC PV and is not visible to `caget`, `camonitor`,
or another QtEDM process. It can be consumed by other displays opened in the
same running QtEDM process, including embedded and tabbed displays.

## Resource Palette fields

| Field | Meaning |
| --- | --- |
| Foreground / Background | EDIT-mode appearance of the logical widget. The widget is hidden in EXECUTE mode. |
| Variable | Name of the process-local numeric soft PV that receives the result. A blank value creates a private `__expr_...` name; use an explicit name whenever another widget must subscribe. |
| Calc | EPICS calc expression. Inputs `A`, `B`, `C`, and `D` correspond to the four channel fields. QtEDM also accepts `==` and `!=` and normalizes them for the EPICS calc engine. |
| Channel A-D | Numeric source channels. Each may be a CA PV, a `pva://` PV, a supported provider channel, or another Expression Channel variable. Unused inputs default to zero. |
| Initial Value | Value published immediately when EXECUTE mode starts, before a successful calculation is published. |
| Event Signal | Determines which successful evaluations replace the published value; see the table below. |
| Precision | Saved numeric presentation setting. A consuming widget's own limits and precision determine how it renders the soft-PV value. |

Input channels are subscribed in real-time mode. A missing or nonnumeric value
is treated as zero for evaluation; a non-finite value is ignored. An invalid or
empty calc expression does not publish calculated updates, but the Initial
Value is still available.

## Event Signal behavior

| Resource Palette choice | ADL value | Publication rule |
| --- | --- | --- |
| Never | `never` | Publish only Initial Value; evaluate input changes without publishing their results. |
| On First Change | `onFirstChange` | Publish the first successful evaluation after startup, then hold that value. |
| On Any Change | `onAnyChange` | Publish whenever the calculated result differs from the last published value. This is the default. |
| Trigger 0->1 | `triggerZeroToOne` | Publish when consecutive evaluated results cross from `<= 0` to `> 0`. The first evaluation establishes the starting state and does not trigger. |
| Trigger 1->0 | `triggerOneToZero` | Publish when consecutive evaluated results cross from `> 0` to `<= 0`. The first evaluation establishes the starting state and does not trigger. |

The trigger modes detect a zero/nonzero boundary in the **calc result**; they
do not require that the expression return exactly 0 or 1.

## ADL example

This block publishes the sum of two IOC PVs as `expr:sum`:

```text
expression_channel {
  object {
    x=20
    y=66
    width=120
    height=40
  }
  variable="expr:sum"
  calc="A+B"
  channelA="source:one"
  channelB="source:two"
  channelC=""
  channelD=""
  initialValue=0
  eventSignal="onAnyChange"
  clr=30
  bclr=4
  precision=3
}
```

A consuming Text Monitor uses the result like an ordinary channel:

```text
"text update" {
  object {
    x=180
    y=78
    width=150
    height=16
  }
  monitor {
    chan="expr:sum"
    clr=14
    bclr=4
  }
}
```

Expression Channels may be chained. For example, set a second node's Channel A
to `expr:sum` and its Calc to `A*2`. QtEDM pre-registers declared outputs when
entering EXECUTE mode, so ordering across the loaded display tree is
deterministic. Use unique Variable names: two active producers with the same
name publish into the same registry entry and can overwrite each other.

## Troubleshooting

- **The consuming widget tries to connect through CA.** Confirm that its channel
  exactly matches the Expression Channel's Variable and that both displays are
  open in the same QtEDM process. Do not add a `ca://` or `pva://` prefix to the
  soft-PV name.
- **Only the Initial Value appears.** Check the calc syntax and source channel
  names, then confirm Event Signal is not **Never**. QtEDM logs invalid and empty
  calc expressions to standard error.
- **A trigger never fires.** Trigger modes need two successful evaluations and
  only publish when the result crosses the zero boundary in the configured
  direction.
- **A first-change value arrives sooner than expected.** Each input monitor event
  can cause an evaluation; On First Change does not wait for all four channels
  to connect. Design the expression and initial value with startup ordering in
  mind.
- **The value changes unexpectedly.** Search all loaded displays for duplicate
  Variable names and for calculation chains that feed the same name.
- **The variable cannot be inspected later.** A blank Variable is assigned a
  generated private name. Set an explicit name when another widget or the PV
  Information dialog must refer to it consistently.

## Reference fixture and validation

The maintained example is
[`tests/test_ExpressionChannel.adl`](../tests/test_ExpressionChannel.adl). It
covers direct publication, chaining, On First Change, and Trigger 0->1. Parser
coverage is in
[`qtedm/tests/test_adl_parser.cc`](tests/test_adl_parser.cc), and IOC cases are
listed in [`tests/qtedm_ioc_cases.json`](../tests/qtedm_ioc_cases.json).

From the repository root, run:

```bash
make test-qtedm-unit
make test-qtedm-ioc
```

For a focused manual check, open `tests/test_ExpressionChannel.adl` against the
repository test IOC, change `channelA_PV` through `channelD_PV`, and confirm the
four displayed soft-PV outputs follow the instructions printed on the display.
