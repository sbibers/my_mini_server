/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   my_mini_server.c                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sbibers <sbibers@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/09/26 13:56:55 by sbibers           #+#    #+#             */
/*   Updated: 2025/09/26 17:24:37 by sbibers          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static int g_server_fd = -1; // server socket file descriptor
static volatile sig_atomic_t g_shutdown_requested = 0; // signal handler flag

typedef struct s_client
{
    int id;
    int fd;
    char *client_buffer;
    struct s_client *next;
}  t_client;

static t_client *number_of_clients; // list of clients
static int last_id; // last client id

static void wrong_args()
{
    const char *msg = "Wrong number of arguments\n";
    write(2, msg, strlen(msg));
    exit(1);
}

static void fatal_error()
{
    const char *msg = "Fatal error\n";
    write(2, msg, strlen(msg));
    exit(1);
}

static void free_client(t_client *client)
{
    if (!client)
    {
        return;
    }
    close(client->fd);
    if (client->client_buffer)
    {
        free(client->client_buffer);
    }
    free(client);
}

static void free_clients(t_client *clients)
{
    t_client *next;

    while (clients)
    {
        next = clients->next;
        free_client(clients);
        clients = next;
    }
    number_of_clients = NULL;
}

void handle_sigint(int sig)
{
    (void)sig;
    g_shutdown_requested = 1;
    if (g_server_fd != -1)
    {
        // closing the listening socket will unblock select
        close(g_server_fd);
        g_server_fd = -1;
    }
}

static void cleanup_and_exit(int exit_code)
{
    if (g_server_fd != -1)
    {
        close(g_server_fd);
        g_server_fd = -1;
    }
    free_clients(number_of_clients);
    const char *msg = "Server shutting down ...\n";
    write(2, msg, strlen(msg));
    exit(exit_code);
}

static t_client *new_client(int fd)
{
    t_client *new_client = (t_client *)malloc(sizeof(t_client));
    if (!new_client)
    {
        if (g_server_fd != -1)
        {
            close(g_server_fd);
            g_server_fd = -1;
        }
        free_clients(number_of_clients);
        fatal_error();
    }
    new_client->fd = fd;
    new_client->id = last_id++;
    new_client->client_buffer = NULL;
    new_client->next = NULL;
    return (new_client);
}

static void add_client(t_client *client)
{
    t_client *iterator = number_of_clients;

    if (!iterator)
    {
        number_of_clients = client;
        return;
    }
    while (iterator->next)
    {
        iterator = iterator->next;
    }
    iterator->next = client;
}

static void broadcast(const char *msg, int fd)
{
    t_client *iterator = number_of_clients;

    while (iterator)
    {
        if (iterator->fd != fd)
        {
            send(iterator->fd, msg, strlen(msg), 0);
        }
        iterator = iterator->next;
    }
}

static void remove_client(int fd)
{
    t_client *iterator = number_of_clients;
    t_client *prev = NULL;

    while (iterator)
    {
        if (iterator->fd == fd)
        {
            if (prev)
            {
                prev->next = iterator->next;
            }
            else
            {
                number_of_clients = iterator->next;
            }
            free_client(iterator);
            return;
        }
        prev = iterator;
        iterator = iterator->next;
    }
}

static void append_to_buffer(t_client *client, const char *data, int len)
{
    size_t old_len = 0;
    if (client->client_buffer)
    {
        old_len = strlen(client->client_buffer);
    }
    char *new_buffer = malloc(old_len + len + 1);
    if (!new_buffer)
    {
        if (g_server_fd != -1)
        {
            close(g_server_fd);
            g_server_fd = -1;
        }
        free_clients(number_of_clients);
        fatal_error();
    }
    size_t i;
    for (i = 0; i < old_len; i++)
    {
        new_buffer[i] = client->client_buffer[i];
    }
    for (i = 0; i < (size_t)len; i++)
    {
        new_buffer[i + old_len] = data[i];
    }
    new_buffer[len + old_len] = '\0';
    if (client->client_buffer)
    {
        free(client->client_buffer);
    }
    client->client_buffer = new_buffer;
}

static void process_client_buffer(t_client *client)
{
    char *new_line_pos;
    char *line;
    char prefix[64];
    char *line_with_prefix;

    if (!client || !client->client_buffer)
    {
        return;
    }
    while ((new_line_pos = strstr(client->client_buffer, "\n")) != NULL)
    {
        size_t line_len = new_line_pos - client->client_buffer;
        size_t i;
        char *remaining;
        size_t remaining_len;

        line = malloc(line_len + 1);
        if (!line)
        {
            if (g_server_fd != -1)
            {
                close(g_server_fd);
                g_server_fd = -1;
            }
            free_clients(number_of_clients);
            fatal_error();
        }
        for (i = 0; i < line_len; i++)
        {
            line[i] = client->client_buffer[i];
        }
        line[line_len] = '\0';
        sprintf(prefix, "client %d: ", client->id);
        line_with_prefix = malloc(strlen(prefix) + line_len + 2);
        if (!line_with_prefix)
        {
            if (g_server_fd != -1)
            {
                close(g_server_fd);
                g_server_fd = -1;
            }
            free_clients(number_of_clients);
            fatal_error();
        }
        strcpy(line_with_prefix, prefix);
        strcat(line_with_prefix, line);
        strcat(line_with_prefix, "\n");
        broadcast(line_with_prefix, client->fd);
        free(line);
        free(line_with_prefix);
        remaining_len = strlen(client->client_buffer) - (line_len + 1);
        if (remaining_len == 0)
        {
            free(client->client_buffer);
            client->client_buffer = NULL;
            return;
        }
        remaining = malloc(remaining_len + 1);
        if (!remaining)
        {
            if (g_server_fd != -1)
            {
                close(g_server_fd);
                g_server_fd = -1;
            }
            free_clients(number_of_clients);
            fatal_error();
        }
        for (i = 0; i < remaining_len; i++)
        {
            remaining[i] = client->client_buffer[line_len + 1 + i];
        }
        remaining[remaining_len] = '\0';
        free(client->client_buffer);
        client->client_buffer = remaining;
    }
}

int main(int argc, char **argv)
{
    struct sockaddr_in serv_addr;
    fd_set readfds;
    int maxfd;
    t_client *it;
    int ret;

    if (argc != 2)
    {
        wrong_args();
    }
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0)
    {
        fatal_error();
    }
    signal(SIGINT, handle_sigint); // handle ctrl+c
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1 (localhost)
    serv_addr.sin_port = htons(atoi(argv[1]));
    if (bind(g_server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        close(g_server_fd);
        fatal_error();
    }
    if (listen(g_server_fd, SOMAXCONN) < 0) // SOMAXCONN is the maximum number of connections
    {
        close(g_server_fd);
        fatal_error();
    }
    while (!g_shutdown_requested)
    {
        FD_ZERO(&readfds);
        FD_SET(g_server_fd, &readfds);
        FD_SET(0, &readfds); // handle ctrl+d
        maxfd = g_server_fd;
        it = number_of_clients;
        while (it)
        {
            FD_SET(it->fd, &readfds);
            if (it->fd > maxfd)
            {
                maxfd = it->fd;
            }
            it = it->next;
        }
        ret = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0)
        {
            if (g_shutdown_requested)
            {
                break;
            }
            continue;
        }
        if (FD_ISSET(0, &readfds))
        {
            char one_char;
            int r = read(0, &one_char, 1);
            if (r == 0)
            {
                // ctrl+d on stdin
                g_shutdown_requested = 1;
                if (g_server_fd != -1)
                {
                    close(g_server_fd);
                    g_server_fd = -1;
                }
                break;
            }
        }
        if (FD_ISSET(g_server_fd, &readfds))
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_fd = accept(g_server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (new_fd >= 0)
            {
                t_client *client = new_client(new_fd);
                add_client(client);
                char msg[64];
                sprintf(msg, "server: client %d just arrived\n", client->id);
                broadcast(msg, new_fd);
            }
        }
        it = number_of_clients;
        while (it)
        {
            t_client *next = it->next;
            if (FD_ISSET(it->fd, &readfds))
            {
                char buffer[4096];
                int r = recv(it->fd, buffer, sizeof(buffer), 0);
                if (r <= 0)
                {
                    int last_id = it->id;
                    remove_client(it->fd);
                    char msg[64];
                    sprintf(msg, "server: client %d just left\n", last_id);
                    broadcast(msg, -1);
                }
                else
                {
                    append_to_buffer(it, buffer, r);
                    process_client_buffer(it);
                }
            }
            it = next;
        }
    }
    cleanup_and_exit(0);
}
