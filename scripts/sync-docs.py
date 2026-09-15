#!/usr/bin/env python3
"""Generate the web reference from the same manual embedded in QtEDM Help."""
from html import escape
from html.parser import HTMLParser
from pathlib import Path
import argparse
import posixpath
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'docs/site'
PUBLIC = SITE / 'public'
SECTIONS = {
    'Introduction': ('understand/overview', 'About QtEDM'),
    'Relationship': ('understand/medm', 'Relationship to MEDM'),
    'Requirements': ('get-started/requirements', 'Requirements'),
    'CommandLine': ('reference/command-line', 'Command-line options'),
    'Modes': ('operate/modes', 'Edit and execute displays'),
    'Widgets': ('reference/widgets', 'Widget catalog'),
    'Environment': ('reference/environment', 'Environment variables'),
    'ADLFiles': ('reference/adl', 'ADL files and extensions'),
    'Features': ('configure/features', 'Macros and dynamic behavior'),
    'Differences': ('understand/compatibility', 'Compatibility with MEDM'),
    'Building': ('get-started/build-reference', 'Build reference'),
    'Acknowledgments': ('project/authors', 'Authors and acknowledgments'),
    'TechSupport': ('project/support', 'Technical support'),
    'Copyright': ('project/copyright', 'Copyright'),
}
IMPORTS = {
    'develop/plugins': 'docs/QtEDM_Plugin_API.md',
    'configure/expressions': 'qtedm/ExpressionChannelUsage.md',
}
GENERATED = [route + '.md' for route, _ in SECTIONS.values()] + [
    route + '.md' for route in IMPORTS] + ['get-started/install.md', 'project/license.md']


class Node:
    def __init__(self, tag='', attrs=()):
        self.tag, self.attrs, self.children = tag, dict(attrs), []

    def text(self):
        return ''.join(c if isinstance(c, str) else c.text() for c in self.children)

    def walk(self):
        yield self
        for child in self.children:
            if isinstance(child, Node):
                yield from child.walk()


