var displayExpressions = [];

function DisplayExpression(expr)
{
  this.expr = expr;
  this.url = stack[currentFrame].script.url;
  this.funname = stack[currentFrame].script.funname;
}

exports.showForThisFrame = function()
{
  var i;

  for (i=0; i < displayExpressions.length; i++)
  {
    if ((stack[currentFrame].script.funname == displayExpressions[i].funname)
        && (stack[currentFrame].script.url == displayExpressions[i].url))
    {
      write("  " + i + "\t: " + displayExpressions[i].expr + " = ");
      show(displayExpressions[i].expr);
    }
  }
}

exports.add = function(args)
{
  displayExpressions.push(new DisplayExpression(args.join(" ")));
}
