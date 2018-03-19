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
token_validation_ep = "https://localhost:9443/oauth2/introspect"
#OAuth2 attribute key to identify the attribute in the response used to match the login username
login_field = "scope"
#Oauth2 client (application) credential username
oauth2_client_username = "admin"
#Oauth2 client (application) credential password
oauth2_client_password = "admin"
```

And somebody is trying to login with login=foo and password=bar.

pam\_oauth2 module will make a POST http request to https://localhost:9443/oauth2/introspect and check response code and content.

If the response code is not 200 - authentication will fail. After that it will check response content:

```json
{
  "scope":"claudio",
  "active":true,
  "token_type":"Bearer",
  "exp":1520001942,
  "iat":1519998342,
  "client_id":"nrlRYuk3JrEAlwMG8B8Y2pbaPi0a",
  "username":"admin@carbon.super"
}
```

It will check that response is a valid JSON object and top-level object contains following key-value pairs:
```json
  "scope": "foo",
  "active": "true"
```

If some keys haven't been found or values don't match with expectation - authentication will fail.

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
Mar  2 13:46:13 localhost irodsPamAuthCheck: Token validation EP: https://localhost:9443/oauth2/introspect
Mar  2 13:46:13 localhost irodsPamAuthCheck: username_attribute: scope
Mar  2 13:46:13 localhost irodsPamAuthCheck: client_username: admin
Mar  2 13:46:13 localhost irodsPamAuthCheck: client_password: admin
Mar  2 13:46:13 localhost irodsPamAuthCheck: pam_oauth2: successfully authenticated 'claudio'
```

And in iRODS server log:

```
[ ... ]
Authenticated
Mar  2 13:46:16 pid:9327 NOTICE: writeLine: inString = [pep_auth_agent_auth_response_pre] USER CONNECTION INFORMATION -:- auth_scheme: native, client_addr: 127.0.0.1, proxy_rods_zone: cinecaDevel1, proxy_user_name: claudio, user_rods_zone: cinecaDevel1, user_user_name: claudio
```
### User mapping
If we want to have the OAuth2 username decoupled by the local iRODS username, we need a mapping between the OAuth2 user and the iRODS user. If in the pam configuration file /etc/irods/pam.conf there is the following line:
```
# path to user map file
user_map_file = "/etc/irods/user_map.json"
```
where the user_map.json is:
```
{
  "claudio" :"roberto"
}
```
And in the /etc/pam.d/irods, the next lines:
```
auth requisite pam_oauth2.so /etc/irods/pam.conf
auth required pam_exec.so debug /etc/irods/account_manager.sh
```
Where the file account_manager.sh is a shell script based on iRODS icommand client and it can be found in the utilities folder of the current project.  
Then the OAuth2 user "roberto" is mapped into the iRODS user "claudio".  
And I got:
```
pam_oauth2: successfully authenticated 'roberto'
```
but at the same time:
```
Authenticated
Mar 18 11:58:29 pid:1456 NOTICE: writeLine: inString = [pep_auth_agent_auth_response_pre] USER CONNECTION INFORMATION -:- auth_scheme: native, client_addr: 127.0.0.1, proxy_rods_zone: cinecaDevel1, proxy_user_name: claudio, user_rods_zone: cinecaDevel1, user_user_name: claudio
```
License
-------

The original project uses the [MIT license](https://github.com/zalando-incubator/pam-oauth2/blob/master/LICENSE).
