const XMLHttpRequest = require('xhr').XMLHttpRequest;

var myxhr = new XMLHttpRequest();
myxhr.onreadystatechange = function() {
    print("IN STATE: " + this.readyState);
}

myxhr.open('GET', 'http://www.google.com/');
myxhr.send();

print(myxhr.getAllResponseHeaders());

print(myxhr.responseRaw);

print("STATUS CODE = " + myxhr.status);
print("STATUS TEXT = " + myxhr.statusText);

var text = myxhr.responseText;
print("TEXT = " + text);

