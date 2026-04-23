// @ts-check

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  docsSidebar: [
    'intro',
    {
      type: 'category',
      label: '构建',
      items: ['build/index'],
    },
    {
      type: 'category',
      label: '上手开发',
      items: [
        'getting-started/app-overview',
        'getting-started/cli-parameter-pid',
        'getting-started/protocol-task-tick',
      ],
    },
    {
      type: 'category',
      label: '底层实现',
      items: [
        'implementation/lib-overview',
        'implementation/time-and-scheduling',
        'implementation/communication-stack',
      ],
    },
  ],
};

export default sidebars;
