// YOU CAN USE THIS FILE AS REFERENCE FOR SERVER DEVELOPMENT

// include the express module
var express = require("express");

// create an express application
var app = express();

// helps in extracting the body portion of an incoming request stream
var bodyparser = require('body-parser');

// fs module - provides an API for interacting with the file system
var fs = require("fs");

// helps in managing user sessions
var session = require('express-session');

// native js function for hashing messages with the SHA-256 algorithm
var crypto = require('crypto');

// include the mysql module
var mysql = require("mysql");

var xml2js = require('xml2js');

var xmlParser = xml2js.parseString;

// use express-session
// in mremory session is sufficient for this assignment
app.use(session({
  secret: "csci4131secretkey",
  saveUninitialized: true,
  resave: false
}));

// apply the body-parser middleware to all incoming requests
app.use(bodyparser());
app.use(bodyparser.json());


// Read in XML config file;
var text = fs.readFileSync("./dbconfig.xml").toString('utf-8');

var xmlConfig;
xmlParser(text, function(err,result){
  xmlConfig = result.dbconfig;
});

var cfg = {
  host: xmlConfig.host[0],
  user: xmlConfig.user[0],
  password: xmlConfig.password[0],
  database: xmlConfig.database[0],
  port: xmlConfig.port[0]
}

// Connect to mysql server
var con = mysql.createConnection(cfg);

// server listens on port 9007 for incoming connections
app.listen(9012, () => console.log('Listening on port 9007!'));

app.get('/',function(req, res) {
  res.sendFile(__dirname + '/client/welcome.html');
});

// // GET method route for the events page.
// It serves events.html present in client folder
app.get('/events',function(req, res) {
  if (req.session.loggedin) {
    res.sendFile(__dirname + '/client/events.html');
  } else {
    res.redirect('/login');
  }
});

// GET method route for the addEvent page.
// It serves addEvent.html present in client folder
app.get('/addEvent',function(req, res) {
  if (req.session.loggedin) {
    res.sendFile(__dirname + '/client/addEvent.html');
  } else {
    res.redirect('/login');
  }
});

//GET method for stock page
app.get('/stock', function (req, res) {
  if (req.session.loggedin) {
    res.sendFile(__dirname + '/client/stock.html');
  } else {
    res.redirect('/login');
  }
});

//GET method for admin page
app.get('/admin', function (req, res) {
  if (req.session.loggedin) {
    res.sendFile(__dirname + '/client/admin.html');
  } else {
    res.redirect('/login');
  }
});

// GET method route for the login page.
// It serves login.html present in client folder
app.get('/login',function(req, res) {
  if(!req.session.loggedin) {
    res.sendFile(__dirname + '/client/login.html');
  } else {
    res.redirect('/events');
  }
});


// GET method to return the list of events
// The function queries the tbl_events table for the list of events and sends the response back to client
app.get('/getListOfEvents', function(req, res) {
  con.query('SELECT * FROM tbl_events', function(err,result,fields) {
    if (err) throw err;
    if (result.length == 0) {
      console.log("No entries in events table");
    } else { // Generate the JSON string
      var jsonStr = '{"events":[';
      for (var i = 0 ; i < result.length; i++) {
        jsonStr += '{' +
        '"day":' + '"' + result[i].event_day + '"' + ',' +
        '"event":' + '"' + result[i].event_event + '"' + ',' +
        '"start":' + '"' + result[i].event_start + '"' + ',' +
        '"end":' + '"' + result[i].event_end + '"' + ',' +
        '"phone":' + '"' + result[i].event_location + '"' + ',' +
        '"location":' + '"' + result[i].event_phone + '"' + ',' +
        '"info":' + '"' + result[i].event_info + '"' + ',' +
        '"url":' + '"' + result[i].event_url + '"' + '},';
      }
      jsonStr = jsonStr.slice(0, -1); // Remove last comma
      jsonStr += ']}'
      res.send(jsonStr);
    }
  });
});

// GET method to return the list of accounts
// The function queries the tbl_events table for the list of events and sends the response back to client
app.get('/getListOfUsers', function(req, res) {
  con.query('SELECT * FROM tbl_accounts', function(err,result,fields) {
    if (err) throw err;
    if (result.length == 0) {
      console.log("No entries in accounts table");
    } else { // Send Accounts table
      res.send(result);
    }
  });
});

