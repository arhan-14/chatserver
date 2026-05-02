#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "chatd.h"
#include <ctype.h>

#define MAX_USERS 64

User users[MAX_USERS];
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

enum HeaderResult check_header(char *acc, int acc_len, int *header_len, int *body_len)
{
    if (acc_len < 2)
    { 
            return NEED_MORE;
    }
    if (acc[0] != '1')
    {
        return BAD_HEADER;
    }
    if (acc[1] != '|')
    {
        return BAD_HEADER;
    }
    if (acc_len < 6)
    {
        return NEED_MORE;
    }
    if (acc[5] != '|')
    {
        return BAD_HEADER;
    }

    char code[4] = {0};
    strncpy(code, acc + 2, 3);
    if (strcmp(code, "NAM") != 0 && strcmp(code, "SET") != 0 && strcmp(code, "MSG") != 0 && strcmp(code, "WHO") != 0)
    {
        return BAD_HEADER;
    }
    if (acc_len < 7)
    {
        return NEED_MORE;
    }

    int third_bar = -1;
    for (int i = 6; i < acc_len && i < 12; i++)
    {
        if (acc[i] == '|')
        {
            third_bar = i;
            break;
        }
    }

    if (third_bar == -1)
    {
        if (acc_len >= 12)
        {
            return BAD_HEADER;
        }
        return NEED_MORE;
    }

    for (int i = 6; i < third_bar; i++)
    {
        if (acc[i] < '0' || acc[i] > '9')
        {
            return BAD_HEADER;
        }
    }

    char len_str[6] = {0};
    strncpy(len_str, acc + 6, third_bar - 6);
    *body_len = atoi(len_str);
    *header_len = third_bar + 1;

    return HEADER_VALID;
}

int parse_message(char *buf, int len, int header_len, int body_len, Message *msg)
{
    memset(msg, 0, sizeof(*msg));
    msg->version = buf[0];
    strncpy(msg->code, buf + 2, 3);
    msg->code[3] = '\0'; 
    msg->body_len = body_len;
    char *body = buf +  header_len;

    if (body_len <= 0 || header_len + body_len > len)
    {
        return -1;
    }

    if (body[body_len - 1] != '|')
    {
        return -1;
    }

    if (strcmp(msg->code, "WHO") == 0 || strcmp(msg->code, "NAM") == 0 || strcmp(msg->code, "SET") == 0)
    {
        int field_len = body_len - 1;
        msg->field_len = field_len;
        if (strcmp(msg->code, "NAM") == 0)
        {
            if (field_len < 0) return -1;

            int copy_len = field_len;
            if (copy_len >= (int)sizeof(msg->sender))
            {
                copy_len = sizeof(msg->sender) - 1;
            }

            strncpy(msg->sender, body, copy_len);
            msg->sender[copy_len] = '\0';
        }
        else if (strcmp(msg->code, "SET") == 0)
        {
            if (field_len < 0) return -1;

            msg->field_len = field_len;

            int copy_len = field_len;
            if (copy_len >= (int)sizeof(msg->content))
            {
                copy_len = sizeof(msg->content) - 1;
            }

            strncpy(msg->content, body, copy_len);
            msg->content[copy_len] = '\0';
        }
        else
        {
            if (field_len < 0) return -1;

            int copy_len = field_len;
            if (copy_len >= (int)sizeof(msg->recipient))
            {
                copy_len = sizeof(msg->recipient) - 1;
            }

            strncpy(msg->recipient, body, copy_len);
            msg->recipient[copy_len] = '\0';
        }
    }

    else if (strcmp(msg->code, "MSG") == 0)
    {
        char *first_bar = memchr(body, '|', body_len);
        if (first_bar == NULL)
        {
            return -1;
        }
        int sender_len = first_bar - body;
        if (sender_len >= (int)sizeof(msg->sender))
        {
            return -1;
        }
        strncpy(msg->sender, body, sender_len);
        msg->sender[sender_len] = '\0';
    
        char *second_bar = memchr(first_bar + 1, '|', body_len - (first_bar - body) - 1);
        if (second_bar == NULL)
        {
            return -1;
        }

        int recipient_len = second_bar - first_bar - 1;
        if (recipient_len <= 0) return -1;

        int copy_len = recipient_len;
        if (copy_len >= (int)sizeof(msg->recipient))
        {
            copy_len = sizeof(msg->recipient) - 1;
        }

        strncpy(msg->recipient, first_bar + 1, copy_len);
        msg->recipient[copy_len] = '\0';

        int content_len = body_len - (second_bar - body) - 2;
        if (content_len <= 0)
        {
            return -1;
        }

        msg->content_len = content_len;

        int copy_len = content_len;
        if (copy_len >= (int)sizeof(msg->content))
        {
            copy_len = sizeof(msg->content) - 1;
        }

        strncpy(msg->content, second_bar + 1, copy_len);
        msg->content[copy_len] = '\0';
    }
    else
    {
        return -1;
    }

    return 0;
}

