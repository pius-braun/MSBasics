#define _WIN32_WINNT 0x0A00

// #define _DEBUG

#include <cstdio>
#include <sys/types.h>
#include <microhttpd.h>
#include "sometools.h"
#include "somesock.h"
#include "websocket_client.h"

#ifdef TARGET_UX
#include <cstring>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#endif

#define RESETPORT 8081
#define SERVERPORT 8082
#define WEBSOCKETPORT 8083
#define WEBSOCKETSERVICE "servicepython"

#define MAX_BUF_SIZE 512
#define MAX_KEY_SIZE 128

struct con_data
{
    char data_in[MAX_BUF_SIZE];
    char data_out[MAX_BUF_SIZE];
    unsigned int pos_in;
    unsigned int http_response_code;
};

const char *htmlerror_invalid_URI = "<html><body>Error: invalid URI: use '/api/v1/' to access REST interface</body></html>";
const char *resetcmd_header = "POST /api/v1/ HTTP/1.1\r\nConnection: Keep-Alive\r\nContent-Type: application/json\r\nAccept: */*\r\nContent-Length: %d\r\n\r\n";
const char *resetcmd_body = "{\"meta\": {\"action\": \"reset\"}}";
const char *jsonresponse[8] =
    {"{\"data\": {\"id\": \"1\",\"type\": \"service\",\"attributes\": {\"name\": \"servicejava\",\"timestamp\":\"%s\", \"counter\":\"%s\"}}}",
     "{\"errors\": {\"code\": \"100\",\"status\": \"405\",\"source\": null,\"title\": \"Not allowed\",\"details\":\"HTTP method not allowed here: %s\"}}",
     "{\"errors\": {\"code\": \"101\",\"status\": \"400\",\"source\": \"data.attributes.name\",\"title\": \"Bad request\",\"details\":\"Attribute 'name' empty or unknown\"}}",
     "{\"errors\": {\"code\": \"102\",\"status\": \"400\",\"source\": \"meta.action\",\"title\": \"Bad request\",\"details\":\"Attribute 'meta.action' missing or invalid\"}}",
     "{\"errors\": {\"code\": \"103\",\"status\": \"404\",\"source\": \"data.attributes.name\",\"title\": \"Not found\",\"details\":\"Service did not respond to reset\"}}",
     "{\"errors\": {\"code\": \"104\",\"status\": \"404\",\"source\": null,\"title\": \"Not found\",\"details\":\"Invalid URI: use '/api/v1/' to access REST interface\"}}",
     "{\"errors\": {\"code\": \"105\",\"status\": \"413\",\"source\": null,\"title\": \"Payload too large\",\"details\":\"Payload exceeds buffer size\"}}",
     "{\"errors\": {\"code\": \"106\",\"status\": \"400\",\"source\": null,\"title\": \"Bad request\",\"details\":\"Invalid json format\"}}"};

/* Handle requests on "/api/v1/"
   RESTful implementation according to http://jsonapi.org
   Document structure:
    { 
      "data" :
      {
        "id":
        "type": "service",
        "attributes": {
          "name": "<service name>",
          "timestamp": "<timesstamp>",
          "counter": "<last counter value>"
        }
      },
      "meta" : {
        "action" : "reset"
      }
    }
    {
      "errors": [
      {
        "code": "100",
        "status": "405",
        "source": null,
        "title": "Not allowed",
        "details": "HTTP method not allowed here: <method name>"
      },
      {
        "code": "101",
        "status": "400",
        "source": {"pointer": "data.attributes.name"},
        "title": "Bad request",
        "details": "Attribute 'name' must not be empty"
      },
      {
        "code": "102",
        "status": "400",
        "source": {"pointer": "meta.action"},
        "title": "Bad request",
        "details": "Attribute 'meta.action' missing or invalid"
      },
      {
        "code": "103",
        "status": "404",
        "source": null,
        "title": "Service not found",
        "details": "Service did not respond: <service name>"        
      },
        "code": "104",
        "status": "404",
        "source": null,
        "title": "Not found",
        "details":"Invalid URI: use '/api/v1/' to access REST interface"
      },
        "code": "105",
        "status": "413","
        source": null,
        "title": "Payload too large",
        "details":"Payload exceeds buffer size"
      },
        code": "106",
        "status": "400",
        "source": null,
        "title": "Bad request",
        "details":"Invalid json format"
      }]
    }
    Using HTTP 1.1 PUT to /api/v1/ with content type application/json
    only accepting: "action": "reset" on service <service name>
    returning: "data" "attribute"  <timestamp> on successful reset
    returning: "errors": "Not allowed", "Bad request" etc."
*/

int send_json(struct MHD_Connection *connection, const char *json, int http_code)
{
    int ret;
    MHD_Response *response;
    response = MHD_create_response_from_buffer(strlen(json), (void *)json, MHD_RESPMEM_PERSISTENT);
    if (!response)
    {
#ifdef _DEBUG
        printf("(MSBasics-ServiceC) Error allocating response\n");
#endif
        return MHD_NO;
    }
    MHD_add_response_header(response, "Content-Type", "application/json");
    ret = MHD_queue_response(connection, http_code, response);
    if (ret != MHD_YES)
    {
#ifdef _DEBUG
        printf("(MSBasics-ServiceC) Error queue response\n");
#endif
    }
    MHD_destroy_response(response);
    return ret;
}

