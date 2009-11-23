#!/usr/bin/gsr -zzdd
const sh = require("shellalike");

function io_test_cat() {
  var cat = new sh.Process("cat|cat");
  print(cat);
  for each(var each in ['one', 'two', 'three'])
    cat.write(each+'\n');
  cat.flush();
  for (var x in cat) print(x.trim());
}
function io_test_close() {
  var cat = new sh.Process("cat|cat");
  print(cat);
  for each(var each in ['one', 'two', 'three'])
    cat.write(each+'\n');
  cat.close();
  for (var x in cat) print(x.trim());
}
function io_test_tail() {
  var log = sh.exec("tail /var/log/auth.log").exec("rot13");
  for (var line in log)
    print(line.trim());
}
function test_out_file() {
  sh.exec("echo SUCCESS > EVER").exec();
}
function test_no_op() {
  // TODO make work!
  var noop = sh.exec("echo FAILURE > TEST");
}
function test_writing() {
  var rot13 = sh.exec("");
}
function test_graph() {
  var x = sh.exec("one").exec("two").exec(function(src){for(var each in src)yield '+++'+each+'+++'});
  print('@@', x.pipeline());
  //sh.ExecAPI.pipeline(x);
}
function test1() {
  var x = new sh.Digraph;
  var el1 = x.addElement({'name':'alice','toString':function()this.name});
  var el2 = x.addElement({'name':'bob','toString':function()this.name});
  print('added two elements:');
  print('\t', el1);
  print('\t', el2);
  var snk1 = el1.addPad('snk', {'name':'chloe','toString':function()this.name});
  var src1 = el1.addPad('src', {'name':'danielle','toString':function()this.name});
  var snk2 = el2.addPad('snk', {'name':'eleanor','toString':function()this.name});
  var src2 = el2.addPad('src', {'name':'francesca','toString':function()this.name});
  print('added four pads:');
  print('\t', snk1);
  print('\t', src1);
  print('\t', snk2);
  print('\t', src2);
  print('cycles:', x.isCyclic());
  print('linear:', x.isLinear());
  snk2.link(src1);
  print('linked two of the pads:');
  print('\t', snk1);
  print('\t', src1);
  print('\t', snk2);
  print('\t', src2);
  print('cycles:', x.isCyclic());
  print('linear:', x.isLinear());
  snk1.link(src2);
  print('linked the final two pads:');
  print('\t', snk1);
  print('\t', src1);
  print('\t', snk2);
  print('\t', src2);
  print('cycles:', x.isCyclic());
  print('linear:', x.isLinear());
}

try {
  test1();
  //test_out_file();
  //test_graph();
  //io_test_close();
  print("done.");
} catch (e) {
  print('\nuncaught exception\n', e, '\n\n', 'backtrace:\n', e.stack);
  //throw(-1);
}
//print(sh.ExecAPI.splice(sh.exec('tail -f /var/log/syslog'), sh.exec('cat > whatever')));
