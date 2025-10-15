#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include "openssl/ssl.h"
#include "openssl/err.h"

void handle_http_request(int csock) {
	char buf[1024] = {0};

	// Read incoming request
	int bytes = read(csock, buf, sizeof(buf));
	if (bytes < 0) {
		perror("HTTP read failed");
		exit(1);
	}

	char method[16], uri[256], host[256];
	sscanf(buf, "%15s %255s", method, uri);

	char* host_start = strstr(buf, "\nHost: ");
	if (host_start) {
		sscanf(host_start, "\nHost: %255s", host);
	}
	else {
		strcpy(host, "10.0.0.1");
	}

	// Construct Location header
	char location[1024];
	snprintf(location, sizeof(location), "https://%s%s", host, uri);

	char response[2048];
	snprintf(response, sizeof(response), "HTTP/1.0 301 Moved Permanently\r\nLocation: %s\r\n\r\n", location);
	write(csock, response, strlen(response));
	close(csock);
}

void handle_https_request(SSL* ssl) {
    char response[65536] = {0};
    if (SSL_accept(ssl) == -1){
		perror("SSL_accept failed");
		exit(1);
	}
    else {
		char buf[1024] = {0};
        int bytes = SSL_read(ssl, buf, sizeof(buf));
		if (bytes < 0) {
			perror("SSL_read failed");
			exit(1);
		}

		// Get content
		// 1. Parse method and URI
		char method[16], uri[256];
		sscanf(buf, "%15s %255s", method, uri);

		// 2. Check if method is GET and URI available
		if (strcmp(method, "GET") != 0) {
			snprintf(response, sizeof(response), "HTTP/1.0 501 Not Implemented\r\n\r\n");
		}
		else {
			FILE* file = fopen(uri + 1, "r"); // Skip the leading '/'

			if (file == NULL) {
				// File not found
				snprintf(response, sizeof(response), "HTTP/1.0 404 Not Found\r\n\r\n");
			}
			else {
				fseek(file, 0, SEEK_END);
				long fsize = ftell(file);
				fseek(file, 0, SEEK_SET);

				char* content = (char*)malloc(fsize + 1);
				if (content == NULL) {
					perror("malloc failed");
					fclose(file);
					exit(1);
				}
				fread(content, 1, fsize, file);
				content[fsize] = '\0';
				fclose(file);

				if (strstr(buf, "\nRange: ")) {
					size_t start, end;

					char* range_start = strstr(buf, "\nRange: bytes=");
					if (range_start) {
						range_start += strlen("\nRange: bytes=");
						char* sep = strchr(range_start, '-');
						*sep = '\0';
						if (strlen(range_start)) { // start is specified
							start = atoi(range_start);
							if (start >= fsize) {
								snprintf(response, sizeof(response), "HTTP/1.0 416 Requested Range Not Satisfiable\r\n\r\n");
								goto to_end;
							}
							range_start = sep + 1;
							sep = strchr(range_start, '\r');
							*sep = '\0';
							if (strlen(range_start)) { // end is specified
								end = atoi(range_start);
								if (end >= fsize) {
									snprintf(response, sizeof(response), "HTTP/1.0 416 Requested Range Not Satisfiable\r\n\r\n");
									goto to_end;
								}
								else {
									if (end < start) {
										snprintf(response, sizeof(response), "HTTP/1.0 416 Requested Range Not Satisfiable\r\n\r\n");
										goto to_end;
									}
									else {
										snprintf(response, sizeof(response), "HTTP/1.0 206 Partial Content\r\nContent-Range: bytes %zu-%zu/%ld\r\nContent-Length: %zu\r\n\r\n%.*s", start, end, fsize, end - start + 1, (int)(end - start + 1), content + start);
									}
								}
							}
							else { // end is not specified
								end = fsize - 1;
								snprintf(response, sizeof(response), "HTTP/1.0 206 Partial Content\r\nContent-Range: bytes %zu-%zu/%ld\r\nContent-Length: %zu\r\n\r\n%.*s", start, end, fsize, end - start + 1, (int)(end - start + 1), content + start);
							}
						}
						else { // start is not specified
							snprintf(response, sizeof(response), "HTTP/1.0 501 Not Implemented\r\n\r\n");
							goto to_end;
						}
					}
					else { // Malformed Range header
						snprintf(response, sizeof(response), "HTTP/1.0 400 Bad Request\r\n\r\n");
					}
				} else {
					// Full content response
					snprintf(response, sizeof(response), "HTTP/1.0 200 OK\r\nContent-Length: %ld\r\n\r\n%s", fsize, content);
				}
				to_end:
				free(content);
			}
		}

        SSL_write(ssl, response, strlen(response));
    }
    int sock = SSL_get_fd(ssl);
    SSL_free(ssl);
    close(sock);
}


