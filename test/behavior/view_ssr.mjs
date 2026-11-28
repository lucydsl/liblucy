// The ssr target: a machine's view is the HTML that render() returns,
// read from the machine's state and context when it's called. Text is
// escaped, bound attributes are written as they would be set, each machine
// of a list renders its own view, and a state's part is there only while
// the machine is in the state, or in one of its children.
export const target = 'ssr';

export const source = `
machine item {
  input title

  initial state shown {
    check => assign(done) => shown
  }

  view {
    <li data-done={done}><input type="checkbox" checked={done}>{title}</li>
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

    machine progress {
      initial state sending {
        sent => waiting
      }
      state waiting {}
    }

    view(status) {
      <p class="status" title={name}>Saving {name}</p>
    }
  }

  view {
    <h1 title="\${x}">\`Hi\` \${name}</h1>
    <input type="text" value={name} on:input={rename}>
    {part(status)}
    <ul>{items}</ul>
  }
}
`;

export default function run({ mod, assert }) {
  const page = mod.createPage().start();
  assert.equal(typeof page.render, 'function');
  assert.equal(page.dom, undefined, 'no DOM on a server');

  // Nothing assigned yet: empty text and an empty value, and no part.
  assert.equal(page.render(),
    '<h1 title="${x}">`Hi` $</h1><input type="text" value=""><ul></ul>');

  // Text and attributes are escaped.
  page.send({ type: 'rename', data: '<Ada & "Bo">' });
  assert.equal(page.render(),
    '<h1 title="${x}">`Hi` $&#60;Ada &#38; &#34;Bo&#34;&#62;</h1>' +
    '<input type="text" value="&#60;Ada &#38; &#34;Bo&#34;&#62;"><ul></ul>');

  // Each machine in a list renders its own view; a bound attribute is
  // written for true and left out for undefined.
  page.send({ type: 'rename', data: 'Ada' });
  page.send({ type: 'add', data: 'milk' });
  page.send({ type: 'add', data: 'eggs' });
  page.context.items[1].send({ type: 'check', data: true });
  assert.equal(page.render(),
    '<h1 title="${x}">`Hi` $Ada</h1><input type="text" value="Ada">' +
    '<ul><li><input type="checkbox">milk</li><li data-done><input type="checkbox" checked>eggs</li></ul>');

  // The part is there in the state's children, and gone when it's left.
  const status = '<p class="status" title="Ada">Saving Ada</p>';
  page.send('save');
  assert.equal(page.state, 'saving.sending');
  assert.ok(page.render().includes('value="Ada">' + status + '<ul>'), 'the part is in its place');
  page.send('sent');
  assert.equal(page.state, 'saving.waiting');
  assert.ok(page.render().includes(status));
  page.send('done');
  assert.ok(!page.render().includes('status'));

  // Nothing of the DOM is in the module.
  const text = mod.createPage.toString() + mod.createItem.toString();
  assert.ok(!/document|addEventListener|cloneNode/.test(text));
}
