#! /usr/bin/gsr -dd

const Curses = require("curses");
const VM = require("vm");

Curses.Window.prototype.writeln = function()
{
  var args = Array.prototype.join.call(arguments, ' ');
  var cpos;

  if (args && args.length)
  {
    this.write(args);
    cpos = this.getXY();
    cpos.x = 0;
    cpos.y++;
    this.gotoXY(cpos);
  }
}

Curses.Window.prototype.lnwrite = function()
{
  var args = Array.prototype.join.call(arguments, ' ');
  var lines;
  var i;

  if (!args || !args.length)
    return;

  if (this.cursorPosition.x != 0)
    this.write("\n");

  lines = args.split('\n');
  for (i=0; i < lines.length - 1; i++)
  {
    this.write(lines[i] + '\n');
  }

  this.write(lines[i]);
}

const rootWin		= new Curses.Window();
const inputAreaHeight   = (rootWin.height * 0.15) + 3;
const resultAreaHeight	= (rootWin.height * 0.07) + 3;

const inputPane  = new Pane(rootWin, 0, rootWin.height - inputAreaHeight, rootWin.width, inputAreaHeight);
const resultPane = new Pane(rootWin, 0, inputPane.y - resultAreaHeight, Math.floor(rootWin.width / 3), resultAreaHeight);
const errorPane  = new Pane(rootWin, resultPane.x + resultPane.width, resultPane.y, rootWin.width - resultPane.width, resultAreaHeight);
const outputPane = new Pane(rootWin, 0, 0, rootWin.width, resultPane.y);

function Pane(parentWin, x, y, width, height)
{
  this.edge   = new Curses.Window(parentWin, x, y, width, height);
  this.inner  = new Curses.Window(this.edge, 1, 1, width - 2, height - 2);

  this.height = this.edge.height;
  this.width  = this.edge.width;
  this.x      = this.edge.x;
  this.y      = this.edge.y;

  /** Status *********************************/
  this.status = "";
  function statusDisplay(edge, status)
  {
    if (status.length)
      edge.writeXY(edge.width - (4 + status.length), edge.height - 1,'<',status,'>');
  }
  function statusSetter(newStatus, redrawBox)
  {
    if (!this.border || (this.border == true))
      this.edge.drawBorder();
    this._status = newStatus;
    statusDisplay(this.edge, this._status);
    if (this._title)
      titleDisplay(this.edge, this._title);
  }
  this.__defineSetter__("status", statusSetter);
  this.__defineGetter__("status", function(){return this._status});

  /** Title *********************************/
  this.title = "";
  function titleDisplay(edge, title)
  {
    if (title.length)
      edge.writeXY(Math.floor(edge.width/2 - (title.length + 2)/2), 0, '[',title,']');
  }
  function titleSetter(newTitle, redrawBox)
  {
    if (!this.border || (this.border == true))
      this.edge.drawBorder();
    this._title = newTitle;
    titleDisplay(this.edge, this._title);
    if (this._status)
      statusDisplay(this.edge, this._status);
  }
  this.__defineSetter__("title", titleSetter);
  this.__defineGetter__("title", function(){return this._title});
}

var inputWin = inputPane.inner;
var outputWin = outputPane.inner;

outputWin.scrolling = true;
inputWin.scrolling = true;

inputPane.title = "Input Area";
inputPane.status = "Ready";
resultPane.title = "Results";
errorPane.title = "Errors";
outputPane.title = "Output";

function tryRequireMozShell(print)
{
  var MozShell;

  try {
    MozShell = require("mozshell");
  }
  catch(e) {
    print("Exception thrown loading mozshell module: " + e);
    print("");
    print("**** Mozilla JS Shell commands will not be available for this session, and");
    print("**** your code may be evaluated in the wrong context.");
    print("");

    MozShell =
    {
      evalcx: function(code, sandbox)
              {
                with(sandbox)
                {
                  return eval(code);
                }
              }
    };
  }
  finally 
  {
    MozShell.quit = function() { System.exit(0); };
  }

  return MozShell;
}

function main()
{
  var inputModule = require("repl/input");
  var history = new inputModule.History();
  var clipboard = new Array();
  var buffer = "";
  var prompt;
  var pendingHistory;
  var MozShell;

  function print()
  {
    var window = outputWin;

    window.write(Array.prototype.join.call(arguments, ' '));
    window.cursorPosition.x = 0;
    window.cursorPosition.y++;
  }

  MozShell = tryRequireMozShell(print);

  MozShell.debugPrint = function()
  {
    var argv = Array.prototype.join.call(arguments, ' ').split('\n');
    var i;

    for (i=0; i < argv.length; i++)
      outputWin.lnwrite(argv[i]);
  }

  MozShell.errorPrint = function()
  {
    var argv = Array.prototype.join.call(arguments, ' ').split('\n');
    var i;

    for (i=0; i < argv.length; i++)
      errorPane.inner.lnwrite(argv[i]);
  }

  var sandbox = new Object();	/* Note: Sandbox FAR from perfect */
  sandbox.print = print;
  sandbox.MozShell = MozShell;

  var line;

  for(;;)
  {
    inputPane.status = "Insert";
    prompt = "js> ";

    pendingHistory = new Array();

    do
    {
      if (pendingHistory.length)
	pendingHistory = pendingHistory.concat(" ");

      if ((line = inputModule.readline(inputWin, history, clipboard, prompt)) == null)
	return;
      pendingHistory = pendingHistory.concat(line);
      buffer = pendingHistory.join('');

      prompt = "  > ";
    } while (!VM.isCompilableUnit(buffer));

    if (pendingHistory.length)
      history.push(pendingHistory);
    pendingHistory = new Array();

    try	
    {
      outputPane.status = "Executing"
      buffer = 'with (MozShell) {' + buffer + '}';	   

      resultPane.inner.lnwrite(MozShell.evalcx(buffer, sandbox));
      /* resultPane.inner.lnwrite(eval(buffer)); */
      /* resultPane.inner.lnwrite(MozShell.evalcx(MozShell, buffer)); */
      if (MozShell.print) 
        MozShell.print(); /* flush scanRedirCaches */
    } 
    catch(e) 
    {
      var msg;

      if (typeof(e) == "string")
	msg = e;
      else
      if (e.message)
	msg = e.message;
      else
	msg = "Caught unknown exception";

      errorPane.inner.lnwrite(msg);
    }
    finally
    {
      outputPane.status = "";
      buffer = "";
    }
  }

  for (var el in sandbox)
    print(sandbox[el]);
}

main();



