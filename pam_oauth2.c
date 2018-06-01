#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <curl/curl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <libconfig.h>
#include "parson/parson.h"


static const int MAX_MAPPING_ARRAY_SIZE = 100;
static const int MAX_MAPPING_ITEM_SIZE = 50;

struct response
{
  char *ptr;
  size_t len;
};


static size_t writefunc(void *ptr, size_t size, size_t nmemb, struct response *r)
{
  size_t data_size = size * nmemb;
  size_t new_len = r->len + data_size;
  char *new_ptr = realloc(r->ptr, new_len + 1);

  if (new_ptr == NULL)
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: memory allocation failed");
    return 0;
  }

  r->ptr = new_ptr;

  memcpy(r->ptr + r->len, ptr, data_size);
  r->ptr[r->len = new_len] = '\0';

  return data_size;
}

static int check_response(const struct response token_info, char (*map_user_array) [MAX_MAPPING_ITEM_SIZE],
                          const char *const username_attribute)
{
  int match = 0;
  int r = PAM_AUTH_ERR;
  size_t i = 0;
  const char *name = NULL;  
  const char *const response_data = token_info.ptr;

  JSON_Value *schema = json_parse_string(response_data);
  name = json_object_get_string(json_object(schema), username_attribute);
  syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: user mapping item returned by B2ACCESS: %s\n", name);

  for ( i = 0; i < sizeof(map_user_array); i++) {
    if (strncmp(name, map_user_array[i], strlen(name)) == 0) 
    {
      syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: succesfully mapped item %s to local iRODS user\n\n", map_user_array[i]);
      match = 1;
      break;
    }
  }

  
  if (match == 1) 
  {
    r = PAM_SUCCESS;
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: successfully authenticated by B2ACCESS\n");
  }
  else
  {
   syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: user matching failed for user: %s\n", name); 
  } 
 
  return r;
}

static int query_token_info(const char *const tokeninfo_url, const char *const authtok,
                            long *response_code, struct response *token_info)
{
  char *bearer;
  struct curl_slist *headers = NULL;
  int ret = 1;
  CURLcode res;
  CURL *session = curl_easy_init();

  if (!session)
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: can't initialize curl");
    return ret;
  }



  if ((bearer = malloc(strlen("Authorization: Bearer ") + strlen(authtok) + 1)))
  {
    bearer = strcpy(bearer, "Authorization: Bearer ");  
    strcat(bearer, authtok);

    curl_easy_setopt(session, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(session, CURLOPT_URL, tokeninfo_url);
    curl_easy_setopt(session, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(session, CURLOPT_WRITEDATA, token_info);


    /* headers = curl_slist_append(headers, "Postman-Token: 5079ee8b-9734-4a19-b422-074ab1ffcc1d"); */
    headers = curl_slist_append(headers, "Cache-Control: no-cache");
    headers = curl_slist_append(headers, bearer);
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(session, CURLOPT_HTTPHEADER, headers);
    /* curl_easy_setopt(session, CURLOPT_POSTFIELDS, "token=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJ1c2VyX2lkIjoiOWJjNDhmMTktN2M5Yy00YTk0LTlhNzEtYmVjOTUyMjZmOWRhIiwianRpIjoiNzI0YmJjYmYtNDE0My00ZjE1LTk5M2MtZGZmNTg2MWY4MjU3In0.u5BCjrkHMpS9SocDi9SAMKh0_sjrw0tav0rDX9uROaM"); */
  
    res = curl_easy_perform(session);
    if (res == CURLE_OK &&
        curl_easy_getinfo(session, CURLINFO_RESPONSE_CODE, response_code) == CURLE_OK)
    {
      ret = 0;
    }
    else
    {
      syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: curl request failed: %s\n", curl_easy_strerror(res));
    }

    /* free(postData); */
  }
  else
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: memory allocation failed");
  }

  curl_easy_cleanup(session);

  return ret;
}

static int oauth2_authenticate(const char *const tokeninfo_url, const char *const authtok,
                               char (*map_user_array) [MAX_MAPPING_ITEM_SIZE], const char *const username_attribute)
{
  struct response token_info;
  long response_code = 0;
  int ret;

  if ((token_info.ptr = malloc(1)) == NULL)
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: memory allocation failed");
    return PAM_AUTHINFO_UNAVAIL;
  }
  token_info.ptr[token_info.len = 0] = '\0';

  if (query_token_info(tokeninfo_url, authtok, &response_code, &token_info) != 0)
  {
    ret = PAM_AUTHINFO_UNAVAIL;
  }
  else if (response_code == 200)
  {
    ret = check_response(token_info, map_user_array, username_attribute);
  }
  else
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: authentication failed with response_code=%li", response_code);
    ret = PAM_AUTH_ERR;
  }

  free(token_info.ptr);

  return ret;
}

