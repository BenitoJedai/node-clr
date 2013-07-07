var assert = require('assert');
var clr = require('../lib/clr');

describe('clr', function () {
  describe('#init', function () {
    it('should success', function () {
      var ns = clr.init({ global: false });

      assert(ns);
    });
  });

  describe('namespaces', function () {
    it('should have nested namespaces', function () {
      var ns = clr.init({ global: false });

      assert(ns.System.IO);
      assert(ns.System.IO.Ports);
    });

    it('should have classes', function () {
      var ns = clr.init({ global: false });

      assert(ns.System.Console);
      assert(ns.System.IO.Stream);
    });
  });

  describe('classes', function () {
    it('should work as constructor', function () {
      var ns = clr.init({ global: false });

      var dt = new ns.System.DateTime();
      assert(dt);
      assert(clr.isCLRObject(dt));
    });

    // TODO: static event

    it('#static field getter should work', function () {
      var ns = clr.init({ global: false });

      var empty = ns.System.String.Empty;
      assert.strictEqual(empty, '');
    });

    // TODO: static field setter

    it('#static property getter should work', function () {
      var ns = clr.init({ global: false });

      var now = ns.System.DateTime.Now;
      assert(now);
      assert(clr.isCLRObject(now));
    });

    // TODO: static property setter

    it('#static method should work', function () {
      var ns = clr.init({ global: false });

      var r = ns.System.String.Format('Hello, {0}!', 'world');
      assert.strictEqual(r, 'Hello, world!');
    });
    
    it('#instance property getter should work', function () {
      var ns = clr.init({ global: false });

      var dt = new ns.System.DateTime(1970, 1, 1);
      assert.strictEqual(dt.Year, 1970);
    });

    it('#instance property setter should work', function () {
      var ns = clr.init({ global: false });

      var ex = new ns.System.Exception();
      ex.Source = 'here';

      assert.strictEqual(ex.Source, 'here');
    });

    it('#instance method should work', function () {
      var ns = clr.init({ global: false });

      var dt = new ns.System.DateTime(1970, 1, 1);

      assert.strictEqual(dt.ToString(), '1970/01/01 0:00:00');
    });
  });
  
  it('async callback should work', function (done) {
    var ns = clr.init({ global: false });

    var called = 0;
    var t = new ns.System.Threading.Tasks.Task(function () { called++; });
    t.Start();

    setTimeout(function () {
      assert.strictEqual(called, 1);
      done();
    }, 100);
  });
});
