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

function spaces(n)
{
  var a = "";

  while (n--)
    a += " ";

  return a;
}

function History()
{
  this.lines = new Array();
  this.position = 0;

  this.select = function(offset, oldLineBuffer)
  {
    var copy = [];
    var i;
    var send;

    switch(offset)
    {
      case false:
	this.position = this.lines.length;
	break;

      case true:
	this.position = 0;
	break;

      default:
	this.position += offset;
    }

    if (this.lost && (this.position >= this.lines.length))
    {
      if (this.lost)
      {
	send = this.lost;
	this.position = this.lines.length;
      }
    }
    else
    {
      if (this.position < 0)
	this.position = 0;
      else if (this.position > this.lines.length)
	this.position = this.lines.length;
    
      if (this.lines[this.position])
	send = [this.lines[this.position][i] for (i in this.lines[this.position])];
      else
	send = [];
    }

    return send;
  }

  this.replace = function(newLineBuffer)
  {
    this.lines[this.position] = newLineBuffer;
    return this.lines[this.position];
  }

  this.push = function(newLineBuffer)
  {
    var lastLine;

    /* Strange syntax because I don't know JS's shortcut boolean eval rules, and it's important -wg */
    do
    {
      if (this.length == 0)
	break;

      lastLine = this.lines[this.lines.length - 1];

      if (!lastLine)
	break;

      if (newLineBuffer.length != lastLine.length)
	break;

      if (newLineBuffer.join('') != lastLine.join(''))
	break;

      return; 

    } while(false);

    this.lines.push(newLineBuffer);
  }
}

function readline(window, history, clipboard, prompt, maxlen)
{
  var key;
  var inputBuffer;		/* array of character-strings to return as a string */
  var pos = 0;              	/* Position within inputBuffer which matches screen cursor */
  var insertMode = true;	/* false = overwrite, true = shift inputBuffer to right */
  var i;
  var gone;			/* temp: stuff we just deleted from inputBuffer */
  var mark;			/* temp: cursor point (x,y) */
  var newInput;			/* temp: stuff we're just about to add to inputBuffer */

  window.write(prompt);

  inputBuffer = history.select(false, window, null);
  window.write(inputBuffer.join(''));
  history.lost = null;

  if (!maxlen)
    maxlen = (window.width * window.height) - prompt.length - 1;

  function clearLine()
  {
    window.cursorPosition.x -= inputBuffer.slice(-pos).join('').length;
    mark = window.getXY();
    window.write(spaces(inputBuffer.join('').length));
    window.cursorPosition = mark;
    inputBuffer = [];
    pos = 0;
  }

  while ((key = window.getKey()) != null)
  {
    newInput = [];

    switch(key)
    {
      default:
	newInput[0] = key;
	newInput.length = 1;
	break;

      case 'BACKSPACE':
      case '^?':
      case '^H':
	if (pos == 0)
	  break;
	pos--;
	window.cursorPosition.x -= inputBuffer[pos].length;
      case 'DC':
      case '^D':
	gone = inputBuffer.splice(pos, 1);
	if (gone[0])
	{
	  mark = window.getXY();
	  window.write(inputBuffer.slice(pos).join('') + spaces(gone[0].length));
	  window.cursorPosition = mark;
	}
	break;

      case '^W':
	for (i=pos; i >= 0; i--)
	{
	  if (inputBuffer[i] && (inputBuffer[i] == ' '))
	  {
	    i--;
	    break;
	  }
	}

	gone = inputBuffer.splice(i + 1, pos-i);
	window.cursorPosition.x -= gone.length;
	pos -= gone.length;
	mark = window.getXY();
	window.write(inputBuffer.slice(pos).join('') + spaces(gone.join('').length));
	window.cursorPosition = mark;
	break;

      case '^V':
	insertMode = !insertMode;
	inputPane.status = insertMode ? "Insert" : "Overwrite";
	break;

      case '^U':
	window.cursorPosition.x -= inputBuffer.join('').length;
	mark = window.getXY();
	window.write(spaces(inputBuffer.join('').length));
	window.cursorPosition = mark;
	pos = 0;
	inputBuffer.length = 0;
	break;

      case '^K':
	mark = window.getXY();
	clipboard = inputBuffer.slice(pos);
	inputBuffer.length -= clipboard.length;
	window.write(spaces(clipboard.join('').length));
	window.cursorPosition = mark;
	break;

      case '^Y':
	if (clipboard)
	  newInput = clipboard.concat([]);
	break;

      case '^A':
	if (pos)
	{
 	  window.cursorPosition.x -= inputBuffer.slice(-pos).join('').length;
  	  pos = 0;
	}
	break;

      case '^E':
	window.cursorPosition.x += inputBuffer.slice(pos).join('').length;
	pos = inputBuffer.length;
	break;

      case 'ENTER':
      case '^M':
      case '^J':
	window.cursorPosition.x = 0;
	window.cursorPosition.y++;
	history.lost = null;
	return inputBuffer;
	break;

      case '^P':
      case 'UP':
	if (!history.lost)
	  history.lost = inputBuffer;

	clearLine();
	newInput = history.select(-1, inputBuffer);
	break;

      case '^N':
      case 'DOWN':
	clearLine();
	newInput = history.select(+1, inputBuffer);
	break;

      case '^F':
      case 'RIGHT':
	if (pos < inputBuffer.length - 1)
	{
	  window.cursorPosition.x += inputBuffer[pos].length;
	  pos++;
	}
	else if (pos == inputBuffer.length - 1)
	{
	  pos++;
	  window.cursorPosition.x += 1;
	}
	break;

      case '^B':
      case 'LEFT':
	if (pos)
	{
	  pos--;
	  window.cursorPosition.x -= inputBuffer[pos].length;
	}
	break;

      case '^L':
	rootWin.refresh();
	window.refresh();
	mark = window.getXY();
	if (pos)
	  window.cursorPosition.x -= (prompt.length + inputBuffer.slice(-pos).join('').length);
	else
	  window.cursorPosition.x = 0;
	window.write(prompt, inputBuffer.join(''));

	window.cursorPosition = mark;
	break;

	window.cursorPosition.x = 0;
	mark = window.cursorPosition;
	window.writeXY(0,mark.y,prompt, inputBuffer.join(''));
	window.cursorPosition = mark;
	window.cursorPosition.x = prompt.length + inputBuffer.slice(-pos).join('').length;
	break;

      case '^C':
	window.write(key);
	return null;
    } 
    
    /* Not good enough: multi-character keys can push this over the edge in overwrite mode */
    if (insertMode && ((newInput.join('').length + inputBuffer.join('').length) > maxlen))
      newInput.length = maxlen - inputBuffer.join('').length;

    if (newInput.length)
    {
      mark = window.getXY();
      if (newInput.length > 1)
      {
	if (insertMode)
	  inputBuffer = inputBuffer.slice(0,pos).concat(newInput, inputBuffer.slice(pos));
	else
	  inputBuffer = inputBuffer.slice(0,pos).concat(newInput, inputBuffer.slice(pos + newInput.length));
      }
      else
	inputBuffer.splice(pos, insertMode ? 0 : newInput.length, newInput);

      window.write(inputBuffer.slice(pos).join(''));

      pos += newInput.length;
      mark.x += newInput.join('').length;
      window.cursorPosition = mark;
    }
  }

  return null;
}

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
  var history = new History();
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

      if ((line = readline(inputWin, history, clipboard, prompt)) == null)
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