static void get_mapped_user(const char *filename, const char *user_key,
                              char (*map) [MAX_MAPPING_ITEM_SIZE])
{
  JSON_Array *array;
  JSON_Value *user_map;
  size_t i;

  syslog(LOG_AUTH | LOG_DEBUG, "Searching for user: %s\n\n", user_key);
  user_map = json_parse_file(filename);
  array = json_object_get_array(json_object(user_map), user_key);
  if (array != NULL) {
    for ( i = 0; i < json_array_get_count(array); i++) {
      syslog(LOG_AUTH | LOG_DEBUG, "Found local mapping item: %s\n\n", json_array_get_string(array, i));
      strcpy(map[i], json_array_get_string(array, i));
     }
  }
  
  json_value_free(user_map);
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  const char *authtok = NULL;
  char *tokeninfo_url = NULL;
  char *username_attribute = NULL;
  char *user_map_path = NULL;
  const char *irods_user = NULL;
  char map_user_array[MAX_MAPPING_ARRAY_SIZE][MAX_MAPPING_ITEM_SIZE];
  config_t cfg, *cf;
  const char *config_path;
  const char *token_validation_ep;
  const char *login_field;
  const char *user_map_file;

  if (argc > 0)
    config_path = argv[0];

  cf = &cfg;
  config_init(cf);
  /* Read the file. If there is an error, report it and exit. */
  if (!config_read_file(cf, config_path))
  {
    syslog(LOG_AUTH | LOG_DEBUG, "%s:%d - %s\n", config_error_file(cf),
           config_error_line(cf), config_error_text(cf));
    config_destroy(cf);
    return PAM_AUTHINFO_UNAVAIL;
  }

  if (config_lookup_string(cf, "token_validation_ep", &token_validation_ep))
  {
    tokeninfo_url = malloc(strlen(token_validation_ep) + 1);
    strcpy(tokeninfo_url, token_validation_ep);
    syslog(LOG_AUTH | LOG_DEBUG, "Token validation EP: %s\n\n", tokeninfo_url);
  }
  else
  {
    syslog(LOG_AUTH | LOG_DEBUG, "No 'token_validation_ep' setting in configuration file.\n");
    return PAM_AUTHINFO_UNAVAIL;
  }
  
  if (config_lookup_string(cf, "login_field", &login_field))
  {
    username_attribute = malloc(strlen(login_field) + 1);
    strcpy(username_attribute, login_field);
    syslog(LOG_AUTH | LOG_DEBUG, "username_attribute: %s\n\n", username_attribute);
  }
  else
  {
    syslog(LOG_AUTH | LOG_DEBUG, "No 'username_attribute' setting in configuration file.\n");
    return PAM_AUTHINFO_UNAVAIL;
  } 

  if (config_lookup_string(cf, "user_map_file", &user_map_file))
  {
    user_map_path = malloc(strlen(user_map_file) + 1);
    strcpy(user_map_path, user_map_file);
    syslog(LOG_AUTH | LOG_DEBUG, "user_map_path: %s\n\n", user_map_path);
  }
  else
  {
    user_map_path = NULL;
    syslog(LOG_AUTH | LOG_DEBUG, "No 'user_map_file' setting in configuration file.\n");
  }

  config_destroy(cf);

  /*    if (argc > 0) tokeninfo_url = argv[0]; */
  /*    if (argc > 1) ct[0].key = argv[1]; */


  if (tokeninfo_url == NULL || *tokeninfo_url == '\0')
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: tokeninfo_url is not defined or invalid");
    return PAM_AUTHINFO_UNAVAIL;
  }

  if (username_attribute == NULL || *username_attribute == '\0')
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: login_field is not defined or empty");
    return PAM_AUTHINFO_UNAVAIL;
  }

  if (pam_get_user(pamh, &irods_user, NULL) != PAM_SUCCESS || irods_user == NULL || *irods_user == '\0')
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: can't get user login");
    return PAM_AUTHINFO_UNAVAIL;
  }

  if (pam_get_authtok(pamh, PAM_AUTHTOK, &authtok, NULL) != PAM_SUCCESS || authtok == NULL || *authtok == '\0')
  {
    syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: can't get authtok");
    return PAM_AUTHINFO_UNAVAIL;
  }

  if (user_map_path != NULL)
  {
    get_mapped_user(user_map_path, irods_user, map_user_array);
    if (map_user_array != NULL)
    {
      syslog(LOG_AUTH | LOG_DEBUG, "pam_oauth2: Found user mapping array for user %s\n\n", irods_user);
    }
  }

  return oauth2_authenticate(tokeninfo_url, authtok, map_user_array, username_attribute);
}

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  return PAM_CRED_UNAVAIL;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  return PAM_SUCCESS;
}
