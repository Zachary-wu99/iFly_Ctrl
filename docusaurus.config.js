// @ts-check

import {themes as prismThemes} from 'prism-react-renderer';

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'iFly_Ctrl 文档',
  tagline: 'STM32F405 飞控工程文档',
  favicon: 'img/favicon.ico',

  future: {
    v4: true,
  },

  url: 'https://zachary-wu99.github.io',
  baseUrl: '/iFly_Ctrl/',
  organizationName: 'zachary-wu99',
  projectName: 'iFly_Ctrl',
  deploymentBranch: 'gh-pages',
  onBrokenLinks: 'throw',

  i18n: {
    defaultLocale: 'zh-Hans',
    locales: ['zh-Hans'],
  },

  presets: [
    [
      'classic',
      ({
        docs: {
          routeBasePath: '/',
          sidebarPath: './sidebars.js',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  themeConfig:
    ({
      image: 'img/docusaurus-social-card.jpg',
      colorMode: {
        defaultMode: 'light',
        disableSwitch: true,
        respectPrefersColorScheme: false,
      },
      docs: {
        sidebar: {
          autoCollapseCategories: true,
          hideable: true,
        },
      },
      navbar: {
        title: 'iFly_Ctrl',
        logo: {
          alt: 'iFly_Ctrl Logo',
          src: 'img/logo.svg',
        },
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'docsSidebar',
            position: 'left',
            label: '工程文档',
          },
        ],
      },
      footer: {
        style: 'light',
        links: [
          {
            title: '文档',
            items: [
              {
                label: '工程简介',
                to: '/',
              },
            ],
          },
          {
            title: '目录',
            items: [
              {
                label: '上手开发',
                to: '/getting-started/app-overview',
              },
              {
                label: '底层实现',
                to: '/implementation/lib-overview',
              },
            ],
          },
        ],
        copyright: `Copyright © ${new Date().getFullYear()} iFly_Ctrl Docs`,
      },
      prism: {
        theme: prismThemes.github,
        darkTheme: prismThemes.dracula,
      },
    }),
};

export default config;
