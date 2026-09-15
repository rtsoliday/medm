---
title: Documentation
description: Build QtEDM, open an EPICS display, and find the widget and ADL reference.
---

<div class="doc-eyebrow">EPICS DISPLAY MANAGER · QT 5 / QT 6</div>

# QtEDM documentation

<p class="doc-intro">Design control screens, monitor EPICS process variables, and bring existing MEDM displays into QtEDM.</p>

<div class="doc-paths">
  <a class="doc-path" href="./get-started/first-display.html"><span class="path-number">01 / LEARN</span><strong>Open your first display →</strong><span>Start with an ADL file and learn the edit and execute modes.</span></a>
  <a class="doc-path" href="./operate/modes.html"><span class="path-number">02 / OPERATE</span><strong>Work with displays →</strong><span>Navigate control screens and choose an operating mode.</span></a>
  <a class="doc-path" href="./configure/features.html"><span class="path-number">03 / CONFIGURE</span><strong>Connect your display →</strong><span>Use macros, dynamic attributes, related displays, and expression channels.</span></a>
  <a class="doc-path" href="./reference/widgets.html"><span class="path-number">04 / LOOK UP</span><strong>Find a widget or option →</strong><span>Browse illustrated widgets, command-line syntax, and ADL extensions.</span></a>
</div>

## Start with an ADL display

After [building QtEDM](./get-started/install), launch the editor from the repository root:

```sh
bin/Linux-x86_64/qtedm
```

To monitor an existing display, supply its path and choose execute mode:

```sh
bin/Linux-x86_64/qtedm -x path/to/display.adl
```

Replace the path with your own display. [Open your first display](./get-started/first-display) explains macros, display lookup, and read-only operation. Use the executable for your platform on macOS or Windows.

<div class="doc-note"><strong>Moving from MEDM?</strong> QtEDM reads the core ADL format and adds its own extensions. Review <a href="./understand/compatibility.html">compatibility with MEDM</a> before sharing extended displays with legacy clients.</div>

## Explore the widgets

![QtEDM Cartesian Plot widget examples](/images/qtedm-widgets/cartesian-plot.png)

The [widget catalog](./reference/widgets) covers graphics, monitors, controllers, plots, and QtEDM-specific objects. Each entry describes its purpose and configuration, with screenshots where available.

## Extend and maintain QtEDM

Read the [plugin API](./develop/plugins), run the [test suites](./develop/testing), or consult the [ADL reference](./reference/adl). The [standalone reference manual](./project/manuals) is also available offline through **Help > Overview** in QtEDM.
