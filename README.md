OAuth2 PAM module
=================

This PAM module enables login with OAuth2 token instead of password.
And it is a fork of https://github.com/CyberDem0n/pam-oauth2

## How to install it:

```bash
$ sudo yum install openssl-devel pam-devel libcurl-devel libconfig-devel
$ git clone <current project>
$ git submodule init
$ git submodule update
$ cp utilities/Makefile parson/Makefile
$ make
$ sudo make install
```
Sometimes it is necessary also:
```bash
$ cp /lib/security/pam_oauth2.so /usr/lib64/security/
```

## Configuration

```
auth sufficient pam_oauth2.so <path to configuration file> key1=value2 key2=value2
```

## How it works

Lets assume that configuration is looking like:

```
auth sufficient pam_oauth2.so /etc/irods/pam.conf active=true
```
And the configuration file /etc/irods/pam.conf looks like this:

```
#OAuth2 url for token validation
token_validation_ep = "https://b2access-integration.fz-juelich.de:443/oauth2/userinfo"
#OAuth2 attribute key to identify the attribute in the response used to match the login username
login_field = "email"
# path to user map file
user_map_file = "/etc/irods/user_map.json"
```

And somebody is trying to login with login=foo and password=bar.

pam\_oauth2 module will make an http request to https://b2access-integration.fz-juelich.de:443/oauth2/userinfo passing the "bar" value and check response code and content.

If the response code is not 200 - authentication will fail. 
If response code is 200 the response content is parsed and the value of the property "email" is used to identify the user:

```json
{
  "email":"roberto@email.com",
  "active":true,
  "token_type":"Bearer",
  "exp":1520001942,
  "iat":1519998342,
  "client_id":"nrlRYuk3JrEAlwMG8B8Y2pbaPi0a",
  "username":"admin@carbon.super"
}
```

To successfully authenticate the user and map it to a valid iRODS one, the value "roberto@email.com" must be associated to the current iRODS username in the user_map_file, as explained in the next session.


### User mapping
If we want to have the OAuth2 username decoupled from the local iRODS username, we need a mapping between the OAuth2 user and the iRODS user. In the pam configuration file /etc/irods/pam.conf must be specified the path to the map file:
```
# path to user map file
user_map_file = "/etc/irods/user_map.json"
```
where the user_map.json is, for instance (note that an array is need also for a single email value):
```json
{
  "roberto": ["roberto@email.it", "r.mucci@email.it"],
  "claudio": ["c.cacc@email.com", "claudio@email.it", "c.cacciari@email.it"],
  "paolo": ["paolo@email.com"]
}
```
When the iRODS user "roberto" performs an "iinit", the pam-oauth2 module gets the email array associated to "roberto" (```["roberto@email.it", "r.mucci@email.it"]```). Only if the the email returned by B2ACCESS ("roberto@email.it") appears in the email array the user is successfully authenticated. 


## How it works with iRODS

iRODS (www.irods.org) supports PAM authentication (https://docs.irods.org/4.2.1/plugins/pluggable_authentication/).  
It is possible to enable the OAuth2 authentication in iRODS in this way:
* the user get a valid OAuth2 token from an independent tool (web portal, http API, etc.)
* then she login via command line using PAM iRODS configuration and passing the token string as password.
* if the aforementioned PAM module is correctly configured in /etc/pam.d/irods, then the user can be authenticated.  
  
This PAM module is just a Proof of Concept and it has been tested on a CENTOS 7 with iRODS v4.2.1 and WSO2 Identity provider v5.4.1 (https://wso2.com/identity-and-access-management), in particular using the steps described here: https://docs.wso2.com/display/IS530/Invoke+the+OAuth+Introspection+Endpoint to invoke the introspection endpoint with a scope:

```
$ curl -v -X POST --basic -u nrlRYuk3JrEAlwMG8B8Y2pbaPi0a:8YDugo0txFVczgAjSIPkwvR_32ka -H 'Content-Type: application/x-www-form-urlencoded;charset=UTF-8' -k -d 'grant_type=client_credentials&scope=claudio' https://localhost:9443/oauth2/token
[ ... ]
* Connection #0 to host localhost left intact
{"access_token":"2294e78b-bad0-3609-a6d4-5b4950d64833","scope":"claudio","token_type":"Bearer","expires_in":3600}

$ curl -k -u admin:admin -H 'Content-Type: application/x-www-form-urlencoded' -X POST --data 'token=2294e78b-bad0-3609-a6d4-5b4950d64833' https://localhost:9443/oauth2/introspect
{"scope":"claudio","active":true,"token_type":"Bearer","exp":1520001942,"iat":1519998342,"client_id":"nrlRYuk3JrEAlwMG8B8Y2pbaPi0a","username":"admin@carbon.super"}
```
Then the token obtained is provided as input to the iRODS login command:

```
$ iinit
Enter your current PAM password:
```

And if it works, it is possible to see in the syslog debug log:

```
Token validation EP: https://b2access-integration.fz-juelich.de:443/oauth2/userinfo
Jun  1 09:26:34 localhost irodsPamAuthCheck: username_attribute: email
Jun  1 09:26:34 localhost irodsPamAuthCheck: user_map_path: /etc/irods/user_map.json
Jun  1 09:26:34 localhost irodsPamAuthCheck: Searching for user: roberto
Jun  1 09:26:34 localhost irodsPamAuthCheck: Found local mapping item: roberto@email.it
Jun  1 09:26:34 localhost irodsPamAuthCheck: Found local mapping item: r.mucci@email.it
Jun  1 09:26:34 localhost irodsPamAuthCheck: pam_oauth2: Found user mapping array for user roberto
Jun  1 09:26:34 localhost irodsPamAuthCheck: pam_oauth2: user mapping item returned by B2ACCESS: roberto@email.com
Jun  1 09:26:34 localhost irodsPamAuthCheck: pam_oauth2: succesfully mapped item to local iRODS user
Jun  1 09:26:34 localhost irodsPamAuthCheck: pam_oauth2: successfully authenticated by B2ACCESS
```

And in iRODS server log:

```
[ ... ]
Authenticated
Mar  2 13:46:16 pid:9327 NOTICE: writeLine: inString = [pep_auth_agent_auth_response_pre] USER CONNECTION INFORMATION -:- auth_scheme: native, client_addr: 127.0.0.1, proxy_rods_zone: cinecaDevel1, proxy_user_name: claudio, user_rods_zone: cinecaDevel1, user_user_name: claudio
```

License
-------

The original project uses the [MIT license](https://github.com/zalando-incubator/pam-oauth2/blob/master/LICENSE).