class Manual(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = Node()
        self.stack = [self.root]

    def handle_starttag(self, tag, attrs):
        node = Node(tag, attrs)
        self.stack[-1].children.append(node)
        if tag not in ('meta', 'img', 'br', 'hr', 'link', 'input'):
            self.stack.append(node)

    def handle_endtag(self, tag):
        if self.stack[-1].tag != tag:
            raise ValueError(f'Unbalanced manual HTML: {tag}')
        self.stack.pop()

    def handle_data(self, data):
        self.stack[-1].children.append(data)


def anchor(node):
    return next((n.attrs.get('name') or n.attrs.get('id') for n in node.walk()
                 if n.attrs.get('name') or n.attrs.get('id')), None)


def plain(text):
    # Escape Markdown punctuation as well as HTML/Vue delimiters in prose.
    return re.sub(r'([\\`*_\[\]{}])', r'\\\1', escape(text, quote=False))


class Renderer:
    def __init__(self, route, anchors):
        self.route, self.anchors = route, anchors

    def link(self, href):
        if href.startswith('#'):
            target = self.anchors[href[1:]]
            return ('#' + href[1:] if target == self.route else
                    posixpath.relpath(target + '.html', posixpath.dirname(self.route)) + href)
        return href

    def html(self, node):
        if isinstance(node, str):
            return escape(node, quote=False)
        attrs = dict(node.attrs)
        if 'href' in attrs:
            attrs['href'] = self.link(attrs['href'])
        attributes = ''.join(f' {k}="{escape(v or "", quote=True)}"' for k, v in attrs.items())
        body = ''.join(self.html(c) for c in node.children)
        return f'<{node.tag}{attributes}>' + body + f'</{node.tag}>'

    def render(self, node):
        if isinstance(node, str):
            return plain(node)
        tag = node.tag
        body = ''.join(self.render(c) for c in node.children)
        if tag in ('h2', 'h3', 'h4'):
            ident = anchor(node)
            suffix = ' {#' + ident + '}' if ident else ''
            return '\n\n' + '#' * (int(tag[1]) - 1) + ' ' + plain(node.text().strip()) + suffix + '\n\n'
        if tag == 'pre':
            return '\n\n```text\n' + node.text().strip('\n') + '\n```\n\n'
        if tag == 'code':
            return '`` ' + node.text() + ' ``'
        if tag in ('strong', 'em'):
            mark = '**' if tag == 'strong' else '*'
            return mark + body + mark
        if tag == 'a':
            return '[' + body + '](' + self.link(node.attrs['href']) + ')' if 'href' in node.attrs else ''
        if tag == 'img':
            return '\n\n![' + plain(node.attrs.get('alt', '')) + '](/' + node.attrs['src'] + ')\n\n'
        if tag == 'table':
            return '\n\n<div class="manual-table" v-pre>\n' + self.html(node) + '\n</div>\n\n'
        if tag == 'ul':
            return '\n\n' + '\n'.join('- ' + self.render(c).strip().replace('\n', '\n  ')
                                      for c in node.children if isinstance(c, Node)) + '\n\n'
        if tag == 'p':
            return '\n\n' + body.strip() + '\n\n'
        if tag == 'div' and node.attrs.get('class') == 'note':
            return '\n\n::: info\n' + body.strip() + '\n:::\n\n'
        return body


def write(route, content):
    if route + '.md' not in GENERATED:
        raise ValueError(f'Unregistered generated route: {route}')
    path = SITE / (route + '.md')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('<!-- Generated by scripts/sync-docs.py; edit the original source. -->\n\n' + content.strip() + '\n')


def clean(dependencies=False):
    paths = [SITE / p for p in GENERATED] + [PUBLIC / 'images', PUBLIC / 'downloads',
             PUBLIC / 'manual', SITE / 'dist', SITE / '.vitepress/cache', ROOT / 'docs/html']
    if dependencies:
        paths.append(SITE / 'node_modules')
    for path in paths:
        if path.is_file() or path.is_symlink():
            path.unlink()
        elif path.is_dir():
            shutil.rmtree(path)


def sync():
    # These directories contain generated copies only; authored assets remain
    # in docs/images and the original source paths.
    for name in ('images', 'downloads', 'manual'):
        path = PUBLIC / name
        if path.exists():
            shutil.rmtree(path)
    parser = Manual()
    parser.feed((ROOT / 'docs/QtEDM.html').read_text())
    body = next(n for n in parser.root.walk() if n.tag == 'body')
    sections, current = {}, None
    for node in body.children:
        if isinstance(node, Node) and node.tag == 'h2':
            current = anchor(node)
            if current not in SECTIONS and current != 'Contents':
                raise ValueError(f'Register new manual section: {current}')
            sections[current] = []
        if current in SECTIONS:
            sections[current].append(node)
    if set(SECTIONS) - sections.keys():
        raise ValueError('A required manual section is missing')
    anchors = {'Contents': 'index'}
    for key, nodes in sections.items():
        if key not in SECTIONS:
            continue
        for node in nodes:
            if isinstance(node, Node):
                for n in node.walk():
                    ident = n.attrs.get('name') or n.attrs.get('id')
                    if ident:
                        if ident in anchors:
                            raise ValueError(f'Duplicate manual anchor: {ident}')
                        anchors[ident] = SECTIONS[key][0]
    for key, (route, title) in SECTIONS.items():
        renderer = Renderer(route, anchors)
        content = ''.join(renderer.render(n) for n in sections[key][1:])
        write(route, '# ' + title + ' {#' + key + '}\n\n' + content)
    for route, source in IMPORTS.items():
        text = (ROOT / source).read_text()
        # Source-code links remain inspectable in a self-contained download tree.
        def linked(match):
            label, href = match.groups()
            if re.match(r'^[a-z]+:', href) or href.startswith('#'):
                return match.group(0)
            path = ((ROOT / source).parent / href).resolve()
            relative = path.relative_to(ROOT)
            dest = PUBLIC / 'downloads' / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dest)
            link = posixpath.relpath('downloads/' + relative.as_posix(), posixpath.dirname(route))
            return f'<a href="{link}" download>{escape(label.replace("`", ""))}</a>'
        write(route, re.sub(r'\[([^\]]+)\]\(([^)]+)\)', linked, text))
    readme = (ROOT / 'README.md').read_text()
    install = readme.split('### Prerequisites\n', 1)[1].split('### Run QtEDM\n', 1)[0]
    write('get-started/install', '# Install and build QtEDM\n\n## Prerequisites\n\n' + install.replace('### Build QtEDM', '## Build QtEDM'))
    write('project/license', '# License\n\n```text\n' + (ROOT / 'LICENSE').read_text().rstrip() + '\n```')
    shutil.copytree(ROOT / 'docs/images', PUBLIC / 'images', dirs_exist_ok=True)
    shutil.copytree(ROOT / 'docs/images', PUBLIC / 'manual/images', dirs_exist_ok=True)
    for name in ('QtEDM.html', 'QtEDM_Showcase.html', 'MEDM.html',
                 'QtEDM_Announcement.pdf', 'QtEDM_Announcement.md', 'QtEDM_Plugin_API.md'):
        shutil.copy2(ROOT / 'docs' / name, PUBLIC / 'manual' / name)
    print(f'Imported {len(SECTIONS)} manual sections and preserved {len(anchors) - 1} named anchors.')


if __name__ == '__main__':
    args = argparse.ArgumentParser(description=__doc__)
    group = args.add_mutually_exclusive_group()
    group.add_argument('--clean', action='store_true')
    group.add_argument('--distclean', action='store_true')
    options = args.parse_args()
    if options.clean or options.distclean:
        clean(options.distclean)
    else:
        sync()
