// render() for hydration: each key's text, list and part is between
// <!--[--> and <!--]-->, so the browser can find it in the HTML, and so
// that text stays a node of its own.
export const target = 'ssr';
export const hydrate = true;

export const source = `
machine item {
  input title

  initial state shown {}

  view {
    <li>{title}</li>
  }
}

machine page {
  initial state editing {
    rename => assign(name) => editing
    add => append(items, spawn(item) {
      title = event.data
    }) => editing
    save => saving
  }

  state saving {
    done => editing
    view(status) {
      <p>Saving</p>
    }
  }

  view {
    <h1>Hi {name}!</h1>
    {part(status)}
    <ul>{items}</ul>
  }
}
`;

export default function run({ mod, assert }) {
  const page = mod.createPage().start();
  assert.equal(page.render(), '<h1>Hi <!--[--><!--]-->!</h1><!--[--><!--]--><ul><!--[--><!--]--></ul>');
  page.send({ type: 'rename', data: 'Ada' });
  page.send({ type: 'add', data: 'milk' });
  page.send('save');
  assert.equal(page.render(),
    '<h1>Hi <!--[-->Ada<!--]-->!</h1><!--[--><p>Saving</p><!--]-->' +
    '<ul><!--[--><li><!--[-->milk<!--]--></li><!--]--></ul>');
}
