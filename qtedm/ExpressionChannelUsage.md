# QtEDM Expression Channels

## Overview

An `Expression Channel` is a logical QtEDM widget that evaluates an EPICS calc
expression over up to four input channels, then publishes the result as a
process-local soft PV. Other widgets can subscribe to that soft PV by using the
expression channel's `variable` name as their normal channel name.

In edit mode the widget is visible so it can be selected and configured. In
execute mode it becomes invisible and runs only as a calculation/publishing
node.

This is the QtEDM equivalent of caQtDM's `caCalc`.

## When To Use It

Use an expression channel when you need one of these patterns inside QtEDM
without adding IOC-side logic:

- derive a display value from one or more live PVs
- build a local threshold/trigger signal
- fan out one calculated result to several widgets
- chain local calculations together across widgets or displays in the same
  QtEDM process

## Create An Expression Channel

1. Open the Object Palette.
2. In the `Monitors` section, choose `Expression Channel`.
3. Drag out the widget in edit mode.
4. With the new widget selected, use the Resource Palette to configure:
   - `Variable`
   - `Calc`
   - `Channel A` through `Channel D`
   - `Initial Value`
   - `Event Signal`
   - `Foreground`, `Background`, and `Precision`
5. Add a consumer widget such as `Text Monitor`, `Meter`, or `Bar Monitor`.
6. Set that consumer widget's channel name to the expression channel's
   `Variable` value.
7. Enter execute mode.

## Resource Palette Fields

### Variable

The soft-PV name that other widgets subscribe to.

- Use an explicit name when another widget needs to reference the result.
- Names are process-local to the running QtEDM instance.
- If left empty, QtEDM generates a private name of the form
  `__expr_<uuid>`. That is useful for internal logic nodes, but it is not
  convenient for other widgets to reference manually.

### Calc

The EPICS calc expression. The implementation uses QtEDM's existing
`medm_calc.c` engine, so the expression syntax follows the usual EPICS calc
rules.

Examples:

- `A+B`
- `(A-B)*10`
- `A>5`
- `SIN(A)`
- `MAX(A,B)`

Input mapping:

- `A` -> `channelA`
- `B` -> `channelB`
- `C` -> `channelC`
- `D` -> `channelD`

If a channel is empty or has not provided a value yet, that input contributes
`0.0`.

### Channel A / B / C / D

These are the input PV names for the calc expression.

- They can be real CA/PVA channels.
- They can also be other expression-channel variable names, which allows
  chaining.
- Empty inputs are allowed.

### Initial Value

The value published immediately when execute mode starts, before the first
successful calc evaluation. This keeps downstream widgets connected to a known
value during startup.

### Event Signal

Controls when a newly evaluated result is published.

- `Never`: evaluate but never publish updates after startup.
- `On First Change`: publish only the first successful post-startup evaluation.
- `On Any Change`: publish each time the result changes. This is the default.
- `Trigger Zero To One`: publish only when the result crosses from `<= 0` to
  `> 0`.
- `Trigger One To Zero`: publish only when the result crosses from `> 0` to
  `<= 0`.

### Foreground / Background / Precision

These affect the edit-mode representation only. The expression channel is not a
visible operator widget in execute mode.

## Runtime Behavior

When a display enters execute mode:

- QtEDM resolves the output variable name.
- The soft PV is registered in the process-local soft-PV registry.
- `initialValue` is published immediately.
- QtEDM subscribes to channels `A` through `D`.
- Each input update re-evaluates the calc expression.
- If the `eventSignal` condition matches, QtEDM publishes the result to the
  soft PV.

Any widget that uses the expression channel's `variable` name as its own
channel can then receive that published value as if it were a normal PV.

This works across multiple displays in the same QtEDM process as long as they
are open in that same application instance.

## Minimal Workflow Example

Goal: show the sum of two source PVs in a text monitor.

1. Add an expression channel.
2. Set:
   - `Variable = local:sum`
   - `Calc = A+B`
   - `Channel A = src:pv1`
   - `Channel B = src:pv2`
   - `Event Signal = On Any Change`
3. Add a `Text Monitor`.
4. Set the text monitor channel to `local:sum`.
5. Enter execute mode.

The text monitor will now show the locally calculated sum.

## Chaining Example

You can feed one expression channel into another:

- Expression 1:
  - `Variable = expr:base`
  - `Calc = A+B`
- Expression 2:
  - `Variable = expr:chain`
  - `Calc = A*2`
  - `Channel A = expr:base`

Any downstream widget can subscribe to `expr:chain`.

## Trigger Example

To publish only when an input crosses a threshold upward:

- `Variable = expr:trigger_up`
- `Calc = A>5`
- `Channel A = some:pv`
- `Event Signal = Trigger Zero To One`

The output publishes only when the evaluated expression changes from false to
true.

## ADL Format

Expression channels are stored in `.adl` files with an
`expression_channel { ... }` block:

```adl
expression_channel {
  object {
    x=20
    y=66
    width=120
    height=40
  }
  variable="expr:sum"
  calc="A+B"
  channelA="channelA_PV"
  channelB="channelB_PV"
  initialValue=0
  eventSignal="onAnyChange"
  clr=30
  bclr=4
  precision=3
}
```

QtEDM supports create, load, save, copy, and paste for this block type.

Legacy `medm` does not implement expression channels. In practice the legacy
parser is expected to ignore the `expression_channel` block rather than execute
it.

## Testing And Reference Files

Use these files as the current reference set:

- Usage and design notes:
  - `qtedm/ExpressionChannelImplementationPlan.md`
  - `qtedm/ExpressionChannelUsage.md`
- Manual and IOC-backed harness:
  - `tests/test_ExpressionChannel.adl`
  - `tests/index.adl`
- Automated parser coverage:
  - `qtedm/tests/test_adl_parser.cc`
- Automated IOC smoke coverage:
  - `tests/qtedm_ioc_cases.json`

Example commands:

```bash
make -j4
CCACHE_DISABLE=1 make -C qtedm tests
cd qtedm
QT_QPA_PLATFORM=offscreen QTEDM_NOLOG=1 LC_ALL=C TZ=UTC HOME=/tmp \
  ./O.Linux-x86_64/test_adl_parser
cd ..
tests/run_qtedm_ioc_tests.sh \
  --case expression_channel_soft_pv_any_change \
  --case expression_channel_chained_soft_pv_update \
  --case expression_channel_on_first_change_latches \
  --case expression_channel_trigger_zero_to_one
```

## Troubleshooting

### The consumer widget never updates

Check these first:

- The consumer channel exactly matches the expression channel `variable`.
- The display is in execute mode.
- The calc expression is valid.
- The input channel names are correct.
- Both widgets are running in the same QtEDM process.

### The output name changes every run

The `variable` field is blank. Set an explicit variable name if another widget
needs a stable subscription target.

### Two expression channels interfere with each other

They likely share the same `variable` name. Variable names must be unique
within the running QtEDM process.

### A chained expression does not resolve

Make sure the upstream expression channel has a non-empty `variable` name and
that the downstream channel field references that exact name.
