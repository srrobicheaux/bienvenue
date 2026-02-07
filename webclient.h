// Include your client functions from network.c
bool network_post(const char *json_body, const char *middleware_host, const char * middleware_path);
bool networkGet(const char *middleware_host, const char *middleware_path);
bool ResponseNumber(const char *key, long long *out_buf, size_t buf_size);
bool ResponseString(const char *key, char *out_buf, size_t buf_size);
