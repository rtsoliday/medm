# Maintain these docs

QtEDM's website uses the same VitePress layout and APS framing as the QtALH documentation. The complete HTML reference remains the source for the manual embedded in QtEDM's **Help > Overview**.

## Build and preview

Install Node.js 22 or newer with npm and Python 3, then run from the repository root:

```sh
make docs
python3 -m http.server 8000 --directory docs/html --bind 127.0.0.1
```

Open `http://127.0.0.1:8000/`. Serve the generated directory over HTTP; opening the website with `file://` does not support its modules or local search. The standalone `docs/QtEDM.html` continues to work independently.

`make docs` installs locked dependencies when absent, imports the manual and supporting guides, builds the site, checks local links and anchors, and copies the validated output to `docs/html`. Qt and EPICS are not required for the documentation build.

## Choose the source to edit

| Content | Source |
| --- | --- |
| Widget, CLI, ADL, environment, and behavior reference | `docs/QtEDM.html` |
| Installation instructions | `README.md` |
| Plugin API | `docs/QtEDM_Plugin_API.md` |
| Expression channels | `qtedm/ExpressionChannelUsage.md` |
| License | `LICENSE` |
| Homepage and focused guides | Authored Markdown under `docs/site/` |
| Navigation and appearance | `docs/site/.vitepress/config.mts` and `.vitepress/theme/` |

Generated Markdown has a comment naming `scripts/sync-docs.py`. Edit its original source, not the generated page. Register new top-level manual sections in that script and their generated paths in `.gitignore`. Named manual anchors are retained, and cross-section links are rewritten to the correct web pages. Screenshots are copied from `docs/images/`; retain their original filenames and provenance.

Keep the embedded manual self-contained, using basic HTML and inline styles supported by `QTextBrowser`. Preserve its anchors and relative image paths, along with the resource entries in `qtedm/resources/help.qrc`. Rebuild the applications after changing the embedded manual.

## Shared theme

`Layout.vue` extends the default VitePress layout using `doc-before` and `doc-after` slots for institutional links and credits. The affiliation row scrolls with the document. Centralize colors, responsive spacing, and print rules in `custom.css`; retain dark-mode counterparts and use `withBase` for internal links in Vue components.

Check the homepage, operating guide, widget catalog, and reference tables on desktop and mobile, in both themes. Verify search, keyboard focus, sidebar navigation, and anchor links. Print rules hide navigation and institutional framing.

## Hosting paths

Copy the entire generated `docs/html` directory to a static web server. For a subdirectory, build with the exact URL prefix first:

```sh
make docs DOCS_BASE=/manuals/QtEDM/
```

Check an existing build with the matching prefix:

```sh
python3 scripts/build-docs.py --check-only --base /manuals/QtEDM/
```

Use `make docs DOCS_BASE=/` for a server's root. Build output is local; building does not publish a website.

## Cleanup

Commit authored pages, the theme, scripts, package manifest, and lockfile. Generated pages, copied assets, `docs/html`, and `docs/site/dist` are ignored.

`make docs-clean` removes generated documentation while preserving authored pages and dependencies. `make docs-distclean` also removes installed documentation dependencies. Run `make docs` to recreate the output.