enum ValidationResult validate_message(Message *msg)
{
    if (strcmp(msg->code, "NAM") == 0)
    {
        if (strlen(msg->sender) < 1)
        {
            return ERR_UNREADABLE;
        }
        int name_len = msg->body_len - 1;

        if (name_len < 1)
        {
            return ERR_UNREADABLE;
        }
        if (name_len > 32)
        {
            return ERR_TOO_LONG;
        }
        for (size_t i = 0; i < strlen(msg->sender); i++)
        {
            char c = msg->sender[i];
            if (!isalpha((unsigned char)c) && !isdigit((unsigned char)c) && c != '-' && c != '_')
            {
                return ERR_ILLEGAL_CHAR;
            }
        }
        pthread_mutex_lock(&users_mutex);
        for (size_t i = 0; i < MAX_USERS; i++)
        {
            if (users[i].active && strcmp(users[i].name, msg->sender) == 0)
            {
                pthread_mutex_unlock(&users_mutex);
                return ERR_NAME_IN_USE;
            }
        }
        pthread_mutex_unlock(&users_mutex);
    }
    else if (strcmp(msg->code, "SET") == 0)
    {
        if (msg->field_len > 64)
        {
            return ERR_TOO_LONG;
        }
        for (size_t i = 0; i < strlen(msg->content); i++)
        {
            char c = msg->content[i];
            if (c < 32 || c > 126)
            {
                return ERR_ILLEGAL_CHAR;
            }
        }
    }
    else if (strcmp(msg->code, "MSG") == 0)
    {
        if (strlen(msg->content) < 1)
        {
            return ERR_UNREADABLE;
        }
        if (msg->content_len > 80)
        {
            return ERR_TOO_LONG;
        }
        for (size_t i = 0; i < strlen(msg->content); i++)
        {
            char c = msg->content[i];
            if (c < 32 || c > 126)
            {
                return ERR_ILLEGAL_CHAR;
            }
        }
        if (strcmp(msg->recipient, "#all") != 0)
        {
            pthread_mutex_lock(&users_mutex);
            int found = 0;
            for (size_t i = 0; i < MAX_USERS; i++)
            {
                if (users[i].active && strcmp(users[i].name, msg->recipient) == 0)
                {
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&users_mutex);
            if (!found)
            {
                return ERR_UNKNOWN_USER;
            }
        }
    }
    else if (strcmp(msg->code, "WHO") == 0)
    {
        if (strlen(msg->recipient) < 1)
        {
            return ERR_UNREADABLE;
        }
        if (strcmp(msg->recipient, "#all") != 0)
        {
            pthread_mutex_lock(&users_mutex);
            int found = 0;
            for (size_t i = 0; i < MAX_USERS; i++)
            {
                if (users[i].active && strcmp(users[i].name, msg->recipient) == 0)
                {
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&users_mutex);
            if (!found)
            {
                return ERR_UNKNOWN_USER;
            }
        }
    }
    return VALID;
}

void send_msg(int fd, char *sender, char *recipient, char *body)
{
    char buf[99999];
    int body_len = strlen(sender) + strlen(recipient) + strlen(body) + 3;
    snprintf(buf, sizeof(buf), "1|MSG|%d|%s|%s|%s|", body_len, sender, recipient, body);
    write(fd, buf, strlen(buf));
}

void send_err(int fd, int code, char *explanation)
{
    char buf[256];
    int body_len = snprintf(NULL, 0, "%d|%s|", code, explanation);
    snprintf(buf, sizeof(buf), "1|ERR|%d|%d|%s|", body_len, code, explanation);
    write(fd, buf, strlen(buf));
}

void broadcast(char *sender, char *recipient, char *body)
{
    pthread_mutex_lock(&users_mutex);
    for (size_t i = 0; i < MAX_USERS; i++)
    {
        if (users[i].active && users[i].has_name)
        {
            send_msg(users[i].fd, sender, recipient, body);
        }
    }
    pthread_mutex_unlock(&users_mutex);
}

int process_message(Message *msg, int client_fd, char *username, int *has_name)
{
    if (strcmp(msg->code, "NAM") != 0 && !*has_name)
    {
        return -1;
    }

    if (strcmp(msg->code, "NAM") == 0 && *has_name)
    {
        return -1;
    }
    if (strcmp(msg->code, "NAM") == 0)
    {
        pthread_mutex_lock(&users_mutex);
        int slot = -1;
        for (size_t i = 0; i < MAX_USERS; i++)
        {
            if (!users[i].active)
            {
                slot = i;
                break;
            }
        }
        if (slot == -1)
        {
            pthread_mutex_unlock(&users_mutex);
            return -1;
        }
        users[slot].fd = client_fd;
        users[slot].active = 1;
        users[slot].has_name = 1;
        strncpy(users[slot].name, msg->sender, sizeof(users[slot].name) - 1);
        users[slot].name[sizeof(users[slot].name) - 1] = '\0';
        users[slot].status[0] = '\0';
        pthread_mutex_unlock(&users_mutex);

        strncpy(username, msg->sender, 32);
        username[32] = '\0';
        *has_name = 1;

        send_msg(client_fd, "#all", username, "Welcome to the chat!");
    }
    else if (strcmp(msg->code, "SET") == 0)
    {
        pthread_mutex_lock(&users_mutex);
        for (size_t i = 0; i < MAX_USERS; i++)
        {
            if (users[i].active && users[i].fd == client_fd)
            {
                strncpy(users[i].status, msg->content, sizeof(users[i].status) - 1);
                users[i].status[sizeof(users[i].status) - 1] = '\0';
                break;
            }
        }
        pthread_mutex_unlock(&users_mutex);

        if (strlen(msg->content) > 0)
        {
            char broadcast_body[128];
            snprintf(broadcast_body, sizeof(broadcast_body),
                     "%s is now \"%s\"", username, msg->content);
            broadcast("#all", "#all", broadcast_body);
        }
    }
    else if (strcmp(msg->code, "MSG") == 0)
    {
        if (strcmp(msg->recipient, "#all") == 0)
        {
            broadcast(username, "#all", msg->content);
        }
        else
        {
            pthread_mutex_lock(&users_mutex);
            int recipient_fd = -1;
            for (size_t i = 0; i < MAX_USERS; i++)
            {
                if (users[i].active && strcmp(users[i].name, msg->recipient) == 0)
                {
                    recipient_fd = users[i].fd;
                    break;
                }
            }
            pthread_mutex_unlock(&users_mutex);

            if (recipient_fd == -1)
            {
                return -1;
            }
            send_msg(recipient_fd, username, msg->recipient, msg->content);
        }
    }
    else if (strcmp(msg->code, "WHO") == 0)
    {
        if (strcmp(msg->recipient, "#all") == 0)
        {
            char who_list[4096] = "";
            int offset = 0;

            pthread_mutex_lock(&users_mutex);
            for (size_t i = 0; i < MAX_USERS; i++)
            {
                if (users[i].active && users[i].has_name)
                {
                    int written;
                    if (strlen(users[i].status) > 0)
                    {
                        written = snprintf(who_list + offset, sizeof(who_list) - offset,
                                           "%s: %s\n", users[i].name, users[i].status);
                    }
                    else
                    {
                        written = snprintf(who_list + offset, sizeof(who_list) - offset,
                                           "%s\n", users[i].name);
                    }
                    if (written < 0)
                    {
                        break;
                    }

                    if (written >= (int)(sizeof(who_list) - offset))
                    {
                        offset = sizeof(who_list) - 1;
                        break;
                    }

                    offset += written;
                }
            }
            pthread_mutex_unlock(&users_mutex);

            
            if (offset > 0 && who_list[offset - 1] == '\n')
            {
                who_list[offset - 1] = '\0';
            }

            send_msg(client_fd, "#all", username, who_list);
        }
        else
        {
            char status_copy[65];
            status_copy[0] = '\0';

            pthread_mutex_lock(&users_mutex);
            for (size_t i = 0; i < MAX_USERS; i++)
            {
                if (users[i].active && strcmp(users[i].name, msg->recipient) == 0)
                {
                    strncpy(status_copy, users[i].status, sizeof(status_copy) - 1);
                    status_copy[sizeof(status_copy) - 1] = '\0';
                    break;
                }
            }
            pthread_mutex_unlock(&users_mutex);

            char body[128] = "";

            if (strlen(status_copy) == 0)
            {
                send_msg(client_fd, "#all", username, "No status");
            }
            else
            {
                snprintf(body, sizeof(body), "%s: %s", msg->recipient, status_copy);
                send_msg(client_fd, "#all", username, body);            
            }
        }
    }
    else
    {
        return -1;
    }

    return 0;
}

void cleanup_user(int client_fd)
{
    if (client_fd < 0) return;
    pthread_mutex_lock(&users_mutex);
    for (size_t i = 0; i < MAX_USERS; i++)
    {
        if (users[i].active && users[i].fd == client_fd)
        {
            users[i].active   = 0;
            users[i].has_name = 0;
            users[i].name[0]  = '\0';
            users[i].status[0] = '\0';
            users[i].fd       = -1;
            break;
        }
    }
    pthread_mutex_unlock(&users_mutex);
}

void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char acc[4096];
    int acc_len = 0;
    char temp[1024];
    char username[33] = "";
    int has_name = 0;

    while (1)
    {
        ssize_t bytes = read(client_fd, temp, sizeof(temp));
        if (bytes == 0)
        {
            cleanup_user(client_fd);
            break;
        }
        if (bytes < 0)
        {
            cleanup_user(client_fd);
            break;
        }

        if (acc_len + bytes > (int)sizeof(acc))
        {
            send_err(client_fd, ERR_UNREADABLE, "Unreadable");
            cleanup_user(client_fd);
            close(client_fd);
            return NULL;
        }

        memcpy(acc + acc_len, temp, bytes);
        acc_len += bytes;

        while (1)
        {
            int header_len = 0;
            int body_len = 0;
            enum HeaderResult hr = check_header(acc, acc_len, &header_len, &body_len);

            if (hr == NEED_MORE)
            {
                break;
            }
            if (hr == BAD_HEADER)
            {
                send_err(client_fd, ERR_UNREADABLE, "Unreadable");
                cleanup_user(client_fd);
                close(client_fd);
                return NULL;
            }
            if (body_len < 0 || header_len + body_len > (int)sizeof(acc))
            {
                send_err(client_fd, ERR_UNREADABLE, "Unreadable");
                cleanup_user(client_fd);
                close(client_fd);
                return NULL;
            }
            if (acc_len < header_len + body_len)
            {
                break;
            }

            Message msg;
            if (parse_message(acc, acc_len, header_len, body_len, &msg) != 0)
            {
                send_err(client_fd, ERR_UNREADABLE, "Unreadable");
                cleanup_user(client_fd);
                close(client_fd);
                return NULL;
            }

            enum ValidationResult vr = validate_message(&msg);
            if (vr != VALID)
            {
                char *explanation;
                switch (vr)
                {
                    case ERR_NAME_IN_USE:   explanation = "Name in use";         break;
                    case ERR_UNKNOWN_USER:  explanation = "Unknown recipient";   break;
                    case ERR_ILLEGAL_CHAR:  explanation = "Illegal character";   break;
                    case ERR_TOO_LONG:      explanation = "Too long";            break;
                    default:                explanation = "Unreadable";          break;
                }
                send_err(client_fd, vr, explanation);

                if (vr == ERR_UNREADABLE)
                {
                    cleanup_user(client_fd);
                    close(client_fd);
                    return NULL;
                }

                int msg_len = header_len + body_len;
                memmove(acc, acc + msg_len, acc_len - msg_len);
                acc_len -= msg_len;
                continue;
            }

            if (process_message(&msg, client_fd, username, &has_name) != 0)
            {
                send_err(client_fd, ERR_UNREADABLE, "Unreadable");
                cleanup_user(client_fd);
                close(client_fd);
                return NULL;
            }

            int msg_len = header_len + body_len;
            memmove(acc, acc + msg_len, acc_len - msg_len);
            acc_len -= msg_len;
        }
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s PORT\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Usage: %s PORT\n", argv[0]);
        return 1;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(listen_fd);
        return 1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) < 0)
    {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        int *fd_ptr = malloc(sizeof(int));
        if (fd_ptr == NULL)
        {
            fprintf(stderr, "malloc failed\n");
            close(client_fd);
            continue;
        }
        *fd_ptr = client_fd;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, fd_ptr) != 0)
        {
            fprintf(stderr, "pthread_create failed\n");
            close(client_fd);
            free(fd_ptr);
            continue;
        }

        pthread_detach(thread);
    }

    close(listen_fd);
    return 0;
}