#include <pthread.h>

typedef struct {
	int sockfd;
	SSL_CTX *ctx;
} https_thread_arg_t;

typedef struct {
	int sockfd;
} http_thread_arg_t;

void* https_thread_func(void* arg) {
	https_thread_arg_t* args = (https_thread_arg_t*)arg;
	int sockfd = args->sockfd;
	SSL_CTX* ctx = args->ctx;
	while (1) {
		struct sockaddr_in caddr;
		socklen_t len = sizeof(caddr);
		int csock = accept(sockfd, (struct sockaddr*)&caddr, &len);
		if (csock < 0) {
			perror("HTTPS Accept failed");
			continue;
		}
		SSL *ssl = SSL_new(ctx);
		SSL_set_fd(ssl, csock);
		handle_https_request(ssl);
	}
	return NULL;
}

void* http_thread_func(void* arg) {
	http_thread_arg_t* args = (http_thread_arg_t*)arg;
	int sockfd = args->sockfd;
	while (1) {
		struct sockaddr_in caddr;
		socklen_t len = sizeof(caddr);
		int csock = accept(sockfd, (struct sockaddr*)&caddr, &len);
		if (csock < 0) {
			perror("HTTP Accept failed");
			continue;
		}
		handle_http_request(csock);
	}
	return NULL;
}

int main()
{
	// init SSL Library
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	// enable TLS method
	const SSL_METHOD *method = TLS_server_method();
	SSL_CTX *ctx = SSL_CTX_new(method);

	// load certificate and private key
	if (SSL_CTX_use_certificate_file(ctx, "./keys/cnlab.cert", SSL_FILETYPE_PEM) <= 0) {
		perror("load cert failed");
		exit(1);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, "./keys/cnlab.prikey", SSL_FILETYPE_PEM) <= 0) {
		perror("load prikey failed");
		exit(1);
	}

	// 1. Init socket
	int sock_1 = socket(AF_INET, SOCK_STREAM, 0);
	int sock_2 = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_1 < 0 || sock_2 < 0) {
		perror("Opening socket failed");
		exit(1);
	}
	int enable = 1;
	if (setsockopt(sock_1, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0 || setsockopt(sock_2, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
		exit(1);
	}

	// 2. Bind sock fd to port 443 and 80
	struct sockaddr_in addr_1, addr_2;
	bzero(&addr_1, sizeof(addr_1));
	addr_1.sin_family = AF_INET;
	addr_1.sin_addr.s_addr = INADDR_ANY;
	addr_1.sin_port = htons(443);

	bzero(&addr_2, sizeof(addr_2));
	addr_2.sin_family = AF_INET;
	addr_2.sin_addr.s_addr = INADDR_ANY;
	addr_2.sin_port = htons(80);

	if (bind(sock_1, (struct sockaddr*)&addr_1, sizeof(addr_1)) < 0) {
		perror("Bind 443 failed");
		exit(1);
	}
	if (bind(sock_2, (struct sockaddr*)&addr_2, sizeof(addr_2)) < 0) {
		perror("Bind 80 failed");
		exit(1);
	}

	// 3. Listen for connections
	listen(sock_1, 10);
	listen(sock_2, 10);

	// 4. Create threads for HTTP and HTTPS
	pthread_t tid_https, tid_http;
	https_thread_arg_t https_args = {sock_1, ctx};
	http_thread_arg_t http_args = {sock_2};

	if (pthread_create(&tid_https, NULL, https_thread_func, &https_args) != 0) {
		perror("pthread_create https failed");
		exit(1);
	}
	if (pthread_create(&tid_http, NULL, http_thread_func, &http_args) != 0) {
		perror("pthread_create http failed");
		exit(1);
	}

	pthread_join(tid_https, NULL);
	pthread_join(tid_http, NULL);

	close(sock_1);
	close(sock_2);
	SSL_CTX_free(ctx);

	return 0;
}
