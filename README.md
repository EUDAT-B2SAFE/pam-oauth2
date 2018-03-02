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
auth sufficient pam_oauth2.so /etc/irods/pam.conf grp=tester
```

And somebody is trying to login with login=foo and password=bar.

pam\_oauth2 module will make http request https://foo.org/oauth2/tokeninfo?access_token=bar (tokeninfo url is simply concatenated with token) and check response code and content.

If the response code is not 200 - authentication will fail. After that it will check response content:

```json
{
  "access_token": "bar",
  "expires_in": 3598,
  "grp": "tester",
  "scope": [
    "uid"
  ],
  "token_type": "Bearer",
  "uid": "foo"
}
```

It will check that response is a valid JSON object and top-level object contains following key-value pairs:
```json
  "uid": "foo",
  "grp": "tester"
```

If some keys haven't been found or values don't match with expectation - authentication will fail.

License
-------

The original project uses the [MIT license](https://github.com/zalando-incubator/pam-oauth2/blob/master/LICENSE).
