// Load and parse an XML string.
var xml = new XML('<xml><test>TestData</test><test>TestData 2<test2>test</test2></test></xml>');
consoleLog('info', 'XML-DUMP\n' + xml.serialize());

// Get the first child named 'test'
var child = xml.getChild('test');
if(child) {
    consoleLog('info', 'XML child name: ' + child.name + ', data: ' + child.data);
}

// Get next child of the same name and same level
var nextChild = child.next();

if(nextChild) {
    consoleLog('info', 'XML child name: ' + nextChild.name + ', data: ' + nextChild.data);
}

// Create a new child
var newChild = xml.addChild('NewTest');

// Set some data to it
newChild.data = 'new data';

// Set an attribute
newChild.setAttribute('firstattr', 'myvalue');

// Print the attribute
consoleLog('info', 'XML newChild attrbute firstattr: ' + newChild.getAttribute('firstattr'));

// Log the entire XML
consoleLog('info', 'Full XML ::\n' + xml.serialize());

// Remove the first child 'test'
consoleLog('info', 'Remove child [' + child.name + ']\n');
child.remove();

// Log the entire XML
consoleLog('info', 'Full XML ::\n' + xml.serialize());

