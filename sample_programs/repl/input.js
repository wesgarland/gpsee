/** REPL-orient input module */

exports.History = function History()
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

exports.readline = function readline(window, history, clipboard, prompt, maxlen)
{
  var key;
  var inputBuffer;		/* array of character-strings to return as a string */
  var pos = 0;              	/* Position within inputBuffer which matches screen cursor */
  var insertMode = true;	/* false = overwrite, true = shift inputBuffer to right */
  var i;
  var gone;			/* temp: stuff we just deleted from inputBuffer */
  var mark;			/* temp: cursor point (x,y) */
  var newInput;			/* temp: stuff we're just about to add to inputBuffer */

  function spaces(n)
  {
    var a = "";

    while (n--)
      a += " ";
    return a;
  }

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