// POST method to insert details of a new user into tbl_accounts table
app.post('/addUser', function(req, res) {
    var userName = req.body.name;
    var userLogin = req.body.login;
    var userPass =  crypto.createHash('sha256').update(req.body.password).digest('base64');
    if (userName && userLogin && userPass) {
      con.query('SELECT * FROM tbl_accounts WHERE acc_login = ?', [userLogin], function(error, results, fields) {
  			if (results.length > 0 && results[0].acc_id != userId) {
          // User already Exists
  				console.log("User Already Exists");
				res.redirect('/admin');
  			} else {
				var rowToBeInserted = {
					acc_name: userName,
					acc_login: userLogin,
					acc_password: userPass
				};
			con.query('INSERT tbl_accounts SET ?', rowToBeInserted, function(err, result) {
				if(err) {
				throw err;
				}
				console.log("Value inserted");
			});
			}
		});
    } else {
      console.log("Invalid UserData: one or more entries are missing!")
    }
});

// POST method to delete details of a user in tbl_events table
app.post('/deleteUser', function(req, res) {
    var login = req.body.login;
    console.log(req.body);

    con.query('DELETE FROM tbl_accounts WHERE acc_login = ?', [login], function(err, result) {
      if(err) {
        throw err;
      }
      console.log("User Deleted");
    });
  });

// POST method to insert details of a new event to tbl_events table
app.post('/postEvent', function(req, res) {
    var rowToBeInserted = {
    event_day: req.body.day,
    event_event: req.body.event,
    event_start: req.body.start,
    event_end: req.body.end,
    event_location: req.body.location,
    event_phone: req.body.phone,
    event_info: req.body.info,
    event_url: req.body.url
  };
  con.query('INSERT tbl_events SET ?', rowToBeInserted, function(err, result) {
    if(err) {
      throw err;
    }
    console.log("Values inserted into events table");
  });
  res.redirect('/events');
});

// POST method to update details of a new event to tbl_events table
app.post('/updateUser', function(req, res) {
    var userName = req.body.name;
    var userLogin = req.body.login;
    var userPass =  crypto.createHash('sha256').update(req.body.password).digest('base64');
    var userId = req.body.id;
    if (userName && userLogin && userPass) {
      con.query('SELECT * FROM tbl_accounts WHERE acc_login = ?', [userLogin], function(error, results, fields) {
  			if (results.length > 0 && results[0].acc_id != userId) {
          // User already Exists
  				res.send('User already used by Another user!');
  			} else {
          con.query('UPDATE tbl_accounts SET acc_name = ?, acc_login = ?, acc_password = ? WHERE acc_id = ?', [userName, userLogin, userPass, userId], function(err, result) {
            if(err) {
              throw err;
            }
            console.log("Updated tbl_accounts");
          });
        }
  		});
    } else { // Send an error
      console.log("Invalid UserData: one or more entries are missing!")
    }
});

// POST method to validate user login
// upon successful login, user session is created
app.post('/sendLoginDetails', function(req, res) {
  var username = req.body.username;
	var password = crypto.createHash('sha256').update(req.body.password).digest('base64');
	if (username && password) {
		con.query('SELECT * FROM tbl_accounts WHERE acc_login = ? AND acc_password = ?', [username, password], function(error, results, fields) {
			if (results.length > 0) {
				req.session.loggedin = true;
				req.session.username = username;
        req.session.login = username;
				res.redirect('/events');
			} else {
				res.send('Incorrect Username and/or Password!');
			}
			res.end();
		});
	} else {
		res.send('Please enter Username and Password!');
		res.end();
	}
});

// Gets the session for returning the username
app.get('/userLogin', function (req, res) {
  if (req.session.loggedin) {
    res.send(req.session);
  } else {
    res.redirect('/login');
  }
});

// log out of the application
// destroy user session
app.get('/logout', function(req, res) {
  if(!req.session.loggedin) {
    res.send('Session not started, can not logout!');
  } else {
    console.log ("Session Destroyed!");
    req.session.destroy();
    res.redirect('/login');
  }
});

// middle ware to serve static files
app.use('/client', express.static(__dirname + '/client'));


// function to return the 404 message and error to client
app.get('*', function(req, res) {
  res.send("Error 404.");
});
