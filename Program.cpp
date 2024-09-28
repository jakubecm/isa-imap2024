// OpenSSL headers
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <iostream>
#include "ArgumentParser.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>

int main(int argc, char *argv[])
{
    ArgumentParser parser(argc, argv);
    ArgumentParser::ParsedArgs args = parser.parse();

    // Print out parsed arguments
    std::cout << "Server: " << args.server << std::endl;
    std::cout << "Port: " << (args.port ? args.port : (args.use_tls ? 993 : 143)) << std::endl;
    std::cout << "Použít TLS: " << (args.use_tls ? "Ano" : "Ne") << std::endl;
    std::cout << "Certifikát: " << args.certfile << std::endl;
    std::cout << "Adresář certifikátů: " << args.certaddr << std::endl;
    std::cout << "Nové zprávy: " << (args.new_only ? "Ano" : "Ne") << std::endl;
    std::cout << "Hlavičky: " << (args.headers_only ? "Ano" : "Ne") << std::endl;
    std::cout << "Autentizační soubor: " << args.authfile << std::endl;
    std::cout << "Schránka: " << args.mailbox << std::endl;
    std::cout << "Výstupní adresář: " << args.outdir << std::endl;

    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    SSL_library_init();

    if (args.use_tls)
    {
        SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
        if (!ctx)
        {
            ERR_print_errors_fp(stderr);
            return 1;
        }

        if (!args.certfile.empty())
        {
            if (!SSL_CTX_load_verify_locations(ctx, args.certfile.c_str(), args.certaddr.c_str()))
            {
                ERR_print_errors_fp(stderr);
                SSL_CTX_free(ctx);
                return 1;
            }
        }
        else
        {
            // If no certificate file is specified, use the default system certificate store
            if (SSL_CTX_load_verify_locations(ctx, NULL, args.certaddr.c_str()) != 1)
            {
                ERR_print_errors_fp(stderr);
                SSL_CTX_free(ctx);
                return 1;
            }
        }

        BIO *bio = BIO_new_ssl_connect(ctx);

        if (!bio)
        {
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            return 1;
        }

        std::string server_port = args.server + ":" + std::to_string(args.port);
        BIO_set_conn_hostname(bio, server_port.c_str());

        if (BIO_do_connect(bio) <= 0)
        {
            ERR_print_errors_fp(stderr);
            BIO_free_all(bio);
            SSL_CTX_free(ctx);
            return 1;
        }

        char buffer[5024];
        std::cout << "Successfuly connected to the server with TLS." << args.server << std::endl;
        int bytes_read = BIO_read(bio, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0'; // Add a null terminator for correct output
            std::cout << buffer << std::endl;
        }

        std::string logout_command = "A002 LOGOUT\r\n";
        BIO_write(bio, logout_command.c_str(), logout_command.size());

        // Reading BYE response
        bytes_read = BIO_read(bio, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0'; // Add a null terminator for correct output
            std::cout << "Server response to LOGOUT:\n"
                      << buffer << std::endl;
        }

        SSL *ssl = NULL;
        BIO_get_ssl(bio, &ssl);

        SSL_shutdown(ssl);
        SSL_CTX_free(ctx);

        BIO_free_all(bio);
    }
    else
    {
        // Non secure connection
        BIO *bio = BIO_new_connect((args.server + ":" + std::to_string(args.port)).c_str());
        if (!bio)
        {
            ERR_print_errors_fp(stderr);
            return 1;
        }

        // Navázání nešifrovaného spojení
        if (BIO_do_connect(bio) <= 0)
        {
            ERR_print_errors_fp(stderr);
            BIO_free_all(bio);
            return 1;
        }

        std::cout << "Sucessful non-secure connection estabilished." << args.server << std::endl;

        // ... communication ...

        // Freeing resources
        BIO_free_all(bio);
    }

    return 0;
}
