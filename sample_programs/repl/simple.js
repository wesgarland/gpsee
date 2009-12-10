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

const screen = new Curses.Window();
screen.scrolling = true;

function main()
{
  var inputModule = require("./input");
  var history = new inputModule.History();
  var clipboard = new Array();
  var buffer = "";
  var prompt;
  var pendingHistory;

  function print()
  {
    var window = screen;

    window.write(Array.prototype.join.call(arguments, ' '));
    window.cursorPosition.x = 0;
    window.cursorPosition.y++;
  }


  var line;

  for(;;)
  {
    prompt = "js> ";

    pendingHistory = new Array();

    do
    {
      if (pendingHistory.length)
	pendingHistory = pendingHistory.concat(" ");

      if ((line = inputModule.readline(screen, history, clipboard, prompt)) == null)
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
//      buffer = 'with ({}) {' + buffer + '}';	   
      screen.writeln(eval(buffer));
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

      screen.writeln(msg);
    }
    finally
    {
      buffer = "";
    }
  }
}

main();



