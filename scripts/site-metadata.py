#!/usr/bin/env python3
"""Keep social cards and canonical URLs complete on the maintained site pages."""
import html
import hashlib
from html.parser import HTMLParser
from pathlib import Path
import re

SITE = Path(__file__).resolve().parents[1] / 'docs/site'
PAGES = ['index', 'apps', 'modules', 'getting-started', 'docs', 'renderers', 'widgets']


class Head(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.title = ''
        self.description = ''
        self.in_title = False

    def handle_starttag(self, tag, attributes):
        values = dict(attributes)
        if tag == 'title': self.in_title = True
        if tag == 'meta' and values.get('name') == 'description':
            self.description = values.get('content', '')

    def handle_endtag(self, tag):
        if tag == 'title': self.in_title = False

    def handle_data(self, text):
        if self.in_title: self.title += text


def update():
    # Version each mascot icon so browsers replace the retired letter favicon.
    icons = [
        ('icon', 'image/x-icon', '16x16 32x32 48x48', 'favicon.ico'),
        ('icon', 'image/png', '32x32', 'favicon-32.png'),
        ('apple-touch-icon', 'image/png', '180x180', 'apple-touch-icon.png'),
    ]
    icon_tags = []
    for rel, mime, sizes, filename in icons:
        revision = hashlib.sha256((SITE / filename).read_bytes()).hexdigest()[:12]
        icon_tags.append(f'<link rel="{rel}" type="{mime}" sizes="{sizes}" href="/{filename}?v={revision}">')
    for name in PAGES:
        path = SITE / (name + '.html')
        source = path.read_text()
        head = Head(); head.feed(source.split('</head>', 1)[0])
        assert head.title and head.description, path
        url = 'https://kryonlabs.com/' + ('' if name == 'index' else name)
        fields = {
            'og:type': 'website', 'og:site_name': 'Kryon Labs', 'og:locale': 'en_US',
            'og:url': url, 'og:title': head.title, 'og:description': head.description,
            'og:image': 'https://kryonlabs.com/og.png', 'og:image:type': 'image/png',
            'og:image:width': '1200', 'og:image:height': '630',
            'og:image:alt': 'Kryon dragon beside the names of the Kryon UI modules and desktop apps.',
            'twitter:card': 'summary_large_image', 'twitter:title': head.title,
            'twitter:description': head.description, 'twitter:image': 'https://kryonlabs.com/og.png',
            'twitter:image:alt': 'Kryon dragon beside the names of the Kryon UI modules and desktop apps.',
        }
        source = re.sub(r'<meta\b[^>]*(?:property|name)=["\'](?:og:|twitter:)[^>]*>', '', source)
        source = re.sub(r'<link\b[^>]*rel=["\']canonical["\'][^>]*>', '', source)
        source = re.sub(r'<link\b[^>]*rel=[\"\'](?:icon|shortcut icon|apple-touch-icon)[\"\'][^>]*>', '', source)
        tags = icon_tags + [f'<link rel="canonical" href="{url}">']
        tags += [f'<meta {"property" if key.startswith("og:") else "name"}="{key}" content="{html.escape(value, quote=True)}">' for key, value in fields.items()]
        before, after = source.split('</head>', 1)
        before = '\n'.join(line.rstrip() for line in before.splitlines()).rstrip()
        source = before + '\n' + '\n'.join(tags) + '\n</head>' + after
        path.write_text(source)
    print('Updated social previews and canonical URLs on maintained site pages')


if __name__ == '__main__': update()
