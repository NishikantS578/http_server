#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <threads.h>
#include "zlib.h"

#define CHUNK 512

struct Client
{
	int client_fd;
	unsigned char file_name[100];
};

int def(unsigned char *source, unsigned char *dest)
{
	int ret;
	unsigned have;
	unsigned char in[CHUNK] = "";
	strcpy(in, source);
	unsigned char out[CHUNK] = "";

	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.next_in = in;
	strm.avail_in = strlen(in);
	strm.avail_out = CHUNK;
	strm.next_out = out;
	deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);

	ret = deflate(&strm, Z_FINISH);
	deflateEnd(&strm);
	for (int i = 0; i < CHUNK; i++)
	{
		dest[i] = out[i];
	}
	return strm.total_out;
}

int handle_client(void *client)
{
	int res_len = 0;
	int client_fd = ((struct Client *)client)->client_fd;
	unsigned char file_name[100] = "";
	strcpy(file_name, ((struct Client *)client)->file_name);
	free((struct Client *)client);

	unsigned char buf[1024] = "";
	int received_bytes_count = read(client_fd, buf, 1024);
	if (received_bytes_count < 0)
	{
		printf("Could not recieve data");
	}
	int i = 0;
	unsigned char res[1024] = "HTTP/1.1 404 Not Found\r\n\r\n";
	res_len = strlen(res);
	if (strncmp(buf, "GET", 3) == 0)
	{
		i += 3;

		i++;
		if (strncmp(buf + i, "/", 1) == 0)
		{
			i++;
			if (strncmp(buf + i, "echo", 4) == 0)
			{
				i += 4;

				i++;
				unsigned char data[CHUNK] = "";
				while (buf[i] != ' ')
				{
					strncat(data, buf + i, 1);
					i++;
				}
				while (strncmp(buf + i, "Accept-Encoding:", 16) != 0)
				{
					i++;
				}
				i += 16;

				i++;
				unsigned char encoding[20] = "";
				unsigned char encoding_res[20] = "";
				while (buf[i] != '\r')
				{
					strcpy(encoding_res, "");
					while (buf[i] != ',' && buf[i] != '\r')
					{
						strncat(encoding_res, buf + i, 1);
						i++;
					}
					if (buf[i] == ',')
					{
						i += 2;
					}
					if (strncmp(encoding_res, "gzip", 4) == 0)
					{
						strcpy(encoding, "gzip");
					}
				}

				if (strcmp(encoding, "gzip") == 0)
				{
					int compressed_size;
					unsigned char data_compressed[CHUNK] = "";
					strncpy(data_compressed, data, CHUNK);
					compressed_size = def(data, data_compressed);

					sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Encoding: %s\r\nContent-Length: %d\r\n\r\n", encoding, compressed_size);
					res_len = strlen(res);
					for (int i = 0; i < compressed_size; i++)
					{
						res[res_len] = data_compressed[i];
						res_len += 1;
					}
				}
				else
				{
					sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Encoding: %s\r\nContent-Length: %d\r\n\r\n%s", encoding, strlen(data), data);
					res_len = strlen(res);
				}
			}
			else if (strncmp(buf + i, "user-agent", 10) == 0)
			{
				i += 10;
				unsigned char data[511] = "";
				while (1)
				{
					while (buf[i] != '\n')
					{
						i++;
					}
					i++;
					if (strncmp(buf + i, "User-Agent:", 11) == 0)
					{
						i += 11;

						i++;
						while (buf[i] != '\r')
						{
							strncat(data, buf + i, 1);
							i++;
						}
						sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", strlen(data), data);
						res_len = strlen(res);
						break;
					}
				}
			}
			else if (strncmp(buf + i, "files", 5) == 0)
			{
				i += 5;
				i++;
				while (buf[i] != ' ')
				{
					strncat(file_name, buf + i, 1);
					i++;
				}
				FILE *fs = fopen(file_name, "r");
				if (fs != NULL)
				{
					unsigned char file_content[CHUNK] = "";
					fseek(fs, 0L, SEEK_END);
					int file_size = ftell(fs);
					fseek(fs, 0, SEEK_SET);
					fread(file_content, file_size, 1, fs);
					fclose(fs);

					sprintf(res, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %d\r\n\r\n%s", file_size, file_content);
					res_len = strlen(res);
				}
			}
			else if (buf[i] == ' ')
			{
				sprintf(res, "HTTP/1.1 200 OK\r\n\r\n");
			}
		}
	}
	else if (strncmp(buf, "POST", 4) == 0)
	{
		printf("%s: hi", file_name);
		i += 4;

		i++;

		if (strncmp(buf + i, "/", 1) == 0)
		{
			i++;
			if (strncmp(buf + i, "files", 5) == 0)
			{
				i += 5;
				i++;
				while (buf[i] != ' ')
				{
					strncat(file_name, buf + i, 1);
					i++;
				}
				FILE *fs = fopen(file_name, "w");
				if (fs != NULL)
				{
					unsigned char file_content[CHUNK] = "";
					while (strncmp(buf + i, "\r\n\r\n", 4) != 0)
					{
						i++;
					}
					i += 4;
					while (strncmp(buf + i, "\0", 1))
					{
						strncat(file_content, buf + i, 1);
						i++;
					}
					fwrite(file_content, strlen(file_content), 1, fs);
					fclose(fs);

					sprintf(res, "HTTP/1.1 201 Created\r\n\r\n");
					res_len = strlen(res);
				}
			}
		}
	}

	printf("\n%s, %d", res, res_len);
	send(client_fd, res, res_len, MSG_CONFIRM);
	close(client_fd);
	return 0;
}

int main(int argc, unsigned char *argv[])
{
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(4221),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	while (1)
	{
		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		printf("Client connected\n");

		thrd_t t;
		struct Client *client = (struct Client *)malloc(sizeof(struct Client));
		client->client_fd = client_fd;
		strcpy(client->file_name, argv[2]);
		int thread_id = thrd_create(&t, handle_client, (void *)(client));
	}

	close(server_fd);
	return 0;
}