static void request_completed(void *cls, struct MHD_Connection *connection,
                              void **con_cls, enum MHD_RequestTerminationCode toe)
{
    if (*con_cls == NULL)
        return;
    con_data *cd = (con_data *)*con_cls;
    free(cd);
    *con_cls = NULL;
}

// condense chunked body to beginning of buffer
int collectchunks(char *buf)
{

    // (1) find start of body
    char *body = strstr(buf, "\r\n\r\n");
    if (body == NULL)
        return 0;

    char *target = buf;
    body += 4;
    char *body2;
    long chunksize = -1;
    while (chunksize != 0)
    {
        // (2) extract chunk size (hex number)
        body2 = strstr(body, "\r\n");
        if (body2 == NULL)
            return 0;
        *body2 = 0;
        body2 += 2;
        errno = 0;
        chunksize = strtol(body, NULL, 16);
        if ((errno != 0) || (chunksize > strlen(body2)))
            return 0;
        // (3) copy chunk
        strncpy(target, body2, (size_t)chunksize);
        target = target + chunksize;
        body = body2 + chunksize;
    }
    *target = 0;
    return 1;
}

// Call  servicejava to perform reset, using POST with "meta/reset"
// counter: latest counter value before reset or empty on error
// returns: 1 on success, 0 otherwise
int trigger_reset(char *counter, const char *servicename, int port)
{
    SOCKET sock;

    if (!sock_connect(servicename, port, sock))
        return 0;

    char *buf = (char *)malloc(MAX_BUF_SIZE);
    sprintf(buf, resetcmd_header, strlen(resetcmd_body));
    strcat(buf, resetcmd_body);
    if (!sock_send(sock, buf, strlen(buf)))
    {
        free(buf);
        return 0;
    }

    if (!sock_shutdown_send(sock))
    {
        free(buf);
        return 0;
    }

    int bpos = 0;
    int iresult;
    do
    {
        iresult = sock_recv(sock, buf + bpos, MAX_BUF_SIZE - bpos);
        if (iresult > MAX_BUF_SIZE - bpos)
        {
#ifdef _DEBUG
            printf("(MSBasics-ServiceC) Error buffer too small\n");
#endif
            iresult = -1;
        }
        else if (iresult > 0)
        {
            bpos = bpos + iresult;
        }
    } while (iresult > 0);
    sock_close(sock);

    if (iresult != 0)
    {
#ifdef _DEBUG
        printf("(MSBasics-ServiceC) Error: receiving data failed\n");
#endif
        free(buf);
        return 0;
    }
    *(buf + bpos) = 0;
#ifdef _DEBUG
    printf("(MSBasics-ServiceC) received %d bytes:\n", strlen(buf));
    for (bpos = 0; bpos < strlen(buf); bpos++)
    {
        if (*(buf + bpos) == '\r')
            printf("<CR>");
        else if (*(buf + bpos) == '\n')
            printf("<LF>\n");
        else if (*(buf + bpos) < 32)
            printf("<%d>", *(buf + bpos));
        else
            printf("%c", *(buf + bpos));
    }
    printf("\n");
#endif
    /* Expected message: 
    HTTP/1.1 200 OK<CR><LF>
    Date: Mon, 14 Dec 2020 12:01:49 GMT<CR><LF>
    Transfer-encoding: chunked<CR><LF>
    Content-type: application/json<CR><LF>
    Content-length: 40<CR><LF>
    <CR><LF>
    28<CR><LF>
    {"data":{"type":"counter","value":2156}}<CR><LF>
    0<CR><LF>
    <CR><LF>
  */
    // (1) find "200 OK"
    if (strstr(buf, "200 OK") == NULL)
    {
#ifdef _DEBUG
        printf("(MSBasics-ServiceC) Error: reset failed\n");
#endif
        free(buf);
        return 0;
    }

    // (2) Extract chunked body
    if (!collectchunks(buf))
    {
#ifdef _DEBUG
        printf("(MSBasics-ServiceC) Error: invalid body part\n");
#endif
        free(buf);
        return 0;
    }

    // (3) copy counter value
    bpos = MAX_KEY_SIZE;
    if (json_get_object(buf, "/data/value", counter, bpos) <= 0)
    {
#ifdef _DEBUG
        printf("(MSBasics-ServiceC) Error: invalid body part\n");
#endif
        strcpy(counter, "ERR");
        free(buf);
        return 0;
    }
    free(buf);
    return 1;
}

const char *ws_payload = "{\"data\": {\"timestamp\":\"%s\", \"counter\":\"%s\"}}";


// Connect to the specified servicename to inform about the latest timestamp and counter value, using WebSocket-protocol
// this is the client side of the Web socket
// counter: latest counter value
// timestamp: latest timestamp
// servicename: name  or ip-address of service to send data to (servicepython)
// returns: 1 on success, 0 otherwise

