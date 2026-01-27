// Include your client functions from network.c
bool network_post(const char *json_body);
char *NetworkResponse(const char *key, char *out_buf, size_t buf_size);