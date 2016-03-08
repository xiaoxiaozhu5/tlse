#include <stdio.h>
#include <sys/types.h>
#ifdef _WIN32
    #include <winsock2.h>
    #define socklen_t int
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h> 
#endif
#include "tlslayer.c"

// ================================================================================================= //
// this example ilustrates the libssl-almost-compatible interface                                    //
// tlslayer.c exports libssl compatibility APIs. Unlike tlslayer low-level apis, the SSL_* interface //
// is blocking! (or depending of the underling socket).                                              //
// ================================================================================================= //

// optional callback function for peer certificate verify
int verify(TLSContext *context, TLSCertificate **certificate_chain, int len) {
    int i;
    if (certificate_chain) {
        for (i = 0; i < len; i++) {
            TLSCertificate *certificate = certificate_chain[i];
            // check validity date
            int err = tls_certificate_is_valid(certificate);
            if (err)
                return err;
            // check certificate in certificate->bytes of length certificate->len
            // the certificate is in ASN.1 DER format
        }
    }
    //return certificate_expired;
    //return certificate_revoked;
    //return certificate_unknown;
    return no_error;
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int ret;
    char msg[] = "GET /\r\nHost: %s:%i\r\n\r\n";
    char msg_buffer[0xFF];
    char buffer[0xFFF];
    char *ref_argv[] = {"", "google.com", "443"};

#ifdef _WIN32
    // Windows: link against ws2_32.lib
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    // ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    // dummy functions ... for semantic compatibility only
    SSL_library_init();
    SSL_load_error_strings();
	
    // note that SSL and SSL_CTX are the same in tlslayer.c
    // both are mapped to TLSContext
    SSL *clientssl = SSL_CTX_new(SSLv3_client_method());
    // optionally, we can set a certificate validation callback function
    SSL_CTX_set_verify(clientssl, SSL_VERIFY_PEER, verify);
	
    if (!clientssl) {
        fprintf(stderr, "Error initializing client context\n");
        return -1;
    }
	
    if (argc < 3)
        argv = ref_argv;

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "ERROR opening socket");
        return -2;
    }
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        return -3;
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        fprintf(stderr, "ERROR connecting to %s:%s", argv[1], argv[2]);
        return -4;
    }
    snprintf(msg_buffer, sizeof(msg_buffer), msg, argv[1], portno);
    // starting from here is identical with libssl
    SSL_set_fd(clientssl, sockfd);

    if ((ret = SSL_connect(clientssl)) != 1) {
        fprintf(stderr, "Handshake Error %i\n", ret);
        return -5;
    }
    ret = SSL_write(clientssl, msg_buffer, strlen(msg_buffer));
    if (ret < 0) {
        fprintf(stderr, "SSL write error %i\n", ret);
        return -6;
    }
    while ((ret = SSL_read(clientssl, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, ret, 1, stdout);
    }
    if (ret < 0)
        fprintf(stderr, "SSL read error %i\n", ret);

    SSL_shutdown(clientssl);
#ifdef _WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
    SSL_CTX_free(clientssl);
    return 0;	
}
