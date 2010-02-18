exports.Array	= [ Array, [], [,,,], new Array(), new Array(123) ];
exports.Object 	= [ Object, {}, { hello: 123 }, new Object() ];
exports.XML	= [ XML, <hello>world</hello>, new XMLList() ];
exports.Date	= [ Date, new Date(), Date.now() ];
exports.RegExp	= [ RegExp, /hello/, new RegExp ];
//exports.UserClass = [ UserClass, new UserClass() ];
exports.Error = [ Error, new Error(), Error() ]