int inform_reset(const char *counter, const char *timestamp, const char *servicename, int serviceport)
{
    SOCKET sock;

    if (!ws_handshake(servicename, serviceport, sock))
        return 0;

    char* buf = (char*)malloc(MAX_BUF_SIZE);
    sprintf(buf, ws_payload, timestamp, counter);
    if (!ws_sendpayload(sock, buf)) {
        free(buf);
        ws_send_close(sock, 1008);
        sock_close(sock);
        return 0;
    }

    free(buf);

    if (!ws_recv_close(sock)) {
        ws_send_close(sock, 1008);
        sock_close(sock);
        return(0);
    }

    sock_close(sock);
    return 1;
}


int answer_to_connection(void *cls, MHD_Connection *connection,
                         const char *url,
                         const char *method, const char *version,
                         const char *upload_data,
                         size_t *upload_data_size, void **con_cls)
{
    int ret;

    if (*con_cls == NULL)
    {
        con_data *cd = (con_data *)malloc(sizeof(con_data));
        if (cd == NULL)
        {
#ifdef _DEBUG
            printf("(MSBasics-ServiceC) Error allocating connection buffer\n");
#endif
            return MHD_NO;
        }
        cd->data_in[0] = 0;
        cd->pos_in = 0;
        *con_cls = cd;
        return MHD_YES;
    }
    if (strcmp(method, MHD_HTTP_METHOD_PUT) == 0)
    {
        con_data *cd = (con_data *)*con_cls;
        if (*upload_data_size != 0)
        {
            if ((cd->pos_in + *upload_data_size) >= MAX_BUF_SIZE)
            {
                strcpy(cd->data_out, jsonresponse[6]); // Payload too large
                cd->http_response_code = MHD_HTTP_PAYLOAD_TOO_LARGE;
            }
            else
            {
                strncpy((char *)&cd->data_in[cd->pos_in], upload_data, *upload_data_size);
                cd->pos_in += *upload_data_size;
                cd->data_in[cd->pos_in] = 0;
                if (json_complete(cd->data_in))
                {
                    char buf[MAX_KEY_SIZE];
                    int len = 32;
                    char servicename[32];
                    int jret = json_get_object(cd->data_in, "/data/attributes/name", servicename, len);
                    switch (jret)
                    {
                    case -2:
                    { // buffer too small->invalid json format
                        strcpy(cd->data_out, jsonresponse[7]);
                        cd->http_response_code = MHD_HTTP_BAD_REQUEST;
                        break;
                    }
                    case -1:
                    { // invalid json format
                        strcpy(cd->data_out, jsonresponse[7]);
                        cd->http_response_code = MHD_HTTP_BAD_REQUEST;
                        break;
                    }
                    case 0:
                    { // key not found
                        strcpy(cd->data_out, jsonresponse[2]);
                        cd->http_response_code = MHD_HTTP_BAD_REQUEST;
                        break;
                    }
                    default:
                    {
                        jret = json_get_object(cd->data_in, "/meta/action", buf, len);
                        if ((jret <= 0) || (strcmp(buf, "reset") != 0))
                        {
                            strcpy(cd->data_out, jsonresponse[3]);
                            cd->http_response_code = MHD_HTTP_BAD_REQUEST;
                        }
                        else
                        {
                            if (!trigger_reset(buf, servicename, RESETPORT))
                            {
                                strcpy(cd->data_out, jsonresponse[4]);
                                cd->http_response_code = MHD_HTTP_BAD_REQUEST;
                            }
                            else
                            {
                                sprintf(cd->data_out, jsonresponse[0], timestamp(), buf);
                                cd->http_response_code = MHD_HTTP_OK;
                                inform_reset(buf, timestamp(), WEBSOCKETSERVICE, WEBSOCKETPORT);
                            }
                        }
                    }
                    }
                }
            }
            *upload_data_size = 0;
            return MHD_YES;
        }
        else
        {
            if (strncmp(url, "/api/v1/", 8) != 0)
            {
                strcpy(cd->data_out, jsonresponse[5]); // invalid URI
                cd->http_response_code = MHD_HTTP_NOT_FOUND;
            }
            else if (!json_complete(cd->data_in))
            {
                strcpy(cd->data_out, jsonresponse[7]); // bad json format
                cd->http_response_code = MHD_HTTP_BAD_REQUEST;
            }
            return send_json(connection, cd->data_out, cd->http_response_code);
        }
    }
    return MHD_NO;
}

int main()
{
    MHD_Daemon *daemon;
    daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG, SERVERPORT, NULL, NULL,
                              (MHD_AccessHandlerCallback)answer_to_connection, NULL, MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL, MHD_OPTION_END);
    if (daemon == NULL)
    {
        printf("(MSBasics-ServiceC) Error starting ServiceC\n");
        return 1;
    }

    printf("(MSBasics-ServiceC) Service started\n");
    getchar();
    MHD_stop_daemon(daemon);
    printf("(MSBasics-ServiceC) Service terminated\n");
    return 0;
}
