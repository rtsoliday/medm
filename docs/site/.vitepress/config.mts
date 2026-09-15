import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'QtEDM',
  description: 'Guides and reference for designing, operating, and extending EPICS displays with QtEDM.',
  lang: 'en-US',
  base: process.env.DOCS_BASE || '/',
  outDir: './dist',
  srcExclude: ['public/**'],
  cleanUrls: false,
  head: [['meta', { name: 'theme-color', content: '#126384' }]],
  themeConfig: {
    siteTitle: 'QtEDM / docs',
    search: { provider: 'local' },
    outline: { level: [2, 3], label: 'On this page' },
    nav: [
      { text: 'Guide', link: '/get-started/first-display' },
      { text: 'Reference', link: '/reference/widgets' },
      { text: 'Development', link: '/develop/plugins' },
      { text: 'Repository', link: 'https://github.com/rtsoliday/medm' }
    ],
    sidebar: [
      { text: 'GET STARTED', items: [
        { text: 'Overview', link: '/' },
        { text: 'Install & build', link: '/get-started/install' },
        { text: 'Requirements', link: '/get-started/requirements' },
        { text: 'Open your first display', link: '/get-started/first-display' }
      ]},
      { text: 'OPERATE & CONFIGURE', collapsed: false, items: [
        { text: 'Edit & execute displays', link: '/operate/modes' },
        { text: 'Macros & dynamic behavior', link: '/configure/features' },
        { text: 'Expression channels', link: '/configure/expressions' },
        { text: 'Sessions & snapshots', link: '/operate/sessions' }
      ]},
      { text: 'REFERENCE', collapsed: false, items: [
        { text: 'Widget catalog', link: '/reference/widgets' },
        { text: 'Command-line options', link: '/reference/command-line' },
        { text: 'ADL files & extensions', link: '/reference/adl' },
        { text: 'Environment variables', link: '/reference/environment' }
      ]},
      { text: 'UNDERSTAND & DEVELOP', collapsed: true, items: [
        { text: 'About QtEDM', link: '/understand/overview' },
        { text: 'Relationship to MEDM', link: '/understand/medm' },
        { text: 'Compatibility', link: '/understand/compatibility' },
        { text: 'Build reference', link: '/get-started/build-reference' },
        { text: 'Plugin API', link: '/develop/plugins' },
        { text: 'Tests & contributions', link: '/develop/testing' },
        { text: 'Maintain these docs', link: '/develop/documentation' }
      ]},
      { text: 'PROJECT', collapsed: true, items: [
        { text: 'Authors', link: '/project/authors' },
        { text: 'Technical support', link: '/project/support' },
        { text: 'Copyright', link: '/project/copyright' },
        { text: 'License', link: '/project/license' },
        { text: 'Standalone manuals', link: '/project/manuals' }
      ]}
    ]
  }
})
